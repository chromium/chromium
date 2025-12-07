// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"

namespace extensions::autofill_util {

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using MockCallbackAfterSuccessfulUserAuth =
    base::MockCallback<CallbackAfterSuccessfulUserAuth>;

MATCHER_P(MatchesIbanType, iban, "") {
  bool is_local = iban.record_type() == autofill::Iban::RecordType::kLocalIban;
  return arg.metadata->is_local == is_local;
}

MATCHER_P(MatchesBnplIssuer, bnpl_issuer, "") {
  return arg.issuer_id ==
         autofill::ConvertToBnplIssuerIdString(bnpl_issuer.issuer_id());
}

class AutofillUtilTest : public InProcessBrowserTest {
 public:
  AutofillUtilTest() = default;

  AutofillUtilTest(const AutofillUtilTest&) = delete;
  AutofillUtilTest& operator=(const AutofillUtilTest&) = delete;

  void SetUpOnMainThread() override {
    mock_device_authenticator_ =
        std::make_unique<device_reauth::MockDeviceAuthenticator>();
  }

 protected:
  autofill::test::AutofillBrowserTestEnvironment autofill_test_environment_;
  std::unique_ptr<device_reauth::MockDeviceAuthenticator>
      mock_device_authenticator_;
};

class AutofillUtilTestForBnpl : public AutofillUtilTest {
 public:
  AutofillUtilTestForBnpl() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {autofill::features::kAutofillEnableBuyNowPayLater,
         autofill::features::kAutofillEnableBuyNowPayLaterSyncing,
         autofill::features::kAutofillEnableBuyNowPayLaterForKlarna},
        /*disabled_features=*/{});
  }

  AutofillUtilTestForBnpl(const AutofillUtilTestForBnpl&) = delete;
  AutofillUtilTestForBnpl& operator=(const AutofillUtilTestForBnpl&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AutofillUtilTest, GenerateIbanList) {
  autofill::TestPaymentsDataManager paydm;
  paydm.SetAutofillWalletImportEnabled(true);

  autofill::Iban local_iban = autofill::test::GetLocalIban();
  paydm.AddIbanForTest(std::make_unique<autofill::Iban>(local_iban));
  autofill::Iban server_iban = autofill::test::GetServerIban();
  paydm.AddServerIban(server_iban);

  IbanEntryList iban_list = GenerateIbanList(paydm);
  EXPECT_THAT(iban_list, UnorderedElementsAre(MatchesIbanType(local_iban),
                                              MatchesIbanType(server_iban)));
}

IN_PROC_BROWSER_TEST_F(AutofillUtilTest, AuthenticateUser_SuccessfulAuth) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  base::MockCallback<base::OnceCallback<void(bool)>> mock_result_callback;
  const std::u16string mock_prompt_message = u"This is a mock message";

  ON_CALL(*mock_device_authenticator_, AuthenticateWithMessage)
      .WillByDefault(
          testing::WithArg<1>([](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));
  EXPECT_CALL(mock_result_callback, Run(true));
  EXPECT_CALL(*mock_device_authenticator_,
              AuthenticateWithMessage(mock_prompt_message, _));

  mock_device_authenticator_->AuthenticateWithMessage(
      mock_prompt_message, mock_result_callback.Get());
#endif
}

IN_PROC_BROWSER_TEST_F(AutofillUtilTest, AuthenticateUser_UnSuccessfulAuth) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  base::MockCallback<base::OnceCallback<void(bool)>> mock_result_callback;
  const std::u16string mock_prompt_message = u"This is a mock message";

  ON_CALL(*mock_device_authenticator_, AuthenticateWithMessage)
      .WillByDefault(
          testing::WithArg<1>([](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          }));
  EXPECT_CALL(mock_result_callback, Run(false));
  EXPECT_CALL(*mock_device_authenticator_,
              AuthenticateWithMessage(mock_prompt_message, _));

  mock_device_authenticator_->AuthenticateWithMessage(
      mock_prompt_message, mock_result_callback.Get());
#endif
}

IN_PROC_BROWSER_TEST_F(AutofillUtilTestForBnpl,
                       GeneratePayOverTimeIssuerList_IssuerLinkedExternally) {
  autofill::TestPaymentsDataManager paydm;
  paydm.SetAutofillWalletImportEnabled(true);
  paydm.SetAutofillPaymentMethodsEnabled(true);
  paydm.SetIsAutofillBnplPrefEnabled(true);
  autofill::BnplIssuer issuer_affirm = autofill::test::GetTestLinkedBnplIssuer(
      autofill::BnplIssuer::IssuerId::kBnplAffirm);
  autofill::BnplIssuer issuer_klarna = autofill::test::GetTestLinkedBnplIssuer(
      autofill::BnplIssuer::IssuerId::kBnplKlarna,
      /*action_required=*/autofill::DenseSet(
          {autofill::PaymentInstrument::ActionRequired::kAcceptTos}));
  paydm.AddBnplIssuer(issuer_affirm);
  paydm.AddBnplIssuer(issuer_klarna);

  PayOverTimeIssuerEntryList bnpl_issuer_list =
      GeneratePayOverTimeIssuerList(paydm);
  EXPECT_THAT(bnpl_issuer_list,
              UnorderedElementsAre(MatchesBnplIssuer(issuer_affirm)));
}

IN_PROC_BROWSER_TEST_F(AutofillUtilTestForBnpl,
                       GeneratePayOverTimeIssuerList_IssuerTosAccepted) {
  autofill::TestPaymentsDataManager paydm;
  paydm.SetAutofillWalletImportEnabled(true);
  paydm.SetAutofillPaymentMethodsEnabled(true);
  paydm.SetIsAutofillBnplPrefEnabled(true);
  autofill::BnplIssuer issuer_affirm = autofill::test::GetTestLinkedBnplIssuer(
      autofill::BnplIssuer::IssuerId::kBnplAffirm);
  autofill::BnplIssuer issuer_klarna = autofill::test::GetTestLinkedBnplIssuer(
      autofill::BnplIssuer::IssuerId::kBnplKlarna);
  paydm.AddBnplIssuer(issuer_affirm);
  paydm.AddBnplIssuer(issuer_klarna);

  PayOverTimeIssuerEntryList bnpl_issuer_list =
      GeneratePayOverTimeIssuerList(paydm);
  EXPECT_THAT(bnpl_issuer_list,
              UnorderedElementsAre(MatchesBnplIssuer(issuer_affirm),
                                   MatchesBnplIssuer(issuer_klarna)));
}

}  // namespace

}  // namespace extensions::autofill_util
