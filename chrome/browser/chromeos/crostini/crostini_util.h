// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "ui/base/resource/scale_factor.h"

namespace base {
class FilePath;
class TimeDelta;
}  // namespace base

namespace gfx {
class ImageSkia;
}  // namespace gfx

class Profile;

namespace crostini {

// Enables/disables overriding IsCrostiniUIAllowedForProfile's normal
// behaviour and returning true instead.
void SetCrostiniUIAllowedForTesting(bool enabled);

// Returns true if crostini is allowed to run for |profile|.
// Otherwise, returns false, e.g. if crostini is not available on the device,
// or it is in the flow to set up managed account creation.
bool IsCrostiniAllowedForProfile(Profile* profile);

// Returns true if crostini UI can be shown. Implies crostini is allowed to
// run.
bool IsCrostiniUIAllowedForProfile(Profile* profile);

// Returns whether if Crostini has been enabled, i.e. the user has launched it
// at least once and not deleted it.
bool IsCrostiniEnabled(Profile* profile);

// Returns whether the default Crostini VM is running for the user.
bool IsCrostiniRunning(Profile* profile);

// Launches the Crostini app with ID of |app_id| on the display with ID of
// |display_id|. |app_id| should be a valid Crostini app list id.
void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id);

// Launch a Crostini App with a given set of files, given as absolute paths in
// the container. For apps which can only be launched with a single file,
// launch multiple instances.
void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id,
                       const std::vector<std::string>& files);

// Convenience wrapper around CrostiniAppIconLoader. As requesting icons from
// the container can be slow, we just use the default (penguin) icons after the
// timeout elapses. Subsequent calls would get the correct icons once loaded.
void LoadIcons(Profile* profile,
               const std::vector<std::string>& app_ids,
               int resource_size_in_dip,
               ui::ScaleFactor scale_factor,
               base::TimeDelta timeout,
               base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)>
                   icons_loaded_callback);

// Retrieves cryptohome_id from profile.
std::string CryptohomeIdForProfile(Profile* profile);

// Retrieves username from profile.  This is the text until '@' in
// profile->GetProfileUserName() email address.
std::string ContainerUserNameForProfile(Profile* profile);

// Returns the home directory within the container for a given profile.
base::FilePath ContainerHomeDirectoryForProfile(Profile* profile);

// Returns the mount directory within the container where paths from the Chrome
// OS host such as within Downloads are shared with the container.
base::FilePath ContainerChromeOSBaseDirectory();

// The Terminal opens Crosh but overrides the Browser's app_name so that we can
// identify it as the Crostini Terminal. In the future, we will also use these
// for Crostini apps marked Terminal=true in their .desktop file.
std::string AppNameFromCrostiniAppId(const std::string& id);

// Returns nullopt for a non-Crostini app name.
base::Optional<std::string> CrostiniAppIdFromAppName(
    const std::string& app_name);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CrostiniUISurface { kSettings = 0, kAppList = 1, kCount };

// See chrome/browser/ui/views/crostini for implementation of the ShowXXX
// functions below.

// Shows the Crostini Installer dialog.
void ShowCrostiniInstallerView(Profile* profile, CrostiniUISurface ui_surface);
// Shows the Crostini Uninstaller dialog.
void ShowCrostiniUninstallerView(Profile* profile,
                                 CrostiniUISurface ui_surface);
// Shows the Crostini Upgrade dialog.
void ShowCrostiniUpgradeView(Profile* profile, CrostiniUISurface ui_surface);

// We use an arbitrary well-formed extension id for the Terminal app, this
// is equal to GenerateId("Terminal").
constexpr char kCrostiniTerminalId[] = "oajcgpnkmhaalajejhlfpacbiokdnnfe";

constexpr char kCrostiniDefaultVmName[] = "termina";
constexpr char kCrostiniDefaultContainerName[] = "penguin";
constexpr char kCrostiniCroshBuiltinAppId[] =
    "nkoccljplnhpfnfiajclkommnmllphnl";
// In order to be compatible with sync folder id must match standard.
// Generated using crx_file::id_util::GenerateId("LinuxAppsFolder")
constexpr char kCrostiniFolderId[] = "ddolnhmblagmcagkedkbfejapapdimlk";
constexpr char kCrostiniDefaultImageServerUrl[] =
    "https://storage.googleapis.com/cros-containers";
constexpr char kCrostiniDefaultImageAlias[] = "debian/stretch";

// Whether running Crostini is allowed for unaffiliated users per enterprise
// policy.
bool IsUnaffiliatedCrostiniAllowedByPolicy();

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_
