// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_CAPTURE_INDICATOR_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_CAPTURE_INDICATOR_H_

#include <unordered_map>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

class StatusIcon;

// Interface to display custom UI during screen-capture (tab/window/screen).
class MediaStreamUI {
 public:
  // Called when stream capture is stopped.
  virtual ~MediaStreamUI() = default;

  // Called when screen capture starts.
  // |stop_callback| is a callback to stop the stream.
  // |source_callback| is a callback to change the desktop capture source.
  // Returns the platform-dependent window ID for the UI, or 0 if not
  // applicable.
  // |media_ids| represent the display-surfaces whose capture has started.
  virtual gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) = 0;

  // Called when Region Capture starts/stops, or when the cropped area changes.
  virtual void OnRegionCaptureRectChanged(
      const std::optional<gfx::Rect>& region_capture_rect) {}
};

// Keeps track of which WebContents are capturing media streams. Used to display
// indicators (e.g. in the tab strip, via notifications) and to make resource
// allocation decisions (e.g. WebContents capturing streams are not discarded).
//
// Owned by MediaCaptureDevicesDispatcher, which is a singleton.
class MediaStreamCaptureIndicator
    : public base::RefCountedThreadSafe<MediaStreamCaptureIndicator>,
      public StatusIconMenuModel::Delegate {
 public:
  enum MediaType {
    kUnknown = 0,
    kUserMedia = 1,
    kDisplayMedia = 2,
    kAllScreensMedia = 4,
  };

  // Maps blink::mojom::MediaStreamType to MediaType.
  static MediaType GetMediaType(blink::mojom::MediaStreamType type);

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                           bool is_capturing_video) {}
    virtual void OnIsCapturingAudioChanged(content::WebContents* web_contents,
                                           bool is_capturing_audio) {}
    virtual void OnIsBeingMirroredChanged(content::WebContents* web_contents,
                                          bool is_being_mirrored) {}
    virtual void OnIsCapturingWindowChanged(content::WebContents* web_contents,
                                            bool is_capturing_window) {}
    virtual void OnIsCapturingDisplayChanged(content::WebContents* web_contents,
                                             bool is_capturing_display) {}

   protected:
    ~Observer() override;
  };

  MediaStreamCaptureIndicator();

  MediaStreamCaptureIndicator(const MediaStreamCaptureIndicator&) = delete;
  MediaStreamCaptureIndicator& operator=(const MediaStreamCaptureIndicator&) =
      delete;

  // Registers a new media stream for |web_contents| and returns an object used
  // by the content layer to notify about the state of the stream. Optionally,
  // |ui| is used to display custom UI while the stream is captured.
  std::unique_ptr<content::MediaStreamUI> RegisterMediaStream(
      content::WebContents* web_contents,
      const blink::mojom::StreamDevices& devices,
      std::unique_ptr<MediaStreamUI> ui = nullptr,
      const std::u16string application_title = std::u16string());

  // Overrides from StatusIconMenuModel::Delegate implementation.
  void ExecuteCommand(int command_id, int event_flags) override;

  // Returns true if |web_contents| is capturing user media (e.g., webcam or
  // microphone input).
  bool IsCapturingUserMedia(content::WebContents* web_contents) const;

  // Returns true if |web_contents| is capturing video (e.g., webcam).
  bool IsCapturingVideo(content::WebContents* web_contents) const;

  // Returns true if |web_contents| is capturing audio (e.g., microphone).
  bool IsCapturingAudio(content::WebContents* web_contents) const;

  // Returns true if |web_contents| itself is being mirrored (e.g., a source of
  // media for remote broadcast).
  bool IsBeingMirrored(content::WebContents* web_contents) const;

  // Returns true if |web_contents| is capturing a desktop window or audio.
  bool IsCapturingWindow(content::WebContents* web_contents) const;

  // Returns true if |web_contents| is capturing a display.
  bool IsCapturingDisplay(content::WebContents* web_contents) const;

  // Called to stop media capturing of the |media_type|.
  // |media_type| is underlying_type of MediaType.
  void StopMediaCapturing(content::WebContents* web_contents,
                          int media_type) const;

  // Adds/Removes observers. Observers needs to be removed during the lifetime
  // of this object.
  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }
  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

 private:
  class UIDelegate;
  class WebContentsDeviceUsage;
  friend class WebContentsDeviceUsage;

  friend class base::RefCountedThreadSafe<MediaStreamCaptureIndicator>;
  ~MediaStreamCaptureIndicator() override;

  // Following functions/variables are executed/accessed only on UI thread.

  // Called by WebContentsDeviceUsage when it's about to destroy itself, i.e.
  // when WebContents is being destroyed.
  void UnregisterWebContents(content::WebContents* web_contents);

  // Updates the status tray menu. Called by WebContentsDeviceUsage.
  void UpdateNotificationUserInterface();

  // Helpers to create and destroy status tray icon. Called from
  // UpdateNotificationUserInterface().
  void EnsureStatusTrayIconResources();
  void MaybeCreateStatusTrayIcon(bool audio, bool video);
  void MaybeDestroyStatusTrayIcon();

  // Gets the status icon image and the string to use as the tooltip.
  void GetStatusTrayIconInfo(bool audio,
                             bool video,
                             gfx::ImageSkia* image,
                             std::u16string* tool_tip);

  // Checks if |web_contents| or any inner WebContents in its tree is using
  // a device for capture. The type of capture is specified using |pred|.
  using WebContentsDeviceUsagePredicate =
      base::FunctionRef<bool(const WebContentsDeviceUsage*)>;
  bool CheckUsage(content::WebContents* web_contents,
                  const WebContentsDeviceUsagePredicate& pred) const;

  // Reference to our status icon - owned by the StatusTray. If null,
  // the platform doesn't support status icons.
  raw_ptr<StatusIcon, DanglingUntriaged> status_icon_ = nullptr;

  // A map that contains the usage counts of the opened capture devices for each
  // WebContents instance.
  std::unordered_map<content::WebContents*,
                     std::unique_ptr<WebContentsDeviceUsage>>
      usage_map_;

  // g_stop_callback_id_ is used to identify each stop_callbacks when
  // AddDevices or RemoveDevices is called. We need this because the device_id
  // are not unique.
  static int g_stop_callback_id_;

  // A vector which maps command IDs to their associated WebContents
  // instance. This is rebuilt each time the status tray icon context menu is
  // updated.
  typedef std::vector<raw_ptr<content::WebContents, VectorExperimental>>
      CommandTargets;
  CommandTargets command_targets_;

  base::ObserverList<Observer, /* check_empty =*/true> observers_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_CAPTURE_INDICATOR_H_
