// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_form_database_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/webdata/common/webdata_constants.h"

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
  auto ui_task_runner = base::ThreadTaskRunnerHandle::Get();
  // TODO(pkasting): http://crbug.com/740773 This should likely be sequenced,
  // not single-threaded; it's also possible these objects can each use their
  // own sequences instead of sharing this one.
  auto db_task_runner = base::CreateSingleThreadTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  web_database_ = new WebDatabaseService(path.Append(kWebDataFilename),
                                         ui_task_runner, db_task_runner);
  web_database_->AddTable(base::WrapUnique(new autofill::AutofillTable));
  web_database_->LoadDatabase();

  autofill_data_ = new autofill::AutofillWebDataService(
      web_database_, ui_task_runner, db_task_runner,
      base::Bind(&DatabaseErrorCallback));
  autofill_data_->Init();
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
  autofill_data_->RemoveAutofillDataModifiedBetween(begin, end);
}

bool AwFormDatabaseService::HasFormData() {
  has_form_data_result_ = false;
  has_form_data_completion_.Reset();
  using awds = autofill::AutofillWebDataService;
  base::PostTask(
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
