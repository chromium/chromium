// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SCREEN_CAPTURE_ARC_SCREEN_CAPTURE_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_SCREEN_CAPTURE_ARC_SCREEN_CAPTURE_BRIDGE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "ash/components/arc/mojom/screen_capture.mojom.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/desktop_media_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcScreenCaptureBridge : public KeyedService,
                               public mojom::ScreenCaptureHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcScreenCaptureBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcScreenCaptureBridge(content::BrowserContext* context,
                         ArcBridgeService* bridge_service);

  ArcScreenCaptureBridge(const ArcScreenCaptureBridge&) = delete;
  ArcScreenCaptureBridge& operator=(const ArcScreenCaptureBridge&) = delete;

  ~ArcScreenCaptureBridge() override;

  // mojom::ScreenCaptureHost overrides:
  void OpenSession(
      mojo::PendingRemote<mojom::ScreenCaptureSessionNotifier> notifier,
      const std::string& package_name,
      const gfx::Size& size,
      OpenSessionCallback callback) override;
  void RequestPermission(const std::string& display_name,
                         const std::string& package_name,
                         RequestPermissionCallback callback) override;
  void TestModeAcceptPermission(const std::string& package_name) override;

  static void EnsureFactoryBuilt();

 private:
  struct PendingCaptureParams {
    PendingCaptureParams(std::unique_ptr<DesktopMediaPicker> picker,
                         const std::string& display_name,
                         RequestPermissionCallback callback);
    ~PendingCaptureParams();

    std::unique_ptr<DesktopMediaPicker> picker;
    const std::string display_name;
    RequestPermissionCallback callback;
  };

  struct GrantedCaptureParams {
    GrantedCaptureParams(const std::string& display_name,
                         content::DesktopMediaID desktop_id,
                         bool enable_notification);
    ~GrantedCaptureParams();

    const std::string display_name;
    const content::DesktopMediaID desktop_id;
    const bool enable_notification;
  };

  void PermissionPromptCallback(const std::string& package_name,
                                content::DesktopMediaID desktop_id);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  // The string in this map corresponds to the passed in package_name when
  // RequestPermission is called. This map is used for when we get the callback
  // from the permissions dialog or when we get a Mojo call to forcibly confirm
  // the permissions. If not for the latter feature, we would not be using this
  // map and instead would just Bind these parameters into the permissions
  // dialog callback itself.
  std::unordered_map<std::string, PendingCaptureParams>
      pending_permissions_map_;

  // The string in this map corresponds to the passed in package_name when
  // RequestPermission is called. That same string should then be passed into
  // OpenSession as a token that correlates the two calls. This map is used to
  // validate that.
  std::unordered_map<std::string, GrantedCaptureParams>
      granted_permissions_map_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcScreenCaptureBridge> weak_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SCREEN_CAPTURE_ARC_SCREEN_CAPTURE_BRIDGE_H_
