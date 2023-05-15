// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CROSAPI_UTIL_H_
#define CHROME_BROWSER_ASH_CROSAPI_CROSAPI_UTIL_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "base/token.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/environment_provider.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "url/gurl.h"

class Profile;

// These methods are used by ash-chrome.
namespace crosapi {
namespace browser_util {

// Checks for the given profile if the user is affiliated or belongs to the
// sign-in profile.
bool IsSigninProfileOrBelongsToAffiliatedUser(Profile* profile);

// Returns the UUID and version for all tracked interfaces. Exposed for testing.
base::flat_map<base::Token, uint32_t> GetInterfaceVersions();

// Represents how to launch Lacros Chrome.
struct InitialBrowserAction {
  explicit InitialBrowserAction(crosapi::mojom::InitialBrowserAction action);
  InitialBrowserAction(InitialBrowserAction&&);
  InitialBrowserAction& operator=(InitialBrowserAction&&);
  ~InitialBrowserAction();

  // Mode how to launch Lacros chrome.
  crosapi::mojom::InitialBrowserAction action;

  // If action is kOpenWindowWithUrls, URLs here is passed to Lacros Chrome,
  // and they will be opened.
  std::vector<GURL> urls;

  // Where this request comes from.
  crosapi::mojom::OpenUrlFrom from = crosapi::mojom::OpenUrlFrom::kUnspecified;
};

// Returns the initial parameter to be passed to Crosapi client,
// such as lacros-chrome.
// If |include_post_login_params| is true, it fills the structure
// with all the parameters, including the ones that are only
// available after login.
mojom::BrowserInitParamsPtr GetBrowserInitParams(
    EnvironmentProvider* environment_provider,
    InitialBrowserAction initial_browser_action,
    bool is_keep_alive_enabled,
    absl::optional<browser_util::LacrosSelection> lacros_selection,
    bool include_post_login_params = true);

// Creates a memory backed file containing the serialized |params|,
// and returns its FD.
// If |include_post_login_params| is true, it creates a file
// with all the parameters, including the ones that are only
// available after login.
base::ScopedFD CreateStartupData(
    EnvironmentProvider* environment_provider,
    InitialBrowserAction initial_browser_action,
    bool is_keep_alive_enabled,
    absl::optional<browser_util::LacrosSelection> lacros_selection,
    bool include_post_login_params = true);

// Serializes and writes post-login parameters into the given FD.
bool WritePostLoginData(base::PlatformFile fd,
                        EnvironmentProvider* environment_provider,
                        InitialBrowserAction initial_browser_action);

// Returns the device settings needed for Lacros.
mojom::DeviceSettingsPtr GetDeviceSettings();

}  // namespace browser_util
}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CROSAPI_UTIL_H_
