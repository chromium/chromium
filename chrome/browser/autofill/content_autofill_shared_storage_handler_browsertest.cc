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
#include "components/autofill/content/common/content_autofill_features.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill {

class ContentAutofillSharedStorageHandlerBrowserTest
    : public InProcessBrowserTest {
 public:
  ContentAutofillSharedStorageHandlerBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillSharedStorageServerCardData);
  }
  ~ContentAutofillSharedStorageHandlerBrowserTest() override = default;

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ContentAutofillSharedStorageHandlerBrowserTest,
                       CheckSharedStorageData) {
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
  storage::SharedStorageDatabase::GetResult result = future.Take();
  ASSERT_EQ(result.result,
            storage::SharedStorageDatabase::OperationResult::kSuccess);

  AutofillCreditCardList card_list_proto;
  std::string decoded_data;
  ASSERT_TRUE(
      base::Base64Decode(base::UTF16ToUTF8(result.data), &decoded_data));
  card_list_proto.ParseFromString(decoded_data);
  auto card_data_list = card_list_proto.server_cards();
  // TODO(crbug.com/324137757): AddTestServerCreditCard results in duplicate
  // cards in the database. Check for exactly one card here once that's fixed.
  ASSERT_LE(1, card_data_list.size());
  AutofillCreditCardData card_data = card_data_list[0];
  EXPECT_EQ(card.LastFourDigits(), base::UTF8ToUTF16(card_data.last_four()));
  EXPECT_EQ(card.network(), card_data.network());
  EXPECT_EQ(card.expiration_month(),
            static_cast<int>(card_data.expiration_month()));
  EXPECT_EQ(card.expiration_year(),
            static_cast<int>(card_data.expiration_year()));

  histogram_tester_.ExpectUniqueSample(
      "Autofill.SharedStorageServerCardDataSetResult",
      storage::SharedStorageManager::OperationResult::kSet, 1);
}

class AutofillSharedStorageServerCardDataDisabledTest
    : public InProcessBrowserTest {
 public:
  AutofillSharedStorageServerCardDataDisabledTest() {
    feature_list_.InitAndDisableFeature(
        features::kAutofillSharedStorageServerCardData);
  }
  ~AutofillSharedStorageServerCardDataDisabledTest() override = default;

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
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
