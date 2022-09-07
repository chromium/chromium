// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_FORM_DATABASE_SERVICE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_FORM_DATABASE_SERVICE_H_

#include "base/files/file_path.h"
#include "base/synchronization/waitable_event.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database_service.h"

namespace android_webview {

// Handles the database operations necessary to implement the autocomplete
// functionality. This includes creating and initializing the components that
// handle the database backend, and providing a synchronous interface when
// needed (the chromium database components have an async. interface).
class AwFormDatabaseService : public WebDataServiceConsumer {
 public:
  AwFormDatabaseService(const base::FilePath path);

  AwFormDatabaseService(const AwFormDatabaseService&) = delete;
  AwFormDatabaseService& operator=(const AwFormDatabaseService&) = delete;

  ~AwFormDatabaseService() override;

  void Shutdown();

  // Returns whether the database has any data stored. May do
  // IO access and block.
  bool HasFormData();

  // Clear any saved form data. Executes asynchronously.
  void ClearFormData();

  scoped_refptr<autofill::AutofillWebDataService>
      get_autofill_webdata_service();

  // WebDataServiceConsumer implementation.
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

 private:
  bool has_form_data_result_;
  base::WaitableEvent has_form_data_completion_;

  scoped_refptr<autofill::AutofillWebDataService> autofill_data_;
  scoped_refptr<WebDatabaseService> web_database_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_FORM_DATABASE_SERVICE_H_
