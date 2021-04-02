// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/native_desktop_media_list.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/hash/hash.h"
#include "base/message_loop/message_pump_type.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
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

#if defined(OS_MAC)
#include "components/remote_cocoa/browser/scoped_cg_window_id.h"
#endif

using content::DesktopMediaID;

namespace {

// Update the list every second.
const int kDefaultNativeDesktopMediaListUpdatePeriod = 1000;

bool IsFrameValid(webrtc::DesktopFrame* frame) {
  // These checks ensure invalid data isn't passed along, potentially leading to
  // crashes, e.g. when we calculate the hash which assumes a positive height
  // and stride.
  // TODO(crbug.com/1085230): figure out why the height is sometimes negative.
  return frame && frame->data() && frame->stride() >= 0 &&
         frame->size().height() >= 0;
}

// Returns a hash of a DesktopFrame content to detect when image for a desktop
// media source has changed.
uint32_t GetFrameHash(webrtc::DesktopFrame* frame) {
  // TODO(dcheng): Is this vulnerable to overflow??
  int data_size = frame->stride() * frame->size().height();
  return base::FastHash(base::make_span(frame->data(), data_size));
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
         DesktopMediaList::Type type,
         std::unique_ptr<webrtc::DesktopCapturer> capturer);
  ~Worker() override;

  void Start();
  void Refresh(const DesktopMediaID::Id& view_dialog_id, bool update_thumnails);

  void RefreshThumbnails(std::vector<DesktopMediaID> native_ids,
                         const gfx::Size& thumbnail_size);

 private:
  typedef std::map<DesktopMediaID, uint32_t> ImageHashesMap;

  // Used to hold state associated with a call to RefreshThumbnails.
  struct RefreshThumbnailsState {
    std::vector<DesktopMediaID> source_ids;
    gfx::Size thumbnail_size;
    ImageHashesMap new_image_hashes;
    size_t next_source_index = 0;
  };

  void RefreshNextThumbnail();

  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // Task runner used for capturing operations.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtr<NativeDesktopMediaList> media_list_;

  DesktopMediaList::Type type_;
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;

  // Stores hashes of snapshots previously captured.
  ImageHashesMap image_hashes_;

  // Non-null when RefreshThumbnails hasn't yet completed.
  std::unique_ptr<RefreshThumbnailsState> refresh_thumbnails_state_;

  base::WeakPtrFactory<Worker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

NativeDesktopMediaList::Worker::Worker(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<NativeDesktopMediaList> media_list,
    DesktopMediaList::Type type,
    std::unique_ptr<webrtc::DesktopCapturer> capturer)
    : task_runner_(task_runner),
      media_list_(media_list),
      type_(type),
      capturer_(std::move(capturer)) {
  DCHECK(capturer_);
}

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
    // Will pass empty results list to RefreshForVizFrameSinkWindows().
    sources.clear();
  }

  bool mutiple_sources = sources.size() > 1;
  std::u16string title;
  for (size_t i = 0; i < sources.size(); ++i) {
    DesktopMediaID::Type type = DesktopMediaID::Type::TYPE_NONE;
    switch (type_) {
      case DesktopMediaList::Type::kScreen:
        type = DesktopMediaID::Type::TYPE_SCREEN;
        // Just in case 'Screen' is inflected depending on the screen number,
        // use plural formatter.
        title = mutiple_sources
                    ? l10n_util::GetPluralStringFUTF16(
                          IDS_DESKTOP_MEDIA_PICKER_MULTIPLE_SCREEN_NAME,
                          static_cast<int>(i + 1))
                    : l10n_util::GetStringUTF16(
                          IDS_DESKTOP_MEDIA_PICKER_SINGLE_SCREEN_NAME);
        break;

      case DesktopMediaList::Type::kWindow:
        type = DesktopMediaID::Type::TYPE_WINDOW;
        // Skip the picker dialog window.
        if (sources[i].id == view_dialog_id)
          continue;
        title = base::UTF8ToUTF16(sources[i].title);
        break;

      default:
        NOTREACHED();
    }
    result.emplace_back(DesktopMediaID(type, sources[i].id), title);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeDesktopMediaList::RefreshForVizFrameSinkWindows,
                     media_list_, result, update_thumnails));
}

void NativeDesktopMediaList::Worker::RefreshThumbnails(
    std::vector<DesktopMediaID> native_ids,
    const gfx::Size& thumbnail_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Ignore if refresh is already in progress.
  if (refresh_thumbnails_state_)
    return;

  // To refresh thumbnails, a snapshot of each window is captured and scaled
  // down to the specified size. Snapshotting can be asynchronous, and so
  // the process looks like the following steps:
  //
  // 1) RefreshNextThumbnail
  // 2) OnCaptureResult
  // 3) UpdateSourceThumbnail (if the snapshot changed)
  // [repeat 1, 2 and 3 until all thumbnails are refreshed]
  // 4) RefreshNextThumbnail
  // 5) UpdateNativeThumbnailsFinished
  //
  // |image_hashes_| is used to help avoid updating thumbnails that haven't
  // changed since the last refresh.

  refresh_thumbnails_state_ = std::make_unique<RefreshThumbnailsState>();
  refresh_thumbnails_state_->source_ids = std::move(native_ids);
  refresh_thumbnails_state_->thumbnail_size = thumbnail_size;
  RefreshNextThumbnail();
}

void NativeDesktopMediaList::Worker::RefreshNextThumbnail() {
  DCHECK(refresh_thumbnails_state_);

  for (size_t index = refresh_thumbnails_state_->next_source_index;
       index < refresh_thumbnails_state_->source_ids.size(); ++index) {
    refresh_thumbnails_state_->next_source_index = index + 1;
    DesktopMediaID source_id = refresh_thumbnails_state_->source_ids[index];
    if (capturer_->SelectSource(source_id.id)) {
      capturer_->CaptureFrame();  // Completes with OnCaptureResult.
      return;
    }
  }

  // Done capturing thumbnails.
  image_hashes_.swap(refresh_thumbnails_state_->new_image_hashes);
  refresh_thumbnails_state_.reset();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeDesktopMediaList::UpdateNativeThumbnailsFinished,
                     media_list_));
}

void NativeDesktopMediaList::Worker::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  auto index = refresh_thumbnails_state_->next_source_index - 1;
  DCHECK(index < refresh_thumbnails_state_->source_ids.size());
  DesktopMediaID id = refresh_thumbnails_state_->source_ids[index];

  // |frame| may be null if capture failed (e.g. because window has been
  // closed).
  if (IsFrameValid(frame.get())) {
    uint32_t frame_hash = GetFrameHash(frame.get());
    refresh_thumbnails_state_->new_image_hashes[id] = frame_hash;

    // Scale the image only if it has changed.
    auto it = image_hashes_.find(id);
    if (it == image_hashes_.end() || it->second != frame_hash) {
      gfx::ImageSkia thumbnail = ScaleDesktopFrame(
          std::move(frame), refresh_thumbnails_state_->thumbnail_size);
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&NativeDesktopMediaList::UpdateSourceThumbnail,
                         media_list_, id, thumbnail));
    }
  }

  // Protect against possible re-entrancy since OnCaptureResult can be invoked
  // from within the call to CaptureFrame.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&Worker::RefreshNextThumbnail,
                                weak_factory_.GetWeakPtr()));
}

NativeDesktopMediaList::NativeDesktopMediaList(
    DesktopMediaList::Type type,
    std::unique_ptr<webrtc::DesktopCapturer> capturer)
    : DesktopMediaListBase(base::TimeDelta::FromMilliseconds(
          kDefaultNativeDesktopMediaListUpdatePeriod)),
      thread_("DesktopMediaListCaptureThread") {
  type_ = type;

#if defined(OS_WIN) || defined(OS_MAC)
  // On Windows/OSX the thread must be a UI thread.
  base::MessagePumpType thread_type = base::MessagePumpType::UI;
#else
  base::MessagePumpType thread_type = base::MessagePumpType::DEFAULT;
#endif
  thread_.StartWithOptions(base::Thread::Options(thread_type, 0));

  worker_ = std::make_unique<Worker>(thread_.task_runner(),
                                     weak_factory_.GetWeakPtr(), type,
                                     std::move(capturer));

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

void NativeDesktopMediaList::RefreshForVizFrameSinkWindows(
    std::vector<SourceDescription> sources,
    bool update_thumnails) {
  DCHECK(can_refresh());

  // Assign |source.id.window_id| if |source.id.id| corresponds to a
  // viz::FrameSinkId.
  for (auto& source : sources) {
    if (source.id.type != DesktopMediaID::TYPE_WINDOW)
      continue;

#if defined(USE_AURA)
    aura::WindowTreeHost* const host =
        aura::WindowTreeHost::GetForAcceleratedWidget(
            *reinterpret_cast<gfx::AcceleratedWidget*>(&source.id.id));
    aura::Window* const aura_window = host ? host->window() : nullptr;
    if (aura_window) {
      DesktopMediaID aura_id = DesktopMediaID::RegisterNativeWindow(
          DesktopMediaID::TYPE_WINDOW, aura_window);
      source.id.window_id = aura_id.window_id;
    }
#elif defined(OS_MAC)
    if (base::FeatureList::IsEnabled(features::kWindowCaptureMacV2)) {
      if (remote_cocoa::ScopedCGWindowID::Get(source.id.id))
        source.id.window_id = source.id.id;
    }
#endif
  }

  UpdateSourcesList(sources);

  if (!update_thumnails) {
    OnRefreshComplete();
    return;
  }

  if (thumbnail_size_.IsEmpty()) {
#if defined(USE_AURA)
    pending_native_thumbnail_capture_ = true;
#endif
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&NativeDesktopMediaList::UpdateNativeThumbnailsFinished,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // OnAuraThumbnailCaptured() and UpdateNativeThumbnailsFinished() are
  // guaranteed to be executed after RefreshForVizFrameSinkWindows() and
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
                                  base::Unretained(worker_.get()),
                                  std::move(native_ids), thumbnail_size_));
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
      base::BindOnce(&NativeDesktopMediaList::OnAuraThumbnailCaptured,
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
