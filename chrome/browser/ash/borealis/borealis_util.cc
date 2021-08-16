// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_util.h"

#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "components/crx_file/id_util.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace borealis {

const char kBorealisAppId[] = "dkecggknbdokeipkgnhifhiokailichf";
const char kBorealisMainAppId[] = "epfhbkiklgmlkhfpbcdleadnhcfdjfmo";
const char kBorealisDlcName[] = "borealis-dlc";
// TODO(b/174282035): Potentially update regex when other strings
// are updated.
const char kBorealisAppIdRegex[] = "([^/]+\\d+)";

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

// App IDs containing this are unidentified and probably aren't games.
// Don't prompt for feedback for them.
static constexpr char kNonGameWindowPrefix[] = "org.chromium.borealis.xid.";

// Windows with these app IDs are not games. Don't prompt for feedback for them.
// Hashed by crx_file::id_util::GenerateId().
static constexpr char kNonGameIdHash1[] = "hnfpbccfbbbjkmcalgjofgokpgjjppon";
static constexpr char kNonGameIdHash2[] = "kooplpnkalpdpoohnhmlmfebokjkgnlb";
static constexpr char kNonGameIdHash3[] = "bmhgcnboebpgmobfgfjcfplecleopefa";

absl::optional<int> GetBorealisAppId(std::string exec) {
  int app_id;
  if (RE2::PartialMatch(exec, kBorealisAppIdRegex, &app_id)) {
    return app_id;
  } else {
    return absl::nullopt;
  }
}

namespace {
GURL GetSysInfoForUrlAsync(GURL url) {
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

  return url;
}
}  // namespace

void FeedbackFormUrl(const guest_os::GuestOsRegistryService* registry_service,
                     const std::string& app_id,
                     const std::string& window_title,
                     base::OnceCallback<void(GURL)> url_callback) {
  // Exclude windows that aren't games.
  if (app_id.find(kNonGameWindowPrefix) != std::string::npos ||
      app_id == kBorealisMainAppId) {
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
  absl::optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (registration.has_value()) {
    absl::optional<int> app_id = GetBorealisAppId(registration->Exec());
    if (app_id.has_value()) {
      url = net::AppendQueryParameter(url, kAppIdKey,
                                      base::StringPrintf("%d", app_id.value()));
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(), base::BindOnce(&GetSysInfoForUrlAsync, url),
      std::move(url_callback));
}

}  // namespace borealis
