// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_APPS_PUBLISHER_H_
#define CHROME_BROWSER_LACROS_LACROS_APPS_PUBLISHER_H_

#include <optional>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/media_requests.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}  // namespace content

// This class tracks the Lacros browser in Lacros, and forwards metadata to
// StandaloneBrowserApps in Ash. StandaloneBrowserApps is an AppService
// publisher, which in turn will glue the Lacros browser into the App Service
// infrastructure.
//
// This class tracks the media usage (e.g. accessing camera, microphone) for the
// Lacros browser and forwards the CapabilityAccess updates to the App_Service
// in Ash via crosapi
class LacrosAppsPublisher : public MediaStreamCaptureIndicator::Observer {
 public:
  LacrosAppsPublisher();
  ~LacrosAppsPublisher() override;

  LacrosAppsPublisher(const LacrosAppsPublisher&) = delete;
  LacrosAppsPublisher& operator=(const LacrosAppsPublisher&) = delete;

  // This class does nothing until Initialize is called. This provides an
  // opportunity for this class and subclasses to finish constructing before
  // pointers get passed and used in inner classes.
  void Initialize();

 protected:
  // Publishes differential CapabilityAccess updates to the App_Service in Ash
  // via crosapi. Virtual for testing.
  virtual void PublishCapabilityAccesses(
      std::vector<apps::CapabilityAccessPtr> accesses);

  // Virtual for testing. Sets up the crosapi connection. Returns false on
  // failure.
  virtual bool InitializeCrosapi();

 private:
  // MediaStreamCaptureIndicator::Observer
  void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* web_contents,
                                 bool is_capturing_audio) override;

  // Returns true if `web_contents` is neither a web app, nor a chrome app.
  // Otherwise, returns false.
  bool ShouldModifyCapabilityAccess(content::WebContents* web_contents);

  void ModifyCapabilityAccess(std::optional<bool> accessing_camera,
                              std::optional<bool> accessing_microphone);

  // Mojo endpoint that's responsible for sending app publisher messages to Ash.
  mojo::Remote<crosapi::mojom::AppPublisher> publisher_;

  // Scoped observer for the MediaStreamCaptureIndicator.
  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      media_dispatcher_{this};

  // Tracks the media usage (e.g. accessing camera, microphone) on a
  // per-WebContents basis.
  apps::MediaRequests media_requests_;
};

#endif  // CHROME_BROWSER_LACROS_LACROS_APPS_PUBLISHER_H_
