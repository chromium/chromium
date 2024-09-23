// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/google/google_brand_code_map_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace google_brand {
namespace chromeos {

namespace {

// Path to file that stores the RLZ brand code on ChromeOS.
const base::FilePath::CharType kRLZBrandFilePath[] =
    FILE_PATH_LITERAL("/opt/oem/etc/BRAND_CODE");

bool IsBrandValid(std::string_view brand) {
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
void SetBrand(base::OnceClosure callback, const std::string& brand) {
  if (!IsBrandValid(brand))
    return;
  g_browser_process->local_state()->SetString(prefs::kRLZBrand, brand);
  std::move(callback).Run();
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
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  std::optional<policy::MarketSegment> market_segment;
  if (connector->IsDeviceEnterpriseManaged())
    market_segment = connector->GetEnterpriseMarketSegment();
  // The rlz brand code may change over time (e.g. when device goes from
  // unenrolled to enrolled status in OOBE). Prefer not to save it in pref to
  // avoid using outdated value.
  return std::string(GetRlzBrandCode(GetBrand(), market_segment));
}

void InitBrand(base::OnceClosure callback) {
  ::ash::system::StatisticsProvider* provider =
      ::ash::system::StatisticsProvider::GetInstance();
  const std::optional<std::string_view> brand =
      provider->GetMachineStatistic(::ash::system::kRlzBrandCodeKey);
  if (brand && IsBrandValid(brand.value())) {
    SetBrand(std::move(callback), std::string(brand.value()));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ReadBrandFromFile),
      base::BindOnce(&SetBrand, std::move(callback)));
}

}  // namespace chromeos
}  // namespace google_brand
