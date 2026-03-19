// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/content/browser/autofill_shared_storage.pb.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill {

class AutofillSharedStorageServerCardDataDisabledTest
    : public InProcessBrowserTest {
 public:
  AutofillSharedStorageServerCardDataDisabledTest() = default;
  ~AutofillSharedStorageServerCardDataDisabledTest() override = default;

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(AutofillSharedStorageServerCardDataDisabledTest,
                       NoSharedStorageData) {
  CreditCard card = test::GetMaskedServerCardVisa();
  AddTestServerCreditCard(browser()->profile(), card);

  base::test::TestFuture<storage::SharedStorageDatabase::GetResult> future;
  // TODO(crbug.com/41492904): Once this data is available via fenced frame,
  // this should test accessing the data via javascript.
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetSharedStorageManager()
      ->Get(payments::GetGooglePayScriptOrigin(), u"browser_autofill_card_data",
            future.GetCallback());
  ASSERT_EQ(future.Take().result,
            storage::SharedStorageDatabase::OperationResult::kNotFound);

  histogram_tester_.ExpectTotalCount(
      "Autofill.SharedStorageServerCardDataSetResult", 0);
}

}  // namespace autofill
