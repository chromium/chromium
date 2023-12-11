// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/key_credential_manager_support_reporter_win.h"

#include <windows.foundation.h>
#include <windows.security.credentials.h>
#include <windows.storage.streams.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Security::Credentials::IKeyCredentialManagerStatics;
using Microsoft::WRL::ComPtr;

namespace key_credential_manager_support {

namespace {

// This has to match the corresponding metrics enumeration in
// tools/metrics/histograms/metadata/webauthn/enums.xml.
enum KeyCredentialManagerAvailability {
  kKeyCredentialManagerAvailable = 0,
  kKeyCredentialManagerNotAvailable = 1,
  kActivationFactoryNotAvailable = 2,
  kIsSupportedAsyncCallFailed = 3,
  kPostAsyncHandlersCallFailed = 4,
  kAsyncOperationFailed = 5,

  kMaxValue = kAsyncOperationFailed,
};

BASE_FEATURE(kReportKeyCredentialManagerSupportWinFeature,
             "ReportKeyCredentialManagerSupportWin",
             base::FEATURE_ENABLED_BY_DEFAULT);

void ReportIsSupportedOutcome(KeyCredentialManagerAvailability availability) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.Windows.KeyCredentialManagerSupported", availability);
}

void AsyncOperationCallback(boolean outcome) {
  if (outcome) {
    ReportIsSupportedOutcome(kKeyCredentialManagerAvailable);
    return;
  }
  ReportIsSupportedOutcome(kKeyCredentialManagerNotAvailable);
}

// This can take a long time and must be called on a blocking thread.
void CheckAndReportIsSupported() {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  ComPtr<IKeyCredentialManagerStatics> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IKeyCredentialManagerStatics,
      RuntimeClass_Windows_Security_Credentials_KeyCredentialManager>(&factory);
  if (FAILED(hr)) {
    ReportIsSupportedOutcome(kActivationFactoryNotAvailable);
    return;
  }

  ComPtr<IAsyncOperation<bool>> is_supported_operation;
  hr = factory->IsSupportedAsync(&is_supported_operation);
  if (FAILED(hr)) {
    ReportIsSupportedOutcome(kIsSupportedAsyncCallFailed);
    return;
  }

  hr = base::win::PostAsyncHandlers(
      is_supported_operation.Get(), base::BindOnce(&AsyncOperationCallback),
      base::BindOnce(
          [](HRESULT) { ReportIsSupportedOutcome(kAsyncOperationFailed); }));
  if (FAILED(hr)) {
    ReportIsSupportedOutcome(kPostAsyncHandlersCallFailed);
  }
}

}  // namespace

void ReportKeyCredentialManagerSupport() {
  if (base::FeatureList::IsEnabled(
          kReportKeyCredentialManagerSupportWinFeature)) {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(&CheckAndReportIsSupported));
  }
}

}  // namespace key_credential_manager_support
