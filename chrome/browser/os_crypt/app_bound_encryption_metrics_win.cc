// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_metrics_win.h"

#include <string>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/com_init_util.h"
#include "base/win/windows_types.h"
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace os_crypt {

namespace prefs {

const char kOsCryptAppBoundFixedDataPrefName[] =
    "os_crypt.app_bound_fixed_data";

}  // namespace prefs

namespace {

// Rather than generate a random key here, use fixed data here for the purposes
// of measuring the performance, as the content itself does not matter.
const char kFixedData[] = "Fixed data used for metrics";

void DecryptAndRecordMetricsOnCOMThread(const std::string& encrypted_data) {
  base::win::AssertComInitialized();

  std::string decrypted_data;
  DWORD last_error;
  HRESULT hr;
  {
    SCOPED_UMA_HISTOGRAM_TIMER("OSCrypt.AppBoundEncryption.Decrypt.Time");
    hr = DecryptAppBoundString(encrypted_data, decrypted_data, last_error);
  }

  if (FAILED(hr)) {
    base::UmaHistogramSparse(
        "OSCrypt.AppBoundEncryption.Decrypt.ResultLastError", last_error);
  } else {
    // Check if it returned success but the data was invalid. This should never
    // happen. If it does, log a unique HRESULT to track it.
    if (decrypted_data != kFixedData) {
      const HRESULT kErrorWrongData =
          MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA101);
      hr = kErrorWrongData;
    }
  }

  base::UmaHistogramSparse("OSCrypt.AppBoundEncryption.Decrypt.ResultCode", hr);
}

std::string EncryptAndRecordMetricsOnCOMThread() {
  base::win::AssertComInitialized();

  std::string encrypted_data;
  DWORD last_error;
  HRESULT hr;
  {
    SCOPED_UMA_HISTOGRAM_TIMER("OSCrypt.AppBoundEncryption.Encrypt.Time");
    hr = EncryptAppBoundString(ProtectionLevel::PATH_VALIDATION, kFixedData,
                               encrypted_data, last_error);
  }

  base::UmaHistogramSparse("OSCrypt.AppBoundEncryption.Encrypt.ResultCode", hr);

  if (FAILED(hr)) {
    base::UmaHistogramSparse(
        "OSCrypt.AppBoundEncryption.Encrypt.ResultLastError", last_error);
  }

  return encrypted_data;
}

void StorePrefOnUiThread(PrefService* local_state,
                         const std::string& encrypted_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (encrypted_data.empty())
    return;
  std::string base64_data;
  base::Base64Encode(encrypted_data, &base64_data);

  local_state->SetString(prefs::kOsCryptAppBoundFixedDataPrefName, base64_data);
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kOsCryptAppBoundFixedDataPrefName, {});
}

bool MeasureAppBoundEncryptionStatus(PrefService* local_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto com_runner = base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);

  if (local_state->HasPrefPath(prefs::kOsCryptAppBoundFixedDataPrefName)) {
    const std::string base64_encrypted_data =
        local_state->GetString(prefs::kOsCryptAppBoundFixedDataPrefName);

    std::string encrypted_data;
    // If this fails it will be caught later when trying to decrypt and logged
    // above..
    std::ignore = base::Base64Decode(base64_encrypted_data, &encrypted_data);

    // Gather metrics for decrypt.
    return com_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&DecryptAndRecordMetricsOnCOMThread, encrypted_data));
  }

  return com_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&EncryptAndRecordMetricsOnCOMThread),
      base::BindOnce(StorePrefOnUiThread, local_state));
}

}  // namespace os_crypt
