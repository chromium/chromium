// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_

#include <iosfwd>
#include <optional>

#include "base/functional/callback_forward.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/gfx/native_widget_types.h"
#else
#include "base/unguessable_token.h"
#endif

class Profile;

namespace apps {

class PackageId;

// Service for requesting installation of apps on ChromeOS.
//
// New users of these APIs should add a new AppInstallSurface entry, which is
// used to make decisions on behavior and record metrics per-usecase.
class AppInstallService {
 public:
  static std::unique_ptr<AppInstallService> Create(Profile& profile);

  virtual ~AppInstallService();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  using WindowIdentifier = gfx::NativeWindow;
#else
  // This token is the window ID as registered in the
  // BrowserAppInstanceRegistry.
  using WindowIdentifier = base::UnguessableToken;
#endif

  // Behaves the same as InstallApp() unless `serialized_package_id` isn't
  // recognized in which case it falls back to asking Almanac for an install URL
  // to open instead. This fallback is for when the client is out of date behind
  // new PackageTypes that the Almanac server can understand.
  virtual void InstallAppWithFallback(
      AppInstallSurface surface,
      std::string serialized_package_id,
      std::optional<WindowIdentifier> anchor_window,
      base::OnceClosure callback) = 0;

// Not needed by Lacros clients, so can avoid adding to the crosapi.
#if BUILDFLAG(IS_CHROMEOS_ASH)

  // Requests installation of the app with ID `package_id` from `surface`. This
  // communicates with the Almanac app API to retrieve app data, and then
  // prompts the user to proceed with the installation. `callback` is called
  // when the installation completes, whether successful or unsuccessful.
  // If an app with the same package_id is already installed that app will be
  // launched instead.
  //
  // `anchor_window`: identifier for the parent window to anchor the
  // resultant app install dialog. If absent, the dialog window will be created
  // on top but not anchored to a parent window.
  virtual void InstallApp(AppInstallSurface surface,
                          PackageId package_id,
                          std::optional<WindowIdentifier> anchor_window,
                          base::OnceClosure callback) = 0;

  // Requests installation of the app with ID `package_id` from `surface`. This
  // communicates with the Almanac app API to retrieve app data, and then
  // silently installs the app without further prompting. `callback` is called
  // when the installation completes, whether successful or unsuccessful.
  virtual void InstallAppHeadless(
      AppInstallSurface surface,
      PackageId package_id,
      base::OnceCallback<void(bool success)> callback) = 0;

  // Requests installation of the app `data` from `surface`. This silently
  // installs the given data with no further prompting. `callback` is called
  // when the installation completes, whether successful or unsuccessful.
  virtual void InstallAppHeadless(
      AppInstallSurface surface,
      AppInstallData data,
      base::OnceCallback<void(bool success)> callback) = 0;
#endif
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_
