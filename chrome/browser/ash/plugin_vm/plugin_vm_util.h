// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_UTIL_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_UTIL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace aura {
class Window;
}  // namespace aura

class Profile;
class GURL;

namespace plugin_vm {

class PluginVmAvailabilitySubscription;

// Name of the pita DLC.
extern const char kPitaDlc[];

// This is used by both the Plugin VM app and its installer.
// Generated as crx_file::id_util::GenerateId("org.chromium.plugin_vm");
extern const char kPluginVmShelfAppId[];

// Name of the Plugin VM.
extern const char kPluginVmName[];

// Base directory for shared paths in Plugin VM, formatted for display.
extern const char kChromeOSBaseDirectoryDisplayText[];

const net::NetworkTrafficAnnotationTag kPluginVmNetworkTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("plugin_vm_image_download", R"(
      semantics {
        sender: "Plugin VM image manager"
        description: "Request to download Plugin VM image is sent in order "
          "to allow user to run Plugin VM."
        trigger: "User clicking on Plugin VM icon when Plugin VM is not yet "
          "installed."
        data: "Request to download Plugin VM image. Sends cookies to "
          "authenticate the user."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        chrome_policy {
          PluginVmImage {
            PluginVmImage: "{'url': 'example.com', 'hash': 'sha256hash'}"
          }
        }
      }
    )");

// Determines if the default Plugin VM is running and visible.
bool IsPluginVmRunning(Profile* profile);

void ShowPluginVmInstallerView(Profile* profile);

// Checks if an window is for the Plugin VM app. Note that it returns false for
// the Plugin VM installer.
bool IsPluginVmAppWindow(const aura::Window* window);

// Retrieves the User Id to be used for Plugin VM. If none is set this will
// return an empty string.
std::string GetPluginVmUserIdForProfile(const Profile* profile);

// Sets fake policy values and enables Plugin VM for tast tests. Device license
// keys are no longer supported in policy, but for testing purposes this key
// will still be used by the PluginVmService. This function also makes the
// installer skip its license check.
// This sets global state, not per-profile state.
// TODO(crbug.com/40107731): Set policy directly from tast instead of using a
// test helper function.
void SetFakePluginVmPolicy(Profile* profile,
                           const std::string& image_path,
                           const std::string& image_hash,
                           const std::string& license_key);
bool FakeLicenseKeyIsSet();
std::string GetFakeLicenseKey();

// Used to clean up the Plugin VM Drive download directory if it did not get
// removed when it should have, perhaps due to a crash.
void RemoveDriveDownloadDirectoryIfExists();

// Returns nullopt if not a drive URL.
std::optional<std::string> GetIdFromDriveUrl(const GURL& url);

// Returns true if window is PluginVM.
bool IsPluginvmWindowId(const std::string& window_id);

// A subscription for changes to Plugin VM's availability. The callback is
// called whenever there are changes that would affect either
// PluginVmFeatures::Get()->IsAllowed() or IsConfigured().
class PluginVmAvailabilitySubscription {
 public:
  using AvailabilityChangeCallback =
      base::RepeatingCallback<void(bool is_allowed, bool is_configured)>;
  PluginVmAvailabilitySubscription(Profile* profile,
                                   AvailabilityChangeCallback callback);
  ~PluginVmAvailabilitySubscription();

  PluginVmAvailabilitySubscription(const PluginVmAvailabilitySubscription&) =
      delete;
  PluginVmAvailabilitySubscription& operator=(
      const PluginVmAvailabilitySubscription&) = delete;

 private:
  void OnPolicyChanged();
  void OnImageExistsChanged();

  raw_ptr<Profile> profile_;

  // Whether Plugin VM was previously allowed for the profile.
  bool is_allowed_;
  bool is_configured_;

  // The user-provided callback method.
  AvailabilityChangeCallback callback_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::CallbackListSubscription device_allowed_subscription_;
  base::CallbackListSubscription fake_license_subscription_;
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_UTIL_H_
