// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"

#include <string_view>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace guest_os {

namespace {

// This prefix is used as a prefix when generating shelf ids for windows we
// couldn't match to an app. It is also used for crostini web dialogs (e.g.
// crostini installer/upgrader) which need to appear in the shelf.
//
// Note: if the value is changed, you will also need to manually update
// kCrostiniInstallerShelfId and kCrostiniUpgraderShelfId.
constexpr char kCrostiniShelfIdPrefix[] = "crostini:";

// Prefix of the WindowAppId set on exo windows for GuestOS X apps.
constexpr char kGuestOsWindowAppIdPrefix[] = "org.chromium.guest_os.";
// This comes after kGuestOsWindowAppIdPrefix+token for GuestOS Wayland apps.
constexpr char kWaylandPrefix[] = "wayland.";
// This comes after kGuestOsWindowAppIdPrefix+token.
constexpr char kWmClassPrefix[] = "wmclass.";

// TODO(b/267377562): Borealis windows have a hardcoded "borealis" token.
constexpr char kBorealisToken[] = "borealis";

const std::string* GetAppNameForWMClass(std::string_view wmclass) {
  // A hard-coded mapping from WMClass to app names.
  // This is used to deal with the Linux apps that don't specify the correct
  // WMClass in their desktop files so that their aura windows can be identified
  // with their respective app IDs.
  static const base::NoDestructor<std::map<std::string, std::string>>
      kWMClassToNname({{"Octave-gui", "GNU Octave"},
                       {"MuseScore2", "MuseScore 2"},
                       {"XnViewMP", "XnView Multi Platform"}});
  const auto it = kWMClassToNname->find(std::string(wmclass));
  if (it == kWMClassToNname->end())
    return nullptr;
  return &it->second;
}

bool MatchingString(const std::string& search_string,
                    const std::string& value_string,
                    bool ignore_space) {
  std::string search = search_string;
  std::string value = value_string;
  if (ignore_space) {
    base::RemoveChars(search, " ", &search);
    base::RemoveChars(value, " ", &value);
  }
  return base::EqualsCaseInsensitiveASCII(search, value);
}

enum class FindAppIdResult { NoMatch, UniqueMatch, NonUniqueMatch };
// Looks for an app where prefs_key is set to search_value. Returns the apps id
// if there was only one app matching, otherwise returns an empty string.
FindAppIdResult FindAppId(const base::Value::Dict& prefs,
                          std::string_view prefs_key,
                          std::string_view search_value,
                          const std::optional<GuestId>& guest_id,
                          std::string* result,
                          bool require_startup_notify = false,
                          bool need_display = false,
                          bool ignore_space = false) {
  result->clear();
  for (const auto item : prefs) {
    if (require_startup_notify && !*item.second.GetDict().FindBool(
                                      guest_os::prefs::kAppStartupNotifyKey)) {
      continue;
    }

    if (need_display) {
      const std::optional<bool> no_display =
          item.second.GetDict().FindBool(guest_os::prefs::kAppNoDisplayKey);
      if (no_display && *no_display) {
        continue;
      }
    }

    // If guest_id is provided, also check that it matches. The guest_id is
    // considered matched if its vm_name and container_name matches
    // corresponding entries in the dictionary.
    if (guest_id && !MatchContainerDict(item.second, *guest_id)) {
      continue;
    }

    const base::Value* value = item.second.GetDict().Find(prefs_key);
    if (!value)
      continue;
    if (value->is_string()) {
      if (!MatchingString(std::string(search_value), value->GetString(),
                          ignore_space)) {
        continue;
      }
    } else if (value->is_dict()) {
      // Look at the unlocalized name to see if that matches.
      const std::string* str_value = value->GetDict().FindString("");
      if (!str_value || !MatchingString(std::string(search_value), *str_value,
                                        ignore_space)) {
        continue;
      }
    } else {
      continue;
    }

    if (!result->empty())
      return FindAppIdResult::NonUniqueMatch;
    *result = item.first;
  }

  if (!result->empty())
    return FindAppIdResult::UniqueMatch;
  return FindAppIdResult::NoMatch;
}

// For GuestOS |window_app_id|s which match the prefix of
// org.chromium.guest_os.<token>.*, return the guest token.
// The token should be one of the following:
// - For Crostini app windows: it is the container_token
// - For Bruschetta app windows: it is the container_token
// - For Borealis app windows: "borealis"
// - For all other guest app windows: "termina"
// Note that PluginVM does not match this prefix since it has a
// hard-coded window_app_id.
std::string GetGuestTokenForWindowId(const std::string* window_app_id) {
  if (!window_app_id ||
      !base::StartsWith(*window_app_id, kGuestOsWindowAppIdPrefix,
                        base::CompareCase::SENSITIVE)) {
    return std::string();
  }
  const auto token_start = strlen(kGuestOsWindowAppIdPrefix);
  // Find the first "." after the kGuestOsWindowAppIdPrefix
  const auto token_end = window_app_id->find(".", token_start);

  auto token = window_app_id->substr(token_start, token_end - token_start);

  return token;
}

std::string GetUnregisteredAppIdPrefix(const std::string& token) {
  if (token == kBorealisToken) {
    return borealis::kBorealisAnonymousPrefix;
  }

  // TODO(b/244651040): We should support other VMs, e.g. bruschetta.
  // For all other unregistered apps, default to "crostini:".
  return kCrostiniShelfIdPrefix;
}

}  // namespace

// The code follows these steps to identify apps and returns the first match:
// 1) If the |window_startup_id| is set, look for a matching desktop file id.
// 2) Ignore windows if the |window_app_id| is not set.
// 3) The |window_app_id| is prefixed by org.chromium.guest_os.<token>., so we
//    should be able to obtain a guest token from it. This will be used to find
//    a guest_id to which the app window belongs to. In the following steps, the
//    container_name and vm_name from the guest_id will be used to find a unique
//    match if available.
// 4) Remove the org.chromium.guest_os.<token>. prefix and use the remaining
//    string (the suffix) for the next steps.
// 5) If the suffix is prefixed by wayland., it's a native Wayland app. Look for
//    a matching desktop file id.
// 6) If the suffix from step 4 is prefixed by wmclass.:
// 6.1) Look for an app where StartupWMClass matches the remaining string.
// 6.2) Look for an app where the desktop file id matches the remaining string.
// 6.3) Look for an app where the unlocalized name matches the remaining
//      string. This handles the xterm & uxterm examples.
// 7) If we couldn't find a match, prefix the |window_app_id| with a generic
//    prefix of 'crostini:' or 'borealis:"', so we can easily identify
//    shelf entries as GuestOs apps. If we could not identify the VM, default
//    to using "crostini:".
std::string GetGuestOsShelfAppId(Profile* profile,
                                 const std::string* window_app_id,
                                 const std::string* window_startup_id) {
  if (!profile || !profile->GetPrefs())
    return std::string();

  const base::Value::Dict& apps =
      profile->GetPrefs()->GetDict(guest_os::prefs::kGuestOsRegistry);

  // TODO(b/244651040): Consider moving the borealis GetBorealisAppId logic
  // here.
  std::string app_id;

  std::string token = GetGuestTokenForWindowId(window_app_id);
  std::optional<GuestId> guest_id =
      GuestOsSessionTrackerFactory::GetForProfile(profile)->GetGuestIdForToken(
          token);

  if (window_startup_id) {
    if (FindAppId(apps, guest_os::prefs::kAppDesktopFileIdKey,
                  *window_startup_id, guest_id, &app_id,
                  true) == FindAppIdResult::UniqueMatch) {
      return app_id;
    }
    LOG(WARNING) << "Startup ID was set to '" << *window_startup_id
                 << "' but not matched. Will attempt to match with window ID.";
  }

  if (!window_app_id) {
    return std::string();
  }

  // If the window_id does not follow the expected format, return a generic id.
  if (!base::StartsWith(*window_app_id, kGuestOsWindowAppIdPrefix,
                        base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "window_app_id:" << *window_app_id
               << " provided is not prefixed with "
               << kGuestOsWindowAppIdPrefix;
    return GetUnregisteredAppIdPrefix(token) + *window_app_id;
  }

  // Get the suffix by stripping "org.chromium.guest_os.<token>.".
  // token.length() + 1 is used since the '.' separator was not included in the
  // token.
  std::string_view suffix = base::MakeStringPiece(
      window_app_id->begin() + strlen(kGuestOsWindowAppIdPrefix) +
          token.length() + 1,
      window_app_id->end());

  // Wayland apps will have a "wayland." identifier.
  if (base::StartsWith(suffix, kWaylandPrefix, base::CompareCase::SENSITIVE)) {
    const std::string_view wayland_app = suffix.substr(strlen(kWaylandPrefix));
    if (FindAppId(apps, guest_os::prefs::kAppDesktopFileIdKey, wayland_app,
                  guest_id, &app_id) == FindAppIdResult::UniqueMatch) {
      return app_id;
    }
    return GetUnregisteredAppIdPrefix(token) + *window_app_id;
  }

  // If we don't have an id to match to a desktop file, use the window app id.
  if (!base::StartsWith(suffix, kWmClassPrefix, base::CompareCase::SENSITIVE)) {
    return GetUnregisteredAppIdPrefix(token) + *window_app_id;
  }

  // If an app had StartupWMClass set to the given WM class, use that,
  // otherwise look for a desktop file id matching the WM class.
  std::string_view key = suffix.substr(strlen(kWmClassPrefix));
  FindAppIdResult result = FindAppId(
      apps, guest_os::prefs::kAppStartupWMClassKey, key, guest_id, &app_id,
      false /* require_startup_notification */, true /* need_display */);
  if (result == FindAppIdResult::UniqueMatch)
    return app_id;
  if (result == FindAppIdResult::NonUniqueMatch)
    return GetUnregisteredAppIdPrefix(token) + *window_app_id;

  if (FindAppId(apps, guest_os::prefs::kAppDesktopFileIdKey, key, guest_id,
                &app_id) == FindAppIdResult::UniqueMatch) {
    return app_id;
  }

  if (FindAppId(apps, guest_os::prefs::kAppNameKey, key, guest_id, &app_id,
                false /* require_startup_notification */,
                true /* need_display */,
                true /* ignore_space */) == FindAppIdResult::UniqueMatch) {
    return app_id;
  }

  const std::string* app_name = GetAppNameForWMClass(key);
  if (app_name &&
      FindAppId(apps, guest_os::prefs::kAppNameKey, *app_name, guest_id,
                &app_id, false /* require_startup_notification */,
                true /* need_display */) == FindAppIdResult::UniqueMatch) {
    return app_id;
  }

  return GetUnregisteredAppIdPrefix(token) + *window_app_id;
}

bool IsUnregisteredCrostiniShelfAppId(std::string_view shelf_app_id) {
  return base::StartsWith(shelf_app_id, kCrostiniShelfIdPrefix,
                          base::CompareCase::SENSITIVE);
}

bool IsUnregisteredGuestOsShelfAppId(std::string_view shelf_app_id) {
  return IsUnregisteredCrostiniShelfAppId(shelf_app_id) ||
         base::StartsWith(shelf_app_id, borealis::kBorealisAnonymousPrefix,
                          base::CompareCase::SENSITIVE);
}

bool IsCrostiniShelfAppId(const Profile* profile,
                          std::string_view shelf_app_id) {
  if (IsUnregisteredCrostiniShelfAppId(shelf_app_id)) {
    return true;
  }

  if (!profile || !profile->GetPrefs()) {
    return false;
  }
  // TODO(timloh): We need to handle desktop files that have been removed.
  // For example, running windows with a no-longer-valid app id will try to
  // use the ExtensionContextMenuModel.
  const auto& apps =
      profile->GetPrefs()->GetDict(guest_os::prefs::kGuestOsRegistry);
  return apps.contains(shelf_app_id);
}

apps::AppType GetAppType(Profile* profile, std::string_view shelf_app_id) {
  if (shelf_app_id.starts_with(kCrostiniShelfIdPrefix)) {
    shelf_app_id.remove_prefix(strlen(kCrostiniShelfIdPrefix));
  }
  const std::string id(shelf_app_id);
  const std::string token = GetGuestTokenForWindowId(&id);
  std::optional<GuestId> guest_id =
      GuestOsSessionTrackerFactory::GetForProfile(profile)->GetGuestIdForToken(
          token);
  if (guest_id.has_value()) {
    return ToAppType(guest_id->vm_type);
  }
  return ToAppType(vm_tools::apps::UNKNOWN);
}

}  // namespace guest_os
