// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/google/google_brand_code_map_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace google_brand {
namespace chromeos {

namespace {

// Path to file that stores the RLZ brand code on ChromeOS.
const base::FilePath::CharType kRLZBrandFilePath[] =
    FILE_PATH_LITERAL("/opt/oem/etc/BRAND_CODE");

bool IsBrandValid(const std::string& brand) {
  return !brand.empty();
}

// Reads the brand code from file |kRLZBrandFilePath|.
std::string ReadBrandFromFile() {
  std::string brand;
  base::FilePath brand_file_path(kRLZBrandFilePath);
  if (!base::ReadFileToString(brand_file_path, &brand))
    LOG(WARNING) << "Brand code file missing: " << brand_file_path.value();
  base::TrimWhitespaceASCII(brand, base::TRIM_ALL, &brand);
  return brand;
}

// For a valid |brand|, sets the brand code and runs |callback|.
void SetBrand(const base::Closure& callback, const std::string& brand) {
  if (!IsBrandValid(brand))
    return;
  g_browser_process->local_state()->SetString(prefs::kRLZBrand, brand);
  callback.Run();
}

// True if brand code has been cleared for the current session.
bool g_brand_empty = false;

}  // namespace

void ClearBrandForCurrentSession() {
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  g_brand_empty = true;
}

std::string GetBrand() {
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (g_brand_empty)
    return std::string();
  // Unit tests do not have prefs.
  if (!g_browser_process->local_state())
    return std::string();
  return g_browser_process->local_state()->GetString(prefs::kRLZBrand);
}

std::string GetRlzBrand() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  base::Optional<policy::MarketSegment> market_segment;
  if (connector->IsEnterpriseManaged())
    market_segment = connector->GetEnterpriseMarketSegment();
  // The rlz brand code may change over time (e.g. when device goes from
  // unenrolled to enrolled status in OOBE). Prefer not to save it in pref to
  // avoid using outdated value.
  return GetRlzBrandCode(GetBrand(), market_segment);
}

void InitBrand(const base::Closure& callback) {
  ::chromeos::system::StatisticsProvider* provider =
      ::chromeos::system::StatisticsProvider::GetInstance();
  std::string brand;
  const bool found = provider->GetMachineStatistic(
      ::chromeos::system::kRlzBrandCodeKey, &brand);
  if (found && IsBrandValid(brand)) {
    SetBrand(callback, brand);
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::Bind(&ReadBrandFromFile), base::Bind(&SetBrand, callback));
}

}  // namespace chromeos
}  // namespace google_brand
