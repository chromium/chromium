// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/observer_list.h"
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
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/gfx/image/image_skia.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/media/webrtc/media_stream_focus_delegate.h"
#include "chrome/grit/branded_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_registry.h"  // nogncheck
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#endif

using content::BrowserThread;
using content::WebContents;

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
const extensions::Extension* GetExtension(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents)
    return nullptr;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(web_contents->GetBrowserContext());
  return registry->enabled_extensions().GetExtensionOrAppByURL(
      web_contents->GetLastCommittedURL());
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

std::u16string GetTitle(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents)
    return std::u16string();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  const extensions::Extension* const extension = GetExtension(web_contents);
  if (extension)
    return base::UTF8ToUTF16(extension->name());
#endif

  return url_formatter::FormatUrlForSecurityDisplay(
      web_contents->GetLastCommittedURL());
}

// Returns if the passed |device| is capturing the whole display. This is
// different from capturing a tab or a single window on a desktop.
bool IsDeviceCapturingDisplay(const blink::MediaStreamDevice& device) {
  return device.display_media_info &&
         device.display_media_info->display_surface ==
             media::mojom::DisplayCaptureSurfaceType::MONITOR;
}

typedef void (MediaStreamCaptureIndicator::Observer::*ObserverMethod)(
    content::WebContents* web_contents,
    bool value);

ObserverMethod GetObserverMethodToCall(const blink::MediaStreamDevice& device) {
  switch (device.type) {
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return &MediaStreamCaptureIndicator::Observer::OnIsCapturingAudioChanged;

    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return &MediaStreamCaptureIndicator::Observer::OnIsCapturingVideoChanged;

    case blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE:
    case blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
      return &MediaStreamCaptureIndicator::Observer::OnIsBeingMirroredChanged;

    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
      return IsDeviceCapturingDisplay(device)
                 ? &MediaStreamCaptureIndicator::Observer::
                       OnIsCapturingDisplayChanged
                 : &MediaStreamCaptureIndicator::Observer::
                       OnIsCapturingWindowChanged;

    case blink::mojom::MediaStreamType::NO_SERVICE:
    case blink::mojom::MediaStreamType::NUM_MEDIA_TYPES:
      NOTREACHED();
  }
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

  WebContentsDeviceUsage(const WebContentsDeviceUsage&) = delete;
  WebContentsDeviceUsage& operator=(const WebContentsDeviceUsage&) = delete;

  ~WebContentsDeviceUsage() override = default;

  bool IsCapturingAudio() const { return audio_stream_count_ > 0; }
  bool IsCapturingVideo() const { return video_stream_count_ > 0; }
  bool IsMirroring() const { return mirroring_stream_count_ > 0; }
  bool IsCapturingWindow() const { return window_stream_count_ > 0; }
  bool IsCapturingDisplay() const { return display_stream_count_ > 0; }

  std::unique_ptr<content::MediaStreamUI> RegisterMediaStream(
      const blink::mojom::StreamDevices& devices,
      std::unique_ptr<MediaStreamUI> ui,
      const std::u16string application_title);

  // Increment ref-counts up based on the type of each device provided.
  void AddDevices(const blink::mojom::StreamDevices& devices,
                  base::OnceClosure stop_callback,
                  int stop_callback_id);

  // Decrement ref-counts up based on the type of each device provided.
  void RemoveDevices(const blink::mojom::StreamDevices& devices,
                     int stop_callback_id);

  void StopMediaCapturing(int media_type);

 private:
  int& GetStreamCount(const blink::MediaStreamDevice& device);

  void AddDevice(const blink::MediaStreamDevice& device);

  void RemoveDevice(const blink::MediaStreamDevice& device);

  // content::WebContentsObserver overrides.
  void WebContentsDestroyed() override {
    indicator_->UnregisterWebContents(web_contents());
  }

  void StopCallbacks(std::map<int, base::OnceClosure>& stop_callbacks);

  scoped_refptr<MediaStreamCaptureIndicator> indicator_;
  int audio_stream_count_ = 0;
  int video_stream_count_ = 0;
  int mirroring_stream_count_ = 0;
  int window_stream_count_ = 0;
  int display_stream_count_ = 0;

  std::map<int, base::OnceClosure> display_media_stop_callbacks_;
  std::map<int, base::OnceClosure> user_media_stop_callbacks_;
  base::WeakPtrFactory<WebContentsDeviceUsage> weak_factory_{this};
};

// Implements MediaStreamUI interface. Instances of this class are created for
// each MediaStream and their ownership is passed to MediaStream implementation
// in the content layer. Each UIDelegate keeps a weak pointer to the
// corresponding WebContentsDeviceUsage object to deliver updates about state of
// the stream.
class MediaStreamCaptureIndicator::UIDelegate : public content::MediaStreamUI {
 public:
  UIDelegate(WebContents* web_contents,
             base::WeakPtr<WebContentsDeviceUsage> device_usage,
             const blink::mojom::StreamDevices& devices,
             std::unique_ptr<::MediaStreamUI> ui,
             const std::u16string application_title)
      : device_usage_(device_usage),
        devices_(devices),
        ui_(std::move(ui)),
#if !BUILDFLAG(IS_ANDROID)
        focus_delegate_(web_contents),
#endif
        application_title_(std::move(application_title)),
        stop_callback_id_(MediaStreamCaptureIndicator::g_stop_callback_id_++) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(devices_.audio_device.has_value() ||
           devices_.video_device.has_value());
  }

  UIDelegate(const UIDelegate&) = delete;
  UIDelegate& operator=(const UIDelegate&) = delete;

  ~UIDelegate() override {
    if (started_ && device_usage_)
      device_usage_->RemoveDevices(devices_, stop_callback_id_);
  }

 private:
  // content::MediaStreamUI interface.
  gfx::NativeViewId OnStarted(
      base::RepeatingClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::string& label,
      std::vector<content::DesktopMediaID> screen_capture_ids,
      StateChangeCallback state_change_callback) override {
    if (started_) {
      // Ignore possibly-compromised renderers that might call
      // MediaStreamDispatcherHost::OnStreamStarted() more than once.
      // See: https://crbug.com/1155426
      return 0;
    }
    started_ = true;

    if (device_usage_) {
      // |device_usage_| handles |stop_callback| when |ui_| is unspecified.
      device_usage_->AddDevices(devices_, stop_callback, stop_callback_id_);
    }

#if BUILDFLAG(IS_CHROMEOS)
    policy::DlpContentManager::Get()->OnScreenShareStarted(
        label, screen_capture_ids, application_title_, stop_callback,
        state_change_callback, source_callback);
#endif

    // If a custom |ui_| is specified, notify it that the stream started and let
    // it handle the |stop_callback| and |source_callback|.
    if (ui_)
      return ui_->OnStarted(stop_callback, std::move(source_callback),
                            screen_capture_ids);

    return 0;
  }

  void OnDeviceStoppedForSourceChange(
      const std::string& label,
      const content::DesktopMediaID& old_media_id,
      const content::DesktopMediaID& new_media_id,
      bool captured_surface_control_active) override {
#if BUILDFLAG(IS_CHROMEOS)
    policy::DlpContentManager::Get()->OnScreenShareSourceChanging(
        label, old_media_id, new_media_id, captured_surface_control_active);
#endif
  }

  void OnDeviceStopped(const std::string& label,
                       const content::DesktopMediaID& media_id) override {
#if BUILDFLAG(IS_CHROMEOS)
    policy::DlpContentManager::Get()->OnScreenShareStopped(label, media_id);
#endif
  }

  void OnRegionCaptureRectChanged(
      const std::optional<gfx::Rect>& region_capture_rect) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (ui_) {
      ui_->OnRegionCaptureRectChanged(region_capture_rect);
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  void SetFocus(const content::DesktopMediaID& media_id,
                bool focus,
                bool is_from_microtask,
                bool is_from_timer) override {
    focus_delegate_.SetFocus(media_id, focus, is_from_microtask, is_from_timer);
  }
#endif

  base::WeakPtr<WebContentsDeviceUsage> device_usage_;
  const blink::mojom::StreamDevices devices_;
  const std::unique_ptr<::MediaStreamUI> ui_;
#if !BUILDFLAG(IS_ANDROID)
  MediaStreamFocusDelegate focus_delegate_;
#endif
  const std::u16string application_title_;
  bool started_ = false;
  const int stop_callback_id_;
};

std::unique_ptr<content::MediaStreamUI>
MediaStreamCaptureIndicator::WebContentsDeviceUsage::RegisterMediaStream(
    const blink::mojom::StreamDevices& devices,
    std::unique_ptr<MediaStreamUI> ui,
    const std::u16string application_title) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return std::make_unique<UIDelegate>(
      web_contents(), weak_factory_.GetWeakPtr(), devices, std::move(ui),
      std::move(application_title));
}

void MediaStreamCaptureIndicator::WebContentsDeviceUsage::AddDevices(
    const blink::mojom::StreamDevices& devices,
    base::OnceClosure stop_callback,
    int stop_callback_id) {
  MediaType type = MediaType::kUnknown;

  if (devices.audio_device.has_value()) {
    AddDevice(devices.audio_device.value());
    type = MediaStreamCaptureIndicator::GetMediaType(
        devices.audio_device.value().type);
  }

  if (devices.video_device.has_value()) {
    AddDevice(devices.video_device.value());
    type = MediaStreamCaptureIndicator::GetMediaType(
        devices.video_device.value().type);
  }

  if (type == MediaType::kUserMedia) {
    user_media_stop_callbacks_[stop_callback_id] = std::move(stop_callback);
  }

  // TODO(crbug.com/40071631): Don't turn on this until related bugs are fixed.
  // This may record the same stop_callback twice and lead to a crash if
  // called later on.
  // if (type == MediaType::kDisplayMedia) {
  //     display_media_stop_callbacks_[stop_callback_id] =
  //     std::move(stop_callback);
  //   }

  if (web_contents()) {
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }

  indicator_->UpdateNotificationUserInterface();
}

void MediaStreamCaptureIndicator::WebContentsDeviceUsage::RemoveDevices(
    const blink::mojom::StreamDevices& devices,
    int stop_callback_id) {
  MediaType type = MediaType::kUnknown;

  if (devices.audio_device.has_value()) {
    RemoveDevice(devices.audio_device.value());
    type = MediaStreamCaptureIndicator::GetMediaType(
        devices.audio_device.value().type);
  }

  if (devices.video_device.has_value()) {
    RemoveDevice(devices.video_device.value());
    type = MediaStreamCaptureIndicator::GetMediaType(
        devices.video_device.value().type);
  }
  if (type == MediaType::kUserMedia) {
    user_media_stop_callbacks_.erase(stop_callback_id);
  }

  if (type == MediaType::kDisplayMedia) {
    display_media_stop_callbacks_.erase(stop_callback_id);
  }

  if (web_contents() && !web_contents()->IsBeingDestroyed()) {
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
    content_settings::UpdateLocationBarUiForWebContents(web_contents());
  }

  indicator_->UpdateNotificationUserInterface();
}

void MediaStreamCaptureIndicator::WebContentsDeviceUsage::StopMediaCapturing(
    int media_type) {
  if (media_type & MediaType::kUserMedia) {
    StopCallbacks(user_media_stop_callbacks_);
  }
  if (media_type & MediaType::kDisplayMedia) {
    StopCallbacks(display_media_stop_callbacks_);
  }
}

void MediaStreamCaptureIndicator::WebContentsDeviceUsage::StopCallbacks(
    std::map<int, base::OnceClosure>& stop_callbacks) {
  // This for loop is implemented in a weird way because std::move on the value
  // will invalid the iterator; we need to record the next iter before
  // std::move.
  for (auto it = stop_callbacks.begin(), it_next = it;
       it != stop_callbacks.end(); it = it_next) {
    ++it_next;
    std::move(it->second).Run();
  }
  stop_callbacks.clear();
}

int& MediaStreamCaptureIndicator::WebContentsDeviceUsage::GetStreamCount(
    const blink::MediaStreamDevice& device) {
  switch (device.type) {
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
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
      return IsDeviceCapturingDisplay(device) ? display_stream_count_
                                              : window_stream_count_;

    case blink::mojom::MediaStreamType::NO_SERVICE:
    case blink::mojom::MediaStreamType::NUM_MEDIA_TYPES:
      NOTREACHED();
  }
}

void MediaStreamCaptureIndicator::WebContentsDeviceUsage::AddDevice(
    const blink::MediaStreamDevice& device) {
  int& stream_count = GetStreamCount(device);
  ++stream_count;

  if (web_contents() && stream_count == 1) {
    ObserverMethod obs_func = GetObserverMethodToCall(device);
    DCHECK(obs_func);
    for (Observer& obs : indicator_->observers_)
      (obs.*obs_func)(web_contents(), true);
  }
}

void MediaStreamCaptureIndicator::WebContentsDeviceUsage::RemoveDevice(
    const blink::MediaStreamDevice& device) {
  int& stream_count = GetStreamCount(device);
  --stream_count;
  DCHECK_GE(stream_count, 0);

  if (web_contents() && stream_count == 0) {
    ObserverMethod obs_func = GetObserverMethodToCall(device);
    DCHECK(obs_func);
    for (Observer& obs : indicator_->observers_)
      (obs.*obs_func)(web_contents(), false);
  }
}

// Return whether current device os a screen sharing.
MediaStreamCaptureIndicator::MediaType
MediaStreamCaptureIndicator::GetMediaType(blink::mojom::MediaStreamType type) {
  switch (type) {
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return MediaStreamCaptureIndicator::MediaType::kUserMedia;

    case blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE:
    case blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB:
      return MediaStreamCaptureIndicator::MediaType::kDisplayMedia;

    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
      return MediaStreamCaptureIndicator::MediaType::kAllScreensMedia;

    default:
      return MediaStreamCaptureIndicator::MediaType::kUnknown;
  }
}

MediaStreamCaptureIndicator::Observer::~Observer() {
  DCHECK(!IsInObserverList());
}

int MediaStreamCaptureIndicator::g_stop_callback_id_ = 0;

MediaStreamCaptureIndicator::MediaStreamCaptureIndicator() = default;

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
    const blink::mojom::StreamDevices& devices,
    std::unique_ptr<MediaStreamUI> ui,
    const std::u16string application_title) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents);

  auto& usage = usage_map_[web_contents];
  if (!usage)
    usage = std::make_unique<WebContentsDeviceUsage>(this, web_contents);

  return usage->RegisterMediaStream(devices, std::move(ui),
                                    std::move(application_title));
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

bool MediaStreamCaptureIndicator::CheckUsage(
    content::WebContents* web_contents,
    const WebContentsDeviceUsagePredicate& pred) const {
  auto it = usage_map_.find(web_contents);
  if (it != usage_map_.end() && pred(it->second.get())) {
    return true;
  }

  for (auto* inner_contents : web_contents->GetInnerWebContents()) {
    if (CheckUsage(inner_contents, pred))
      return true;
  }

  return false;
}

bool MediaStreamCaptureIndicator::IsCapturingUserMedia(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CheckUsage(web_contents, [](const WebContentsDeviceUsage* usage) {
    return usage->IsCapturingAudio() || usage->IsCapturingVideo();
  });
}

bool MediaStreamCaptureIndicator::IsCapturingVideo(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CheckUsage(web_contents, &WebContentsDeviceUsage::IsCapturingVideo);
}

bool MediaStreamCaptureIndicator::IsCapturingAudio(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CheckUsage(web_contents, &WebContentsDeviceUsage::IsCapturingAudio);
}

bool MediaStreamCaptureIndicator::IsBeingMirrored(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CheckUsage(web_contents, &WebContentsDeviceUsage::IsMirroring);
}

bool MediaStreamCaptureIndicator::IsCapturingWindow(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CheckUsage(web_contents, &WebContentsDeviceUsage::IsCapturingWindow);
}

bool MediaStreamCaptureIndicator::IsCapturingDisplay(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CheckUsage(web_contents, &WebContentsDeviceUsage::IsCapturingDisplay);
}

void MediaStreamCaptureIndicator::StopMediaCapturing(
    content::WebContents* web_contents,
    int media_type) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // AllScreensMedia is an managed device feature, and should not be stopped
  // unless fully discussed.
  CHECK(!(media_type & MediaType::kAllScreensMedia))
      << "AllScreensMedia should not be stopped by MediaStreamCaptureIndicator";

  auto it = usage_map_.find(web_contents);
  if (it != usage_map_.end()) {
    it->second->StopMediaCapturing(media_type);
  }
  for (auto* inner_contents : web_contents->GetInnerWebContents())
    StopMediaCapturing(inner_contents, media_type);
}

void MediaStreamCaptureIndicator::UnregisterWebContents(
    WebContents* web_contents) {
  if (IsCapturingVideo(web_contents)) {
    for (Observer& observer : observers_)
      observer.OnIsCapturingVideoChanged(web_contents, false);
  }
  if (IsCapturingAudio(web_contents)) {
    for (Observer& observer : observers_)
      observer.OnIsCapturingAudioChanged(web_contents, false);
  }
  if (IsBeingMirrored(web_contents)) {
    for (Observer& observer : observers_)
      observer.OnIsBeingMirroredChanged(web_contents, false);
  }
  if (IsCapturingWindow(web_contents)) {
    for (Observer& observer : observers_)
      observer.OnIsCapturingWindowChanged(web_contents, false);
  }
  if (IsCapturingDisplay(web_contents)) {
    for (Observer& observer : observers_)
      observer.OnIsCapturingDisplayChanged(web_contents, false);
  }
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
  std::u16string tool_tip;
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
  if (status_tray != nullptr) {
    status_tray->RemoveStatusIcon(status_icon_);
    status_icon_ = nullptr;
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

    // The audio/video icon is shown for extensions or on Android. For
    // regular tabs on desktop, we show an indicator in the tab icon.
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
    std::u16string* tool_tip) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#else   // !BUILDFLAG(IS_ANDROID)
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
  *image = gfx::CreateVectorIcon(*icon, 16, gfx::kGoogleGrey700);
#endif  // !BUILDFLAG(IS_ANDROID)
}
