// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_util.h"

#include <unordered_set>

#include "base/base64.h"
#include "base/no_destructor.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "components/crx_file/id_util.h"
#include "components/exo/shell_surface_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace borealis {

const char kInstallerAppId[] = "dkecggknbdokeipkgnhifhiokailichf";
const char kClientAppId[] = "epfhbkiklgmlkhfpbcdleadnhcfdjfmo";
const char kLauncherSearchAppId[] = "ceoplblcdaffnnflkkcagjpomjgedmdl";
const char kIgnoredAppIdPrefix[] = "org.chromium.guest_os.borealis.xid.";
const char kBorealisDlcName[] = "borealis-dlc";
const char kAllowedScheme[] = "steam";
const re2::LazyRE2 kURLAllowlistRegex[] = {
    {"//store/[0-9]{1,32}"},
    {"//run/[0-9]{1,32}"},
    {"//subscriptioninstall/[0-9]{1,32}"},
    {"//launch/[0-9]{1,32}/Dialog"}};
const char kCompatToolVersionGameMismatch[] = "UNKNOWN (GameID mismatch)";
const char kDeviceInformationKey[] = "entry.1613887985";

namespace {
// Windows with these app IDs are not games. Don't prompt for feedback for them.
//
// Some Steam updater windows use Zenity to show dialog boxes, and use its
// default WMClass.
static constexpr char kZenityId[] =
    "borealis_anon:org.chromium.guest_os.borealis.wmclass.Zenity";
// The Steam client is not a game.
static constexpr char kSteamClientId[] =
    "borealis_anon:org.chromium.guest_os.borealis.wmclass.steam";
// 769 is the Steam App ID assigned to the Steam Big Picture client as of 2023.
static constexpr char kSteamBigPictureId[] =
    "borealis_anon:org.chromium.guest_os.borealis.xprop.769";

// The regex used for extracting the "steam game id" of a .desktop's "Exec="
// field.
const re2::LazyRE2 kSteamGameIdFromExecRegex = {
    "steam:\\/\\/rungameid\\/(\\d+)"};
// The regex used for extracting the "steam game id" of a borealis window (or
// anonymous app).
const re2::LazyRE2 kSteamGameIdFromWindowRegex = {
    "org\\.chromium\\.guest_os\\.borealis\\.xprop\\.(\\d+)"};

// Works for window-data either in the exo_id form, or the anonymous app_id
// form.
std::optional<int> ParseGameIdFromWindowData(const std::string& data) {
  int app_id;
  if (RE2::PartialMatch(data, *kSteamGameIdFromWindowRegex, &app_id)) {
    return app_id;
  }
  return std::nullopt;
}

// Regexes attempting to match known and potential future Proton/SLR versions,
// used to prevent these tools showing up in the launcher.
// These are intended to be as future-proof as practical without matching too
// broadly. In particular, there are actual games named "Proton <Word>".
const re2::LazyRE2 kSpuriousGameBlocklist[] = {
    {"Proton [0-9.]+"},
    {"Proton BattlEye Runtime"},
    {"Proton EasyAntiCheat Runtime"},
    {"Proton Experimental"},
    {"Proton Hotfix"},
    {"Proton Next"},
    {"Steam Linux Runtime.*"},
};

// Additionally block non-games by Steam App ID, in case the tool names are
// changed. This is not future-proof, but provides an additional layer of
// defence for known tools.
bool IsSteamTool(int id) {
  static const base::NoDestructor<std::unordered_set<int>> kSteamToolIds({
      1070560,  // Steam Linux Runtime 1.0 (scout)
      1391110,  // Steam Linux Runtime 2.0 (soldier)
      1628350,  // Steam Linux Runtime 3.0 (sniper)
      858280,   // Proton 3.7
      930400,   // Proton 3.7 Beta
      961940,   // Proton 3.16
      996510,   // Proton 3.16 Beta
      1054830,  // Proton 4.2
      1113280,  // Proton 4.11
      1161040,  // Proton BattlEye Runtime
      1245040,  // Proton 5.0
      1420170,  // Proton 5.13
      1493710,  // Proton Experimental
      1580130,  // Proton 6.3
      1826330,  // Proton EasyAntiCheat Runtime
      1887720,  // Proton 7.0
      2180100,  // Proton Hotfix
      2230260,  // Proton Next
      2348590,  // Proton 8.0
  });
  return kSteamToolIds->contains(id);
}

}  // namespace

std::optional<int> ParseSteamGameId(std::string exec) {
  int app_id;
  if (RE2::PartialMatch(exec, *kSteamGameIdFromExecRegex, &app_id)) {
    return app_id;
  }
  return std::nullopt;
}

std::optional<int> SteamGameId(const aura::Window* window) {
  const std::string* id = exo::GetShellApplicationId(window);
  if (!id) {
    return std::nullopt;
  }
  return ParseGameIdFromWindowData(*id);
}

std::optional<int> SteamGameId(Profile* profile, const std::string& app_id) {
  if (BorealisWindowManager::IsAnonymousAppId(app_id)) {
    return ParseGameIdFromWindowData(app_id);
  }
  guest_os::GuestOsRegistryService* registry =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  if (!registry) {
    return std::nullopt;
  }
  std::optional<guest_os::GuestOsRegistryService::Registration> reg =
      registry->GetRegistration(app_id);
  if (!reg) {
    return std::nullopt;
  }
  return ParseSteamGameId(reg->Exec());
}

bool IsNonGameBorealisApp(const std::string& app_id) {
  if (app_id.find(kIgnoredAppIdPrefix) != std::string::npos ||
      app_id == kClientAppId) {
    return true;
  }

  if (app_id == kZenityId || app_id == kSteamClientId ||
      app_id == kSteamBigPictureId) {
    return true;
  }
  return false;
}

bool ShouldHideIrrelevantApp(
    const guest_os::GuestOsRegistryService::Registration& registration) {
  std::optional<int> id = ParseSteamGameId(registration.Exec());
  if (id && IsSteamTool(id.value())) {
    return true;
  }
  for (auto& blocklist_regex : kSpuriousGameBlocklist) {
    if (re2::RE2::FullMatch(registration.Name(), *blocklist_regex)) {
      return true;
    }
  }
  return false;
}

bool IsExternalURLAllowed(const GURL& url) {
  if (url.scheme() != kAllowedScheme) {
    return false;
  }
  for (auto& allowed_url : kURLAllowlistRegex) {
    if (re2::RE2::FullMatch(url.GetContent(), *allowed_url)) {
      return true;
    }
  }
  return false;
}

bool GetCompatToolInfo(const std::string& owner_id, std::string* output) {
  std::vector<std::string> command = {"/usr/bin/vsh", "--owner_id=" + owner_id,
                                      "--vm_name=borealis", "--",
                                      "/usr/bin/get_compat_tool_versions.py"};
  return base::GetAppOutputAndError(command, output);
}

CompatToolInfo ParseCompatToolInfo(std::optional<int> game_id,
                                   const std::string& output) {
  // Expected stdout of get_compat_tool_versions.py:
  // GameID: <game_id>, Proton:<proton_version>, SLR: <slr_version>, Timestamp: <timestamp>
  // GameID: <game_id>, Proton:<proton_version>, SLR: <slr_version>, Timestamp: <timestamp>
  // ...

  // Only grab the first line, which is for the last game played.
  std::string raw_info = output.substr(0, output.find("\n"));

  CompatToolInfo compat_tool_info;
  base::StringPairs tokenized_info;
  base::SplitStringIntoKeyValuePairs(raw_info, ':', ',', &tokenized_info);
  for (const auto& key_val_pair : tokenized_info) {
    std::string key;
    TrimWhitespaceASCII(key_val_pair.first, base::TRIM_ALL, &key);

    std::string val;
    TrimWhitespaceASCII(key_val_pair.second, base::TRIM_ALL, &val);

    if (key == "GameID") {
      int parsed_val;
      bool ret = base::StringToInt(val, &parsed_val);
      if (ret) {
        compat_tool_info.game_id = parsed_val;
      }
    } else if (key == "Proton") {
      compat_tool_info.proton = val;
    } else if (key == "SLR") {
      compat_tool_info.slr = val;
    }
  }

  // If the app id is known and doesn't match, return the version "UNKNOWN"
  if (game_id.has_value() && compat_tool_info.game_id.has_value() &&
      game_id.value() != compat_tool_info.game_id.value()) {
    LOG(WARNING) << "Expected GameID " << game_id.value() << " got "
                 << compat_tool_info.game_id.value();
    compat_tool_info.proton = kCompatToolVersionGameMismatch;
    compat_tool_info.slr = kCompatToolVersionGameMismatch;
  }

  return compat_tool_info;
}

}  // namespace borealis
