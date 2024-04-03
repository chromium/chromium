// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_metrics_win.h"

#include <string>

#include "base/base64.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version_info/channel.h"
#include "base/win/com_init_util.h"
#include "base/win/windows_types.h"
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "chrome/common/channel_info.h"
#include "components/crash/core/common/crash_key.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace os_crypt {

namespace prefs {

// Pref name changed 03/2024 to reset metrics for a new version of the app-bound
// encryption service.
const char kOsCryptAppBoundFixedData3PrefName[] =
    "os_crypt.app_bound_fixed_data3";

}  // namespace prefs

namespace {

namespace features {
// Emergency 'off-switch' just in case a ton of these log entries are created.
// Current metrics show that fewer than 0.1% of clients should emit a log
// though.
BASE_FEATURE(kAppBoundEncryptionMetricsExtendedLogs,
             "AppBoundEncryptionMetricsExtendedLogs",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

// Rather than generate a random key here, use fixed data here for the purposes
// of measuring the performance, as the content itself does not matter.
const char kFixedData[] = "Fixed data used for metrics";

void DecryptAndRecordMetricsOnCOMThread(const std::string& encrypted_data) {
  base::win::AssertComInitialized();

  std::string decrypted_data;
  DWORD last_error;
  HRESULT hr;
  std::string log_message;
  {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "OSCrypt.AppBoundEncryption.PathValidation.Decrypt.Time2");
    hr = DecryptAppBoundString(encrypted_data, decrypted_data, last_error,
                               &log_message);
  }

  if (FAILED(hr)) {
    base::UmaHistogramSparse(
        "OSCrypt.AppBoundEncryption.PathValidation.Decrypt.ResultLastError2",
        last_error);
    // Only log this extended data on Dev channel.
    if (!log_message.empty() &&
        chrome::GetChannel() == version_info::Channel::DEV &&
        base::FeatureList::IsEnabled(
            features::kAppBoundEncryptionMetricsExtendedLogs)) {
      // Log message is two paths and some linking text totalling fewer than 25
      // characters.
      static crash_reporter::CrashKeyString<(MAX_PATH * 2) + 25>
          app_bound_log_message("app_bound_log");
      app_bound_log_message.Set(log_message);
      base::debug::DumpWithoutCrashing();
    }
  } else {
    // Check if it returned success but the data was invalid. This should never
    // happen. If it does, log a unique HRESULT to track it.
    if (decrypted_data != kFixedData) {
      const HRESULT kErrorWrongData =
          MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA101);
      hr = kErrorWrongData;
    }
  }

  base::UmaHistogramSparse(
      "OSCrypt.AppBoundEncryption.PathValidation.Decrypt.ResultCode2", hr);
}

std::string EncryptAndRecordMetricsOnCOMThread() {
  base::win::AssertComInitialized();

  std::string encrypted_data;
  DWORD last_error;
  HRESULT hr;
  {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "OSCrypt.AppBoundEncryption.PathValidation.Encrypt.Time2");
    hr = EncryptAppBoundString(ProtectionLevel::PROTECTION_PATH_VALIDATION,
                               kFixedData, encrypted_data, last_error);
  }

  base::UmaHistogramSparse(
      "OSCrypt.AppBoundEncryption.PathValidation.Encrypt.ResultCode2", hr);

  if (FAILED(hr)) {
    base::UmaHistogramSparse(
        "OSCrypt.AppBoundEncryption.PathValidation.Encrypt.ResultLastError2",
        last_error);
  }

  return encrypted_data;
}

void StorePrefOnUiThread(PrefService* local_state,
                         const std::string& encrypted_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (encrypted_data.empty())
    return;
  std::string base64_data = base::Base64Encode(encrypted_data);

  local_state->SetString(prefs::kOsCryptAppBoundFixedData3PrefName,
                         base64_data);
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kOsCryptAppBoundFixedData3PrefName, {});
}

bool MeasureAppBoundEncryptionStatus(PrefService* local_state,
                                     bool record_full_metrics) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto support = GetAppBoundEncryptionSupportLevel(local_state);

  base::UmaHistogramEnumeration("OSCrypt.AppBoundEncryption.SupportLevel",
                                support);

  if (support == SupportLevel::kNotSystemLevel) {
    // No service. No App-Bound APIs are available.
    return true;
  }

  // Only record separate timing metrics if the App-Bound provider is not,
  // itself, recording these metrics separately. This ensures the metrics
  // accurately reflect final client behavior.
  if (!record_full_metrics) {
    return true;
  }

  auto com_runner = base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);

  if (local_state->HasPrefPath(prefs::kOsCryptAppBoundFixedData3PrefName)) {
    const std::string base64_encrypted_data =
        local_state->GetString(prefs::kOsCryptAppBoundFixedData3PrefName);

    std::string encrypted_data;
    // If this fails it will be caught later when trying to decrypt and logged
    // above..
    std::ignore = base::Base64Decode(base64_encrypted_data, &encrypted_data);

    // Gather metrics for decrypt.
    return com_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&DecryptAndRecordMetricsOnCOMThread, encrypted_data));
  }

  if (support != SupportLevel::kSupported) {
    // Do not support encrypt of any new data if running on an unsupported
    // platform.
    return true;
  }

  return com_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&EncryptAndRecordMetricsOnCOMThread),
      base::BindOnce(StorePrefOnUiThread, local_state));
}

}  // namespace os_crypt
