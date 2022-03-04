// Copyright 2020 The Chromium Authors. All rights reserved.
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

namespace borealis {

const char kInstallerAppId[] = "dkecggknbdokeipkgnhifhiokailichf";
const char kClientAppId[] = "epfhbkiklgmlkhfpbcdleadnhcfdjfmo";
const char kIgnoredAppIdPrefix[] = "org.chromium.borealis.xid.";
const char kBorealisDlcName[] = "borealis-dlc";
const char kAllowedScheme[] = "c3RlYW0=";
const base::StringPiece kURLAllowlist[] = {"Ly9zdG9yZS8=", "Ly9ydW4v"};
// TODO(b/174282035): Potentially update regex when other strings
// are updated.
const char kBorealisAppIdRegex[] = "([^/]+\\d+)";
const char kProtonVersionGameMismatch[] = "UNKNOWN (GameID mismatch)";
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
static constexpr char kJSONPlatformKey[] = "platform_version";
static constexpr char kJSONProtonKey[] = "proton_version";
static constexpr char kJSONSpecsKey[] = "specs";
static constexpr char kJSONSteamKey[] = "steam_runtime_version";

// App IDs prefixed with this are identified with a numeric "Borealis ID".
const base::StringPiece kBorealisWindowWithIdPrefix(
    "org.chromium.borealis.xprop.");

// Windows with these app IDs are not games. Don't prompt for feedback for them.
// Hashed by crx_file::id_util::GenerateId().
static constexpr char kNonGameIdHash1[] = "hnfpbccfbbbjkmcalgjofgokpgjjppon";
static constexpr char kNonGameIdHash2[] = "kooplpnkalpdpoohnhmlmfebokjkgnlb";
static constexpr char kNonGameIdHash3[] = "bmhgcnboebpgmobfgfjcfplecleopefa";

GURL GetSysInfoForUrlAsync(GURL url,
                           absl::optional<int> game_id,
                           std::string owner_id) {
  base::DictionaryValue json_root;

  if (game_id.has_value()) {
    json_root.SetString(kJSONAppIdKey,
                        base::StringPrintf("%d", game_id.value()));
  }

  json_root.SetString(kJSONBoardKey, base::SysInfo::HardwareModelName());
  json_root.SetString(
      kJSONSpecsKey,
      base::StringPrintf("%ldGB; %s",
                         (long)(base::SysInfo::AmountOfPhysicalMemory() /
                                (1000 * 1000 * 1000)),
                         base::SysInfo::CPUModelName().c_str()));
  json_root.SetString(kJSONPlatformKey,
                      base::SysInfo::OperatingSystemVersion());

  borealis::ProtonVersionInfo version_info;
  std::string output;
  if (borealis::GetProtonVersionInfo(owner_id, &output)) {
    version_info = borealis::ParseProtonVersionInfo(game_id, output);
  } else {
    LOG(WARNING) << "Failed to run get_proton_version.py:";
    LOG(WARNING) << output;
  }
  json_root.SetString(kJSONProtonKey, version_info.proton);
  json_root.SetString(kJSONSteamKey, version_info.slr);

  std::string json_string;
  base::JSONWriter::Write(json_root, &json_string);
  url = net::AppendQueryParameter(url, kDeviceInformationKey, json_string);

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

  std::string hash = crx_file::id_util::GenerateId(app_id);
  if (hash == kNonGameIdHash1 || hash == kNonGameIdHash2 ||
      hash == kNonGameIdHash3) {
    std::move(url_callback).Run(GURL());
    return;
  }

  GURL url(kFeedbackUrl);
  url = net::AppendQueryParameter(url, kAppNameKey, window_title);

  // Attempt to get the Borealis app ID.
  // TODO(b/173977876): Implement this in a more reliable way.
  absl::optional<int> borealis_app_id;
  absl::optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (registration.has_value()) {
    borealis_app_id = GetBorealisAppId(registration->Exec());
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&GetSysInfoForUrlAsync, url, borealis_app_id,
                     ash::ProfileHelper::GetUserIdHashFromProfile(profile)),
      std::move(url_callback));
}

bool IsExternalURLAllowed(const GURL& url) {
  std::string decoded_scheme;
  if (!base::Base64Decode(kAllowedScheme, &decoded_scheme)) {
    LOG(ERROR) << "Failed to decode allowed scheme (" << kAllowedScheme << ")";
    return false;
  }
  if (url.scheme() != decoded_scheme) {
    return false;
  }
  for (auto& allowed_url : kURLAllowlist) {
    std::string decoded_url;
    if (!base::Base64Decode(allowed_url, &decoded_url)) {
      LOG(ERROR) << "Failed to decode allowed url (" << allowed_url << ")";
      continue;
    }
    if (base::StartsWith(url.GetContent(), decoded_url)) {
      return true;
    }
  }
  return false;
}

bool GetProtonVersionInfo(const std::string& owner_id, std::string* output) {
  std::vector<std::string> command = {"/usr/bin/vsh", "--owner_id=" + owner_id,
                                      "--vm_name=borealis", "--",
                                      "/usr/bin/get_proton_version.py"};
  bool success = base::GetAppOutput(command, output);
  if (!success) {
    // Re-run with stderr capture. It is not done initially since
    // GetAppOutputAndError intermixes stdout and stderr and stderr commonly
    // includes informational messages about Linux game sessions that would
    // complicate the parsing of `output`.
    base::GetAppOutputAndError(command, output);
  }
  return success;
}

ProtonVersionInfo ParseProtonVersionInfo(absl::optional<int> game_id,
                                         const std::string& output) {
  // Expected stdout of get_proton_version.py:
  // GameID: <game_id>, Proton:<proton_version>, SLR: <slr_version>, Timestamp: <timestamp>
  // GameID: <game_id>, Proton:<proton_version>, SLR: <slr_version>, Timestamp: <timestamp>
  // ...

  // Only grab the first line, which is for the last game played.
  std::string raw_info = output.substr(0, output.find("\n"));

  ProtonVersionInfo version_info;
  std::string parsed_game_id;
  base::StringPairs tokenized_info;
  base::SplitStringIntoKeyValuePairs(raw_info, ':', ',', &tokenized_info);
  for (const auto& key_val_pair : tokenized_info) {
    std::string key;
    TrimWhitespaceASCII(key_val_pair.first, base::TRIM_ALL, &key);

    std::string val;
    TrimWhitespaceASCII(key_val_pair.second, base::TRIM_ALL, &val);

    if (key == "GameID") {
      parsed_game_id = val;
    } else if (key == "Proton") {
      version_info.proton = val;
    } else if (key == "SLR") {
      version_info.slr = val;
    }
  }

  // If the app id is known and doesn't match, return the version "UNKNOWN"
  if (game_id.has_value() && !parsed_game_id.empty() &&
      parsed_game_id != base::NumberToString(game_id.value())) {
    LOG(WARNING) << "Expected GameID " << game_id.value() << " got "
                 << parsed_game_id;
    version_info.proton = kProtonVersionGameMismatch;
    version_info.slr = kProtonVersionGameMismatch;
  } else if (!parsed_game_id.empty()) {
    if (version_info.proton.empty()) {
      LOG(WARNING) << "Found an unexpected empty Proton version.";
    }
    if (version_info.slr.empty()) {
      LOG(WARNING) << "Found an unexpected empty SLR version.";
    }
  }

  return version_info;
}

void OnGetDlcState(base::OnceCallback<void(const std::string& path)> callback,
                   const std::string& err,
                   const dlcservice::DlcState& dlc_state) {
  if (err != dlcservice::kErrorNone) {
    LOG(ERROR) << "Failed to get dlc state with error: " << err;
  }

  // TODO(b/220799106): Add user visible error.
  if (!dlc_state.INSTALLED) {
    LOG(ERROR) << "Borealis dlc is not installed";
    return;
  }
  // std::string path = dlc_state.root_path().get*;
  std::move(callback).Run(dlc_state.root_path());
}

void GetDlcPath(base::OnceCallback<void(const std::string& path)> callback) {
  chromeos::DlcserviceClient::Get()->GetDlcState(
      kBorealisDlcName, base::BindOnce(&OnGetDlcState, std::move(callback)));
}

}  // namespace borealis
