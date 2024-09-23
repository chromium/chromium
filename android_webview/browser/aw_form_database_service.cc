// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_form_database_service.h"

#include "android_webview/browser/aw_browser_process.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/webdata/common/webdata_constants.h"
#include "content/public/browser/browser_thread.h"

using base::WaitableEvent;

namespace {

// Callback to handle database error. It seems chrome uses this to
// display an error dialog box only.
void DatabaseErrorCallback(sql::InitStatus init_status,
                           const std::string& diagnostics) {
  LOG(WARNING) << "initializing autocomplete database failed";
}

}  // namespace

namespace android_webview {

AwFormDatabaseService::AwFormDatabaseService(const base::FilePath path)
    : has_form_data_result_(false),
      has_form_data_completion_(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  web_database_ = new WebDatabaseService(
      path.Append(kWebDataFilename), content::GetUIThreadTaskRunner({}),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  web_database_->AddTable(std::make_unique<autofill::AutocompleteTable>());
  // WebView shouldn't depend on non-Autocomplete tables. However,
  // `AwFormDatabaseService::ClearFormData()` also clear Autofill-related data.
  // This is likely a bug.
  // Once crbug.com/1501199 is resolved, all tables can be removed.
  web_database_->AddTable(std::make_unique<autofill::AddressAutofillTable>());
  web_database_->AddTable(std::make_unique<autofill::PaymentsAutofillTable>());
  web_database_->LoadDatabase(
      AwBrowserProcess::GetInstance()->GetOSCryptAsync());

  autofill_data_ = new autofill::AutofillWebDataService(
      web_database_, content::GetUIThreadTaskRunner({}));
  autofill_data_->Init(base::BindOnce(&DatabaseErrorCallback));
}

AwFormDatabaseService::~AwFormDatabaseService() {
  Shutdown();
}

void AwFormDatabaseService::Shutdown() {
  // TODO(sgurun) we don't run into this logic right now, but if we do, then we
  // need to implement cancellation of pending queries.
  autofill_data_->ShutdownOnUISequence();
  web_database_->ShutdownDatabase();
}

scoped_refptr<autofill::AutofillWebDataService>
AwFormDatabaseService::get_autofill_webdata_service() {
  return autofill_data_;
}

void AwFormDatabaseService::ClearFormData() {
  base::Time begin;
  base::Time end = base::Time::Max();
  autofill_data_->RemoveFormElementsAddedBetween(begin, end);
}

bool AwFormDatabaseService::HasFormData() {
  has_form_data_result_ = false;
  has_form_data_completion_.Reset();
  using awds = autofill::AutofillWebDataService;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&awds::GetCountOfValuesContainedBetween),
          autofill_data_, base::Time(), base::Time::Max(), this));
  {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    has_form_data_completion_.Wait();
  }
  return has_form_data_result_;
}

void AwFormDatabaseService::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  if (result) {
    DCHECK_EQ(AUTOFILL_VALUE_RESULT, result->GetType());
    const WDResult<int>* autofill_result =
        static_cast<const WDResult<int>*>(result.get());
    has_form_data_result_ = autofill_result->GetValue() > 0;
  }
  has_form_data_completion_.Signal();
}

}  // namespace android_webview
