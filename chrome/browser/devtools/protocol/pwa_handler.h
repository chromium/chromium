// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_PWA_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_PWA_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/protocol/pwa.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {
class WebAppCommandScheduler;
struct WebAppInstallInfo;
}  // namespace web_app

class PWAHandler final : public protocol::PWA::Backend {
 public:
  explicit PWAHandler(protocol::UberDispatcher* dispatcher,
                      const std::string& target_id);

  PWAHandler(const PWAHandler&) = delete;
  PWAHandler& operator=(const PWAHandler&) = delete;

  ~PWAHandler() override;

 private:
  /// Overridden PWA::Backend APIs ///
  void GetOsAppState(const std::string& in_manifest_id,
                     std::unique_ptr<GetOsAppStateCallback> callback) override;

  void Install(const std::string& in_manifest_id,
               protocol::Maybe<std::string> in_install_url_or_bundle_url,
               std::unique_ptr<InstallCallback> callback) override;

  void Uninstall(const std::string& in_manifest_id,
                 std::unique_ptr<UninstallCallback> callback) override;

  void Launch(const std::string& in_manifest_id,
              protocol::Maybe<std::string> in_url,
              std::unique_ptr<LaunchCallback> callback) override;

  void LaunchFilesInApp(
      const std::string& in_manifest_id,
      std::unique_ptr<protocol::Array<std::string>> in_files,
      std::unique_ptr<LaunchFilesInAppCallback> callback) override;

  protocol::Response OpenCurrentPageInApp(
      const std::string& in_manifest_id) override;

  void ChangeAppUserSettings(
      const std::string& in_manifest_id,
      protocol::Maybe<bool> in_link_capturing,
      protocol::Maybe<protocol::PWA::DisplayMode> in_display_mode,
      std::unique_ptr<ChangeAppUserSettingsCallback> callback) override;

  /// Implementation details ///

  // Installs from only the manifest id; requires a WebContents.
  void InstallFromManifestId(const std::string& in_manifest_id,
                             std::unique_ptr<InstallCallback> callback);

  // Installs from the url; requires the manifest_id from the url to match the
  // input in_manifest_id.
  void InstallFromUrl(const std::string& in_manifest_id,
                      const std::string& in_install_url_or_bundle_url,
                      std::unique_ptr<InstallCallback> callback);

  // Is called by InstallFromUrl only.
  void InstallFromInstallInfo(
      const std::string& in_manifest_id,
      const std::string& in_install_url_or_bundle_url,
      std::unique_ptr<InstallCallback> callback,
      std::unique_ptr<web_app::WebAppInstallInfo> web_app_info);

  // Returns the profile of the devtools session, never be nullptr.
  // TODO(crbug.com/331214986): Consider changing to references after the coding
  // style being widely adopted.
  Profile* GetProfile() const;

  // Returns the WebAppCommandScheduler associated with this session, may be
  // nullptr. This is a shortcut of getting web_app::WebAppProvider::scheduler()
  // from GetProfile().
  web_app::WebAppCommandScheduler* GetScheduler() const;

  // Returns the associated WebContents in the devtools session, may be nullptr.
  content::WebContents* GetWebContents() const;

  const std::string target_id_;

  base::WeakPtrFactory<PWAHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_PWA_HANDLER_H_
