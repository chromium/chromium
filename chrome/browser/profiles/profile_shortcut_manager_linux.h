// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_LINUX_H_
#define CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_LINUX_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"

class SkBitmap;

namespace base {
class Environment;
class SequencedTaskRunner;
}  // namespace base

namespace profiles::internal {

// Prefix of the badged profile icon that is written into the profile directory
// and referenced by the profile's desktop entry. The full name is
// "<prefix>-<hash of the icon>.png", so that the icon path changes whenever
// the icon does. Desktop environments cache icons by path, so overwriting the
// icon in place would leave a stale icon in docks and app grids.
inline constexpr base::FilePath::CharType kProfileDesktopEntryIconPrefix[] =
    "Google Profile";

// Returns the directory that per-profile desktop entries are installed into:
// $XDG_DATA_HOME/applications.
base::FilePath GetProfileDesktopEntryDirectory(base::Environment* env);

// Returns `app_icon` with `avatar` drawn in a circle in the bottom right
// corner. Returns `app_icon` if either bitmap is empty.
SkBitmap BadgeIconWithAvatar(const SkBitmap& app_icon, const SkBitmap& avatar);

}  // namespace profiles::internal

// Manages per-profile desktop entries on Linux. A profile's desktop entry is
// named after its per-profile desktop entry ID (see
// shell_integration_linux::GetPerProfileDesktopId()), launches the profile, and
// uses the browser icon badged with the profile's avatar. Once the entry is
// installed, the profile's browser windows use the per-profile ID as their
// X11 WM_CLASS res_name and Wayland app_id, so desktop environments show them
// with the entry's name and icon instead of grouping them with the main
// browser entry. The default profile always uses the main browser entry, so no
// desktop entry is created for it. Like on Windows, profile shortcuts only
// exist while there are multiple profiles.
class ProfileShortcutManagerLinux : public ProfileShortcutManager,
                                    public ProfileAttributesStorage::Observer {
 public:
  // How long to wait after a desktop entry is installed before the profile's
  // windows switch to the per-profile identity. GNOME Shell only notices new
  // desktop entries up to 5 seconds after they are written (see
  // ShellAppCache), and its window tracker only re-evaluates a window's
  // application when the window's WM_CLASS / app_id changes. Windows that
  // switch before the shell knows the entry would stay associated with the
  // main browser entry.
  static constexpr base::TimeDelta kDesktopEntryRegistrationDelay =
      base::Seconds(10);

  explicit ProfileShortcutManagerLinux(ProfileManager* manager);
  ProfileShortcutManagerLinux(const ProfileShortcutManagerLinux&) = delete;
  ProfileShortcutManagerLinux& operator=(const ProfileShortcutManagerLinux&) =
      delete;
  ~ProfileShortcutManagerLinux() override;

  // ProfileShortcutManager:
  void CreateOrUpdateProfileIcon(const base::FilePath& profile_path) override;
  void CreateProfileShortcut(const base::FilePath& profile_path) override;
  void RemoveProfileShortcuts(const base::FilePath& profile_path) override;
  void HasProfileShortcuts(const base::FilePath& profile_path,
                           base::OnceCallback<void(bool)> callback) override;
  void GetShortcutProperties(const base::FilePath& profile_path,
                             base::CommandLine* command_line,
                             std::wstring* name,
                             base::FilePath* icon_path) override;
  bool CanCreateProfileShortcut(const base::FilePath& profile_path) override;

  // ProfileAttributesStorage::Observer:
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;

  // Runs `callback` once all file operations posted so far have completed.
  void FlushForTesting(base::OnceClosure callback);

 private:
  enum class Mode {
    // Create the desktop entry if it doesn't exist yet.
    kCreate,
    // Only update the desktop entry if it exists.
    kUpdateExistingOnly,
  };

  void CreateOrUpdateDesktopEntry(const base::FilePath& profile_path,
                                  Mode mode);
  void RemoveDesktopEntry(const base::FilePath& profile_path);
  // `icon_path` is the icon of the written desktop entry, or std::nullopt if
  // the entry couldn't be written or no longer exists.
  void OnDesktopEntryWritten(const base::FilePath& profile_path,
                             uint64_t generation,
                             std::optional<base::FilePath> icon_path);
  // Marks the profile's desktop entry as installed, unless another file
  // operation has been started since `generation`.
  void OnDesktopEntryRegistered(const base::FilePath& profile_path,
                                uint64_t generation);

  // Starts a new file operation for `profile_path` and returns its generation.
  // Only the reply of the latest operation for a profile updates its state.
  uint64_t StartOperation(const base::FilePath& profile_path);

  raw_ptr<ProfileManager> profile_manager_;

  // Sequence for file operations, so that writes and deletions of the same
  // desktop entry happen in order.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Generation of the latest file operation for each profile.
  base::flat_map<base::FilePath, uint64_t> generations_;

  // Icon referenced by each profile's installed desktop entry.
  base::flat_map<base::FilePath, base::FilePath> icon_paths_;

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      storage_observation_{this};

  base::WeakPtrFactory<ProfileShortcutManagerLinux> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_SHORTCUT_MANAGER_LINUX_H_
