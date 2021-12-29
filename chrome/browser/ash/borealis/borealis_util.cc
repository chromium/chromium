// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_util.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
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

namespace {

// Base feedback form URL, without query parameters for prefilling.
static constexpr char kFeedbackUrl[] =
    "https://docs.google.com/forms/d/e/"
    "1FAIpQLSfyI7K7xV3pKiQeJ-yZlep4XI8ZY9bbr7D33LY2jm4Zoda1cg/"
    "viewform?usp=pp_url";

// Query parameter keys for prefilling form data.
static constexpr char kAppNameKey[] = "entry.1661950665";
static constexpr char kBoardKey[] = "entry.2066138756";
static constexpr char kSpecsKey[] = "entry.1341753442";
static constexpr char kPlatformVersionKey[] = "entry.1193918294";
static constexpr char kAppIdKey[] = "entry.2112096055";
static constexpr char kProtonVersionKey[] = "entry.810819520";
static constexpr char kSlrVersionKey[] = "entry.300735843";

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
  url = net::AppendQueryParameter(url, kBoardKey,
                                  base::SysInfo::HardwareModelName());
  url = net::AppendQueryParameter(
      url, kSpecsKey,
      base::StringPrintf("%ldGB; %s",
                         (long)(base::SysInfo::AmountOfPhysicalMemory() /
                                (1000 * 1000 * 1000)),
                         base::SysInfo::CPUModelName().c_str()));
  url = net::AppendQueryParameter(url, kPlatformVersionKey,
                                  base::SysInfo::OperatingSystemVersion());

  borealis::ProtonVersionInfo version_info = {};
  std::string output;
  if (borealis::GetProtonVersionInfo(owner_id, &output)) {
    version_info = borealis::ParseProtonVersionInfo(game_id, output);
  } else {
    LOG(WARNING) << "Failed to run get_proton_version.py:";
    LOG(WARNING) << output;
  }
  if (!version_info.proton.empty()) {
    url =
        net::AppendQueryParameter(url, kProtonVersionKey, version_info.proton);
  }
  if (!version_info.slr.empty()) {
    url = net::AppendQueryParameter(url, kSlrVersionKey, version_info.slr);
  }

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
    if (borealis_app_id.has_value()) {
      url = net::AppendQueryParameter(
          url, kAppIdKey, base::StringPrintf("%d", borealis_app_id.value()));
    }
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

}  // namespace borealis
