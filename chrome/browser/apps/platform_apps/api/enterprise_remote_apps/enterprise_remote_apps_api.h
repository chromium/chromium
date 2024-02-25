// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_ENTERPRISE_REMOTE_APPS_ENTERPRISE_REMOTE_APPS_API_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_ENTERPRISE_REMOTE_APPS_ENTERPRISE_REMOTE_APPS_API_H_

#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "extensions/browser/extension_function.h"

namespace chrome_apps::api {

class EnterpriseRemoteAppsAddFolderFunction : public ExtensionFunction {
 public:
  EnterpriseRemoteAppsAddFolderFunction();

  EnterpriseRemoteAppsAddFolderFunction(
      const EnterpriseRemoteAppsAddFolderFunction&) = delete;

  EnterpriseRemoteAppsAddFolderFunction& operator=(
      const EnterpriseRemoteAppsAddFolderFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("enterprise.remoteApps.addFolder",
                             ENTERPRISE_REMOTEAPPS_ADDFOLDER)

 protected:
  ~EnterpriseRemoteAppsAddFolderFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnResult(chromeos::remote_apps::mojom::AddFolderResultPtr result);
};

class EnterpriseRemoteAppsAddAppFunction : public ExtensionFunction {
 public:
  EnterpriseRemoteAppsAddAppFunction();

  EnterpriseRemoteAppsAddAppFunction(
      const EnterpriseRemoteAppsAddAppFunction&) = delete;

  EnterpriseRemoteAppsAddAppFunction& operator=(
      const EnterpriseRemoteAppsAddAppFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("enterprise.remoteApps.addApp",
                             ENTERPRISE_REMOTEAPPS_ADDAPP)

 protected:
  ~EnterpriseRemoteAppsAddAppFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnResult(chromeos::remote_apps::mojom::AddAppResultPtr result);
};

class EnterpriseRemoteAppsDeleteAppFunction : public ExtensionFunction {
 public:
  EnterpriseRemoteAppsDeleteAppFunction();

  EnterpriseRemoteAppsDeleteAppFunction(
      const EnterpriseRemoteAppsDeleteAppFunction&) = delete;

  EnterpriseRemoteAppsDeleteAppFunction& operator=(
      const EnterpriseRemoteAppsDeleteAppFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("enterprise.remoteApps.deleteApp",
                             ENTERPRISE_REMOTEAPPS_DELETEAPP)

 protected:
  ~EnterpriseRemoteAppsDeleteAppFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnResult(const std::optional<std::string>& error);
};

class EnterpriseRemoteAppsSortLauncherFunction : public ExtensionFunction {
 public:
  EnterpriseRemoteAppsSortLauncherFunction();

  EnterpriseRemoteAppsSortLauncherFunction(
      const EnterpriseRemoteAppsSortLauncherFunction&) = delete;

  EnterpriseRemoteAppsSortLauncherFunction& operator=(
      const EnterpriseRemoteAppsSortLauncherFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("enterprise.remoteApps.sortLauncher",
                             ENTERPRISE_REMOTEAPPS_SORTLAUNCHER)

 protected:
  ~EnterpriseRemoteAppsSortLauncherFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnResult(const std::optional<std::string>& error);
};

class EnterpriseRemoteAppsSetPinnedAppsFunction : public ExtensionFunction {
 public:
  EnterpriseRemoteAppsSetPinnedAppsFunction();

  EnterpriseRemoteAppsSetPinnedAppsFunction(
      const EnterpriseRemoteAppsSetPinnedAppsFunction&) = delete;

  EnterpriseRemoteAppsSortLauncherFunction& operator=(
      const EnterpriseRemoteAppsSetPinnedAppsFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("enterprise.remoteApps.setPinnedApps",
                             ENTERPRISE_REMOTEAPPS_SETPINNEDAPPS)

 protected:
  ~EnterpriseRemoteAppsSetPinnedAppsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnResult(const std::optional<std::string>& error);
};

}  // namespace chrome_apps::api

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_ENTERPRISE_REMOTE_APPS_ENTERPRISE_REMOTE_APPS_API_H_
