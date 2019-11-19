// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/native_desktop_media_list.h"

#include <utility>

#include "base/bind.h"
#include "base/hash/hash.h"
#include "base/message_loop/message_pump_type.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/scale_argb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/snapshot/snapshot.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/snapshot/snapshot_aura.h"
#endif

using content::BrowserThread;
using content::DesktopMediaID;

namespace {

// Update the list every second.
const int kDefaultNativeDesktopMediaListUpdatePeriod = 1000;

// Returns a hash of a DesktopFrame content to detect when image for a desktop
// media source has changed.
uint32_t GetFrameHash(webrtc::DesktopFrame* frame) {
  int data_size = frame->stride() * frame->size().height();
  return base::Hash(frame->data(), data_size);
}

gfx::ImageSkia ScaleDesktopFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                                 gfx::Size size) {
  gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(0, 0, size.width(), size.height()),
      gfx::Size(frame->size().width(), frame->size().height()));

  SkBitmap result;
  result.allocN32Pixels(scaled_rect.width(), scaled_rect.height(), true);

  uint8_t* pixels_data = reinterpret_cast<uint8_t*>(result.getPixels());
  libyuv::ARGBScale(frame->data(), frame->stride(), frame->size().width(),
                    frame->size().height(), pixels_data, result.rowBytes(),
                    scaled_rect.width(), scaled_rect.height(),
                    libyuv::kFilterBilinear);

  // Set alpha channel values to 255 for all pixels.
  // TODO(sergeyu): Fix screen/window capturers to capture alpha channel and
  // remove this code. Currently screen/window capturers (at least some
  // implementations) only capture R, G and B channels and set Alpha to 0.
  // crbug.com/264424
  for (int y = 0; y < result.height(); ++y) {
    for (int x = 0; x < result.width(); ++x) {
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 3] =
          0xff;
    }
  }

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}

}  // namespace

class NativeDesktopMediaList::Worker
    : public webrtc::DesktopCapturer::Callback {
 public:
  Worker(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
         base::WeakPtr<NativeDesktopMediaList> media_list,
         DesktopMediaID::Type type,
         std::unique_ptr<webrtc::DesktopCapturer> capturer);
  ~Worker() override;

  void Start();
  void Refresh(const DesktopMediaID::Id& view_dialog_id, bool update_thumnails);

  void RefreshThumbnails(const std::vector<DesktopMediaID>& native_ids,
                         const gfx::Size& thumbnail_size);

 private:
  typedef std::map<DesktopMediaID, uint32_t> ImageHashesMap;

  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // Task runner used for capturing operations.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtr<NativeDesktopMediaList> media_list_;

  DesktopMediaID::Type type_;
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;

  std::unique_ptr<webrtc::DesktopFrame> current_frame_;

  ImageHashesMap image_hashes_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

NativeDesktopMediaList::Worker::Worker(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<NativeDesktopMediaList> media_list,
    DesktopMediaID::Type type,
    std::unique_ptr<webrtc::DesktopCapturer> capturer)
    : task_runner_(task_runner),
      media_list_(media_list),
      type_(type),
      capturer_(std::move(capturer)) {}

NativeDesktopMediaList::Worker::~Worker() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void NativeDesktopMediaList::Worker::Start() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  capturer_->Start(this);
}

void NativeDesktopMediaList::Worker::Refresh(
    const DesktopMediaID::Id& view_dialog_id,
    bool update_thumnails) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  std::vector<SourceDescription> result;

  webrtc::DesktopCapturer::SourceList sources;
  if (!capturer_->GetSourceList(&sources)) {
    // Will pass empty results list to RefreshForAuraWindows().
    sources.clear();
  }

  bool mutiple_sources = sources.size() > 1;
  base::string16 title;
  for (size_t i = 0; i < sources.size(); ++i) {
    switch (type_) {
      case DesktopMediaID::TYPE_SCREEN:
        // Just in case 'Screen' is inflected depending on the screen number,
        // use plural formatter.
        title = mutiple_sources
                    ? l10n_util::GetPluralStringFUTF16(
                          IDS_DESKTOP_MEDIA_PICKER_MULTIPLE_SCREEN_NAME,
                          static_cast<int>(i + 1))
                    : l10n_util::GetStringUTF16(
                          IDS_DESKTOP_MEDIA_PICKER_SINGLE_SCREEN_NAME);
        break;

      case DesktopMediaID::TYPE_WINDOW:
        // Skip the picker dialog window.
        if (sources[i].id == view_dialog_id)
          continue;
        title = base::UTF8ToUTF16(sources[i].title);
        break;

      default:
        NOTREACHED();
    }
    result.push_back(
        SourceDescription(DesktopMediaID(type_, sources[i].id), title));
  }

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&NativeDesktopMediaList::RefreshForAuraWindows,
                                media_list_, result, update_thumnails));
}

void NativeDesktopMediaList::Worker::RefreshThumbnails(
    const std::vector<DesktopMediaID>& native_ids,
    const gfx::Size& thumbnail_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  ImageHashesMap new_image_hashes;

  // Get a thumbnail for each native source.
  for (const auto& id : native_ids) {
    if (!capturer_->SelectSource(id.id))
      continue;
    capturer_->CaptureFrame();

    // Expect that DesktopCapturer to always captures frames synchronously.
    // |current_frame_| may be NULL if capture failed (e.g. because window has
    // been closed).
    if (current_frame_) {
      uint32_t frame_hash = GetFrameHash(current_frame_.get());
      new_image_hashes[id] = frame_hash;

      // Scale the image only if it has changed.
      auto it = image_hashes_.find(id);
      if (it == image_hashes_.end() || it->second != frame_hash) {
        gfx::ImageSkia thumbnail =
            ScaleDesktopFrame(std::move(current_frame_), thumbnail_size);
        base::PostTask(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(&NativeDesktopMediaList::UpdateSourceThumbnail,
                           media_list_, id, thumbnail));
      }
    }
  }

  image_hashes_.swap(new_image_hashes);

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&NativeDesktopMediaList::UpdateNativeThumbnailsFinished,
                     media_list_));
}

void NativeDesktopMediaList::Worker::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  current_frame_ = std::move(frame);
}

NativeDesktopMediaList::NativeDesktopMediaList(
    DesktopMediaID::Type type,
    std::unique_ptr<webrtc::DesktopCapturer> capturer)
    : DesktopMediaListBase(base::TimeDelta::FromMilliseconds(
          kDefaultNativeDesktopMediaListUpdatePeriod)),
      thread_("DesktopMediaListCaptureThread") {
  type_ = type;

#if defined(OS_WIN) || defined(OS_MACOSX)
  // On Windows/OSX the thread must be a UI thread.
  base::MessagePumpType thread_type = base::MessagePumpType::UI;
#else
  base::MessagePumpType thread_type = base::MessagePumpType::DEFAULT;
#endif
  thread_.StartWithOptions(base::Thread::Options(thread_type, 0));

  worker_.reset(new Worker(thread_.task_runner(), weak_factory_.GetWeakPtr(),
                           type, std::move(capturer)));

  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Worker::Start, base::Unretained(worker_.get())));
}

NativeDesktopMediaList::~NativeDesktopMediaList() {
  // This thread should mostly be an idle observer. Stopping it should be fast.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
  thread_.task_runner()->DeleteSoon(FROM_HERE, worker_.release());
  thread_.Stop();
}

void NativeDesktopMediaList::Refresh(bool update_thumnails) {
  DCHECK(can_refresh());

#if defined(USE_AURA)
  DCHECK_EQ(pending_aura_capture_requests_, 0);
  DCHECK(!pending_native_thumbnail_capture_);
  new_aura_thumbnail_hashes_.clear();
#endif

  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Worker::Refresh, base::Unretained(worker_.get()),
                     view_dialog_id_.id, update_thumnails));
}

void NativeDesktopMediaList::RefreshForAuraWindows(
    std::vector<SourceDescription> sources,
    bool update_thumnails) {
  DCHECK(can_refresh());

#if defined(USE_AURA)
  // Associate aura id with native id.
  for (auto& source : sources) {
    if (source.id.type != DesktopMediaID::TYPE_WINDOW)
      continue;

    aura::WindowTreeHost* const host =
        aura::WindowTreeHost::GetForAcceleratedWidget(
            *reinterpret_cast<gfx::AcceleratedWidget*>(&source.id.id));
    aura::Window* const aura_window = host ? host->window() : nullptr;
    if (aura_window) {
      DesktopMediaID aura_id = DesktopMediaID::RegisterNativeWindow(
          DesktopMediaID::TYPE_WINDOW, aura_window);
      source.id.window_id = aura_id.window_id;
    }
  }
#endif  // defined(USE_AURA)

  UpdateSourcesList(sources);

  if (!update_thumnails) {
    OnRefreshComplete();
    return;
  }

  if (thumbnail_size_.IsEmpty()) {
#if defined(USE_AURA)
    pending_native_thumbnail_capture_ = true;
#endif
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NativeDesktopMediaList::UpdateNativeThumbnailsFinished,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // OnAuraThumbnailCaptured() and UpdateNativeThumbnailsFinished() are
  // guaranteed to be executed after RefreshForAuraWindows() and
  // CaptureAuraWindowThumbnail() in the browser UI thread.
  // Therefore pending_aura_capture_requests_ will be set the number of aura
  // windows to be captured and pending_native_thumbnail_capture_ will be set
  // true if native thumbnail capture is needed before OnAuraThumbnailCaptured()
  // or UpdateNativeThumbnailsFinished() are called.
  std::vector<DesktopMediaID> native_ids;
  for (const auto& source : sources) {
#if defined(USE_AURA)
    if (source.id.window_id > DesktopMediaID::kNullId) {
      CaptureAuraWindowThumbnail(source.id);
      continue;
    }
#endif  // defined(USE_AURA)
    native_ids.push_back(source.id);
  }

  if (!native_ids.empty()) {
#if defined(USE_AURA)
    pending_native_thumbnail_capture_ = true;
#endif
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&Worker::RefreshThumbnails,
                                  base::Unretained(worker_.get()), native_ids,
                                  thumbnail_size_));
  }
}

void NativeDesktopMediaList::UpdateNativeThumbnailsFinished() {
#if defined(USE_AURA)
  DCHECK(pending_native_thumbnail_capture_);
  pending_native_thumbnail_capture_ = false;
  // If native thumbnail captures finished after aura thumbnail captures,
  // execute |done_callback| to let the caller know the update process is
  // finished.  If necessary, this will schedule the next refresh.
  if (pending_aura_capture_requests_ == 0)
    OnRefreshComplete();
#else
  OnRefreshComplete();
#endif  // defined(USE_AURA)
}

#if defined(USE_AURA)

void NativeDesktopMediaList::CaptureAuraWindowThumbnail(
    const DesktopMediaID& id) {
  DCHECK(can_refresh());

  gfx::NativeWindow window = DesktopMediaID::GetNativeWindowById(id);
  if (!window)
    return;

  gfx::Rect window_rect(window->bounds().width(), window->bounds().height());
  gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(thumbnail_size_), window_rect.size());

  pending_aura_capture_requests_++;
  ui::GrabWindowSnapshotAndScaleAsyncAura(
      window, window_rect, scaled_rect.size(),
      base::Bind(&NativeDesktopMediaList::OnAuraThumbnailCaptured,
                 weak_factory_.GetWeakPtr(), id));
}

void NativeDesktopMediaList::OnAuraThumbnailCaptured(const DesktopMediaID& id,
                                                     gfx::Image image) {
  DCHECK(can_refresh());

  if (!image.IsEmpty()) {
    // Only new or changed thumbnail need update.
    new_aura_thumbnail_hashes_[id] = GetImageHash(image);
    if (!previous_aura_thumbnail_hashes_.count(id) ||
        previous_aura_thumbnail_hashes_[id] != new_aura_thumbnail_hashes_[id]) {
      UpdateSourceThumbnail(id, image.AsImageSkia());
    }
  }

  // After all aura windows are processed, schedule next refresh;
  pending_aura_capture_requests_--;
  DCHECK_GE(pending_aura_capture_requests_, 0);
  if (pending_aura_capture_requests_ == 0) {
    previous_aura_thumbnail_hashes_ = std::move(new_aura_thumbnail_hashes_);
    // Schedule next refresh if aura thumbnail captures finished after native
    // thumbnail captures.
    if (!pending_native_thumbnail_capture_)
      OnRefreshComplete();
  }
}

#endif  // defined(USE_AURA)
