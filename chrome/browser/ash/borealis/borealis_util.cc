// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_util.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/crx_file/id_util.h"
#include "components/exo/shell_surface_util.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/display/screen.h"

namespace borealis {

const char kInstallerAppId[] = "dkecggknbdokeipkgnhifhiokailichf";
const char kClientAppId[] = "epfhbkiklgmlkhfpbcdleadnhcfdjfmo";
const char kLauncherSearchAppId[] = "ceoplblcdaffnnflkkcagjpomjgedmdl";
const char kIgnoredAppIdPrefix[] = "org.chromium.guest_os.borealis.xid.";
const char kBorealisDlcName[] = "borealis-dlc";
const char kAllowedScheme[] = "steam";
const base::StringPiece kURLAllowlist[] = {"//store/", "//run/"};
const char kBorealisAppIdRegex[] = "(?:steam:\\/\\/rungameid\\/)(\\d+)";
const char kCompatToolVersionGameMismatch[] = "UNKNOWN (GameID mismatch)";
const char kDeviceInformationKey[] = "entry.1613887985";

const char kInsertCoinSuccessMessage[] = "Success";
const char kInsertCoinRejectMessage[] = "Coin Invalid";

namespace {

// Base feedback form URL, without query parameters for prefilling.
static constexpr char kFeedbackUrl[] =
    "https://docs.google.com/forms/d/e/"
    "1FAIpQLScGvT2BIwYJe9g15OINX2pvw6TgK8e2ihvSq3hHZudAneRmuA/"
    "viewform?usp=pp_url";
// Query parameter keys for prefilling form data.
static constexpr char kAppNameKey[] = "entry.504707995";
// JSON keys for prefilling JSON section.
static constexpr char kJSONAppIdKey[] = "steam_appid";
static constexpr char kJSONBoardKey[] = "board";
static constexpr char kJSONMonitorsExternal[] = "external_monitors";
static constexpr char kJSONMonitorsInternal[] = "internal_monitors";
static constexpr char kJSONPlatformKey[] = "platform_version";
static constexpr char kJSONProtonKey[] = "proton_version";
static constexpr char kJSONSpecsKey[] = "specs";
static constexpr char kJSONSteamKey[] = "steam_runtime_version";

// App IDs prefixed with this are identified with a numeric "Borealis ID".
const base::StringPiece kBorealisWindowWithIdPrefix(
    "org.chromium.guest_os.borealis.xprop.");

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

GURL AssembleUrlAsync(std::string owner_id,
                      absl::optional<int> game_id,
                      std::string window_title) {
  GURL url(kFeedbackUrl);
  url = net::AppendQueryParameter(url, kAppNameKey, window_title);

  base::Value::Dict json_root;

  // System specs
  json_root.Set(kJSONBoardKey, base::SysInfo::HardwareModelName());
  json_root.Set(
      kJSONSpecsKey,
      base::StringPrintf("%ldGB; %s",
                         (long)(base::SysInfo::AmountOfPhysicalMemory() /
                                (1000 * 1000 * 1000)),
                         base::SysInfo::CPUModelName().c_str()));
  json_root.Set(kJSONPlatformKey, base::SysInfo::OperatingSystemVersion());

  // Number of monitors
  int internal_displays = 0;
  int external_displays = 0;
  for (const display::Display& d :
       display::Screen::GetScreen()->GetAllDisplays()) {
    if (d.IsInternal()) {
      internal_displays++;
    } else {
      external_displays++;
    }
  }
  json_root.Set(kJSONMonitorsInternal, internal_displays);
  json_root.Set(kJSONMonitorsExternal, external_displays);

  // Proton/SLR versions
  borealis::CompatToolInfo compat_tool_info;
  std::string output;
  if (borealis::GetCompatToolInfo(owner_id, &output)) {
    compat_tool_info = borealis::ParseCompatToolInfo(game_id, output);
  } else {
    LOG(WARNING) << "Failed to get compat tool version info:";
    LOG(WARNING) << output;
  }
  json_root.Set(kJSONProtonKey, compat_tool_info.proton);
  json_root.Set(kJSONSteamKey, compat_tool_info.slr);

  // Steam GameID
  if (!game_id.has_value() && compat_tool_info.game_id.has_value()) {
    game_id = compat_tool_info.game_id.value();
  }
  if (game_id.has_value()) {
    json_root.Set(kJSONAppIdKey, base::StringPrintf("%d", game_id.value()));
  }

  std::string device_info;
  base::JSONWriter::Write(json_root, &device_info);
  url = net::AppendQueryParameter(url, kDeviceInformationKey, device_info);
  return url;
}

}  // namespace

absl::optional<int> GetBorealisAppId(std::string exec) {
  int app_id;
  if (RE2::PartialMatch(exec, kBorealisAppIdRegex, &app_id)) {
    return app_id;
  } else {
    return absl::nullopt;
  }
}

absl::optional<int> GetBorealisAppId(const aura::Window* window) {
  const std::string* id = exo::GetShellApplicationId(window);
  if (id && base::StartsWith(*id, kBorealisWindowWithIdPrefix)) {
    int borealis_id;
    if (base::StringToInt(id->substr(kBorealisWindowWithIdPrefix.size()),
                          &borealis_id)) {
      return borealis_id;
    }
  }
  return absl::nullopt;
}

void FeedbackFormUrl(Profile* const profile,
                     const std::string& app_id,
                     const std::string& window_title,
                     base::OnceCallback<void(GURL)> url_callback) {
  const guest_os::GuestOsRegistryService* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);

  // Exclude windows that aren't games.
  if (app_id.find(kIgnoredAppIdPrefix) != std::string::npos ||
      app_id == kClientAppId) {
    std::move(url_callback).Run(GURL());
    return;
  }

  if (app_id == kZenityId || app_id == kSteamClientId ||
      app_id == kSteamBigPictureId) {
    std::move(url_callback).Run(GURL());
    return;
  }

  // Attempt to get the Borealis app ID.
  // TODO(b/173977876): Implement this in a more reliable way.
  absl::optional<int> game_id;
  absl::optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (registration.has_value()) {
    game_id = GetBorealisAppId(registration->Exec());
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&AssembleUrlAsync,
                     ash::ProfileHelper::GetUserIdHashFromProfile(profile),
                     std::move(game_id), std::move(window_title)),
      std::move(url_callback));
}

bool IsExternalURLAllowed(const GURL& url) {
  if (url.scheme() != kAllowedScheme) {
    return false;
  }
  for (auto& allowed_url : kURLAllowlist) {
    if (base::StartsWith(url.GetContent(), allowed_url)) {
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

CompatToolInfo ParseCompatToolInfo(absl::optional<int> game_id,
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
