// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_form_database_service.h"

#include "android_webview/browser/aw_browser_process.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/webdata/common/webdata_constants.h"
#include "content/public/browser/browser_thread.h"

namespace android_webview {

AwFormDatabaseService::AwFormDatabaseService(const base::FilePath path) {
  web_database_ = new WebDatabaseService(
      path.Append(kWebDataFilename), content::GetUIThreadTaskRunner({}),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  // The old AwFormDatabaseService used to create:
  // - autofill::AutocompleteTable
  // - autofill::AddressAutofillTable
  // - autofill::PaymentsAutofillTable
  web_database_->AddTable(
      std::make_unique<autofill::AutocompleteTable::Dropper>());
  web_database_->AddTable(
      std::make_unique<autofill::AddressAutofillTable::Dropper>());
  web_database_->AddTable(
      std::make_unique<autofill::PaymentsAutofillTable::Dropper>());
  web_database_->LoadDatabase(
      AwBrowserProcess::GetInstance()->GetOSCryptAsync());
}

AwFormDatabaseService::~AwFormDatabaseService() {
  web_database_->ShutdownDatabase();
}

}  // namespace android_webview
