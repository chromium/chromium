// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/gfx/image/image_skia.h"

#if !defined(OS_ANDROID)
#include "chrome/grit/chromium_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#endif

using content::BrowserThread;
using content::WebContents;

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
const extensions::Extension* GetExtension(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents)
    return NULL;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(web_contents->GetBrowserContext());
  return registry->enabled_extensions().GetExtensionOrAppByURL(
      web_contents->GetURL());
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

base::string16 GetTitle(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents)
    return base::string16();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  const extensions::Extension* const extension = GetExtension(web_contents);
  if (extension)
    return base::UTF8ToUTF16(extension->name());
#endif

  return url_formatter::FormatUrlForSecurityDisplay(web_contents->GetURL());
}

}  // namespace

// Stores usage counts for all the capture devices associated with a single
// WebContents instance. Instances of this class are owned by
// MediaStreamCaptureIndicator. They also observe for the destruction of their
// corresponding WebContents and trigger their own deletion from their
// MediaStreamCaptureIndicator.
class MediaStreamCaptureIndicator::WebContentsDeviceUsage
    : public content::WebContentsObserver {
 public:
  WebContentsDeviceUsage(scoped_refptr<MediaStreamCaptureIndicator> indicator,
                         WebContents* web_contents)
      : WebContentsObserver(web_contents), indicator_(std::move(indicator)) {}

  bool IsCapturingAudio() const { return audio_stream_count_ > 0; }
  bool IsCapturingVideo() const { return video_stream_count_ > 0; }
  bool IsMirroring() const { return mirroring_stream_count_ > 0; }
  bool IsCapturingDesktop() const { return desktop_stream_count_ > 0; }

  std::unique_ptr<content::MediaStreamUI> RegisterMediaStream(
      const blink::MediaStreamDevices& devices,
      std::unique_ptr<MediaStreamUI> ui);

  // Increment ref-counts up based on the type of each device provided.
  void AddDevices(const blink::MediaStreamDevices& devices,
                  base::OnceClosure stop_callback);

  // Decrement ref-counts up based on the type of each device provided.
  void RemoveDevices(const blink::MediaStreamDevices& devices);

  // Helper to call |stop_callback_|.
  void NotifyStopped();

 private:
  int& GetStreamCount(blink::mojom::MediaStreamType type);

  // content::WebContentsObserver overrides.
  void WebContentsDestroyed() override {
    indicator_->UnregisterWebContents(web_contents());
  }

  scoped_refptr<MediaStreamCaptureIndicator> indicator_;
  int audio_stream_count_ = 0;
  int video_stream_count_ = 0;
  int mirroring_stream_count_ = 0;
  int desktop_stream_count_ = 0;

  base::OnceClosure stop_callback_;
  base::WeakPtrFactory<WebContentsDeviceUsage> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebContentsDeviceUsage);
};

// Implements MediaStreamUI interface. Instances of this class are created for
// each MediaStream and their ownership is passed to MediaStream implementation
// in the content layer. Each UIDelegate keeps a weak pointer to the
// corresponding WebContentsDeviceUsage object to deliver updates about state of
// the stream.
class MediaStreamCaptureIndicator::UIDelegate : public content::MediaStreamUI {
 public:
  UIDelegate(base::WeakPtr<WebContentsDeviceUsage> device_usage,
             const blink::MediaStreamDevices& devices,
             std::unique_ptr<::MediaStreamUI> ui)
      : device_usage_(device_usage), devices_(devices), ui_(std::move(ui)) {
    DCHECK(!devices_.empty());
  }

  ~UIDelegate() override {
    if (started_ && device_usage_)
      device_usage_->RemoveDevices(devices_);
  }

 private:
  // content::MediaStreamUI interface.
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback) override {
    DCHECK(!started_);
    started_ = true;

    if (device_usage_) {
      // |device_usage_| handles |stop_callback| when |ui_| is unspecified.
      device_usage_->AddDevices(
          devices_, ui_ ? base::OnceClosure() : std::move(stop_callback));
    }

    // If a custom |ui_| is specified, notify it that the stream started and let
    // it handle the |stop_callback| and |source_callback|.
    if (ui_)
      return ui_->OnStarted(std::move(stop_callback),
                            std::move(source_callback));

    return 0;
  }

  base::WeakPtr<WebContentsDeviceUsage> device_usage_;
  const blink::MediaStreamDevices devices_;
  const std::unique_ptr<::MediaStreamUI> ui_;
  bool started_ = false;

  DISALLOW_COPY_AND_ASSIGN(UIDelegate);
};

std::unique_ptr<content::MediaStreamUI>
MediaStreamCaptureIndicator::WebContentsDeviceUsage::RegisterMediaStream(
    const blink::MediaStreamDevices& devices,
    std::unique_ptr<MediaStreamUI> ui) {
  return std::make_unique<UIDelegate>(weak_factory_.GetWeakPtr(), devices,
                                      std::move(ui));
}

void MediaStreamCaptureIndicator::WebContentsDeviceUsage::AddDevices(
    const blink::MediaStreamDevices& devices,
    base::OnceClosure stop_callback) {
  for (const auto& device : devices)
    ++GetStreamCount(device.type);

  if (web_contents()) {
    stop_callback_ = std::move(stop_callback);
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }

  indicator_->UpdateNotificationUserInterface();
}

void MediaStreamCaptureIndicator::WebContentsDeviceUsage::RemoveDevices(
    const blink::MediaStreamDevices& devices) {
  for (const auto& device : devices) {
    int& stream_count = GetStreamCount(device.type);
    --stream_count;
    DCHECK_GE(stream_count, 0);
  }

  if (web_contents()) {
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
    content_settings::UpdateLocationBarUiForWebContents(web_contents());
  }

  indicator_->UpdateNotificationUserInterface();
}

void MediaStreamCaptureIndicator::WebContentsDeviceUsage::NotifyStopped() {
  if (stop_callback_)
    std::move(stop_callback_).Run();
}

int& MediaStreamCaptureIndicator::WebContentsDeviceUsage::GetStreamCount(
    blink::mojom::MediaStreamType type) {
  switch (type) {
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return audio_stream_count_;

    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return video_stream_count_;

    case blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE:
    case blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
      return mirroring_stream_count_;

    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE:
      return desktop_stream_count_;

    case blink::mojom::MediaStreamType::NO_SERVICE:
    case blink::mojom::MediaStreamType::NUM_MEDIA_TYPES:
      NOTREACHED();
      return video_stream_count_;
  }
}

MediaStreamCaptureIndicator::MediaStreamCaptureIndicator() {}

MediaStreamCaptureIndicator::~MediaStreamCaptureIndicator() {
  // The user is responsible for cleaning up by reporting the closure of any
  // opened devices.  However, there exists a race condition at shutdown: The UI
  // thread may be stopped before CaptureDevicesClosed() posts the task to
  // invoke DoDevicesClosedOnUIThread().  In this case, usage_map_ won't be
  // empty like it should.
  DCHECK(usage_map_.empty() ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));
}

std::unique_ptr<content::MediaStreamUI>
MediaStreamCaptureIndicator::RegisterMediaStream(
    content::WebContents* web_contents,
    const blink::MediaStreamDevices& devices,
    std::unique_ptr<MediaStreamUI> ui) {
  auto& usage = usage_map_[web_contents];
  if (!usage)
    usage = std::make_unique<WebContentsDeviceUsage>(this, web_contents);

  return usage->RegisterMediaStream(devices, std::move(ui));
}

void MediaStreamCaptureIndicator::ExecuteCommand(int command_id,
                                                 int event_flags) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const int index =
      command_id - IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_FIRST;
  DCHECK_LE(0, index);
  DCHECK_GT(static_cast<int>(command_targets_.size()), index);
  WebContents* web_contents = command_targets_[index];
  if (base::Contains(usage_map_, web_contents))
    web_contents->GetDelegate()->ActivateContents(web_contents);
}

bool MediaStreamCaptureIndicator::IsCapturingUserMedia(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = usage_map_.find(web_contents);
  return it != usage_map_.end() &&
         (it->second->IsCapturingAudio() || it->second->IsCapturingVideo());
}

bool MediaStreamCaptureIndicator::IsCapturingVideo(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = usage_map_.find(web_contents);
  return it != usage_map_.end() && it->second->IsCapturingVideo();
}

bool MediaStreamCaptureIndicator::IsCapturingAudio(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = usage_map_.find(web_contents);
  return it != usage_map_.end() && it->second->IsCapturingAudio();
}

bool MediaStreamCaptureIndicator::IsBeingMirrored(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = usage_map_.find(web_contents);
  return it != usage_map_.end() && it->second->IsMirroring();
}

bool MediaStreamCaptureIndicator::IsCapturingDesktop(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = usage_map_.find(web_contents);
  return it != usage_map_.end() && it->second->IsCapturingDesktop();
}

void MediaStreamCaptureIndicator::NotifyStopped(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = usage_map_.find(web_contents);
  DCHECK(it != usage_map_.end());
  it->second->NotifyStopped();
}

void MediaStreamCaptureIndicator::UnregisterWebContents(
    WebContents* web_contents) {
  usage_map_.erase(web_contents);
  UpdateNotificationUserInterface();
}

void MediaStreamCaptureIndicator::MaybeCreateStatusTrayIcon(bool audio,
                                                            bool video) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status_icon_)
    return;

  // If there is no browser process, we should not create the status tray.
  if (!g_browser_process)
    return;

  StatusTray* status_tray = g_browser_process->status_tray();
  if (!status_tray)
    return;

  gfx::ImageSkia image;
  base::string16 tool_tip;
  GetStatusTrayIconInfo(audio, video, &image, &tool_tip);
  DCHECK(!image.isNull());
  DCHECK(!tool_tip.empty());

  status_icon_ = status_tray->CreateStatusIcon(
      StatusTray::MEDIA_STREAM_CAPTURE_ICON, image, tool_tip);
}

void MediaStreamCaptureIndicator::MaybeDestroyStatusTrayIcon() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!status_icon_)
    return;

  // If there is no browser process, we should not do anything.
  if (!g_browser_process)
    return;

  StatusTray* status_tray = g_browser_process->status_tray();
  if (status_tray != NULL) {
    status_tray->RemoveStatusIcon(status_icon_);
    status_icon_ = NULL;
  }
}

void MediaStreamCaptureIndicator::UpdateNotificationUserInterface() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<StatusIconMenuModel> menu(new StatusIconMenuModel(this));
  bool audio = false;
  bool video = false;
  int command_id = IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_FIRST;
  command_targets_.clear();

  for (const auto& it : usage_map_) {
    // Check if any audio and video devices have been used.
    const WebContentsDeviceUsage& usage = *it.second;
    if (!usage.IsCapturingAudio() && !usage.IsCapturingVideo())
      continue;

    WebContents* const web_contents = it.first;

    // The audio/video icon is shown only for non-whitelisted extensions or on
    // Android. For regular tabs on desktop, we show an indicator in the tab
    // icon.
#if BUILDFLAG(ENABLE_EXTENSIONS)
    const extensions::Extension* extension = GetExtension(web_contents);
    if (!extension)
      continue;
#endif

    audio = audio || usage.IsCapturingAudio();
    video = video || usage.IsCapturingVideo();

    command_targets_.push_back(web_contents);
    menu->AddItem(command_id, GetTitle(web_contents));

    // If the menu item is not a label, enable it.
    menu->SetCommandIdEnabled(command_id, command_id != IDC_MinimumLabelValue);

    // If reaching the maximum number, no more item will be added to the menu.
    if (command_id == IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_LAST)
      break;
    ++command_id;
  }

  if (command_targets_.empty()) {
    MaybeDestroyStatusTrayIcon();
    return;
  }

  // The icon will take the ownership of the passed context menu.
  MaybeCreateStatusTrayIcon(audio, video);
  if (status_icon_) {
    status_icon_->SetContextMenu(std::move(menu));
  }
}

void MediaStreamCaptureIndicator::GetStatusTrayIconInfo(
    bool audio,
    bool video,
    gfx::ImageSkia* image,
    base::string16* tool_tip) {
#if defined(OS_ANDROID)
  NOTREACHED();
#else   // !defined(OS_ANDROID)
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(audio || video);
  DCHECK(image);
  DCHECK(tool_tip);

  int message_id = 0;
  const gfx::VectorIcon* icon = nullptr;
  if (audio && video) {
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_AUDIO_AND_VIDEO;
    icon = &vector_icons::kVideocamIcon;
  } else if (audio && !video) {
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_AUDIO_ONLY;
    icon = &vector_icons::kMicIcon;
  } else if (!audio && video) {
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_VIDEO_ONLY;
    icon = &vector_icons::kVideocamIcon;
  }

  *tool_tip = l10n_util::GetStringUTF16(message_id);
  *image = gfx::CreateVectorIcon(*icon, 16, gfx::kChromeIconGrey);
#endif  // !defined(OS_ANDROID)
}
