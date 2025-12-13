// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service_android.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "components/payments/content/browser_binding/mock_browser_bound_key_store.h"
#include "components/payments/content/browser_binding/mock_passkey_browser_binder.h"
#include "components/payments/content/mock_web_payments_web_data_service.h"
#include "components/webauthn/core/browser/mock_internal_authenticator.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

using testing::_;
using testing::AllOf;
using testing::Field;
using testing::IsEmpty;
using testing::Matcher;
using testing::Return;
using testing::UnorderedElementsAre;

}  // namespace

namespace payments {

namespace {

Matcher<BrowserBoundKeyMetadata::RelyingPartyAndCredentialId>
RelyingPartyAndCredentialIdMatcher(std::string relying_party_id,
                                   std::vector<uint8_t> credential_id) {
  return AllOf(
      Field("relying_party_id",
            &BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::
                relying_party_id,
            relying_party_id),
      Field(
          "credential_id",
          &BrowserBoundKeyMetadata::RelyingPartyAndCredentialId::credential_id,
          credential_id));
}

Matcher<BrowserBoundKeyMetadata> BrowserBoundKeyMetadataMatcher(
    std::string relying_party_id,
    std::vector<uint8_t> credential_id,
    std::vector<uint8_t> browser_bound_key_id,
    base::Time last_used) {
  return AllOf(
      Field(
          "passkey", &BrowserBoundKeyMetadata::passkey,
          RelyingPartyAndCredentialIdMatcher(relying_party_id, credential_id)),
      Field("browser_bound_key_id",
            &BrowserBoundKeyMetadata::browser_bound_key_id,
            browser_bound_key_id),
      Field("last_used", &BrowserBoundKeyMetadata::last_used, last_used));
}

}  // namespace

class BrowserBoundKeyDeleterServiceAndroidTest : public ::testing::Test {
 public:
  BrowserBoundKeyDeleterServiceAndroidTest() {
    EXPECT_TRUE(base::Time::FromUTCString("24 Oct 2025 10:30", &last_used_));

    auto web_data_service =
        base::MakeRefCounted<MockWebPaymentsWebDataService>();
    auto key_store = base::MakeRefCounted<MockBrowserBoundKeyStore>();

    deleter_ = std::make_unique<BrowserBoundKeyDeleterServiceAndroid>(
        web_data_service, key_store);

    auto authenticator = std::make_unique<webauthn::MockInternalAuthenticator>(
        /*web_contents=*/nullptr);
    authenticator_ = authenticator.get();
    deleter_->SetInternalAuthenticatorForTesting(std::move(authenticator));

    auto passkey_browser_binder =
        std::make_unique<MockPasskeyBrowserBinder>(key_store, web_data_service);
    passkey_browser_binder_ = passkey_browser_binder.get();
    deleter_->SetPasskeyBrowserBinderForTesting(
        std::move(passkey_browser_binder));
  }

  ~BrowserBoundKeyDeleterServiceAndroidTest() override = default;

  std::vector<BrowserBoundKeyMetadata> CreateBBKMetadataVector() {
    BrowserBoundKeyMetadata bbk_meta;
    bbk_meta.passkey = BrowserBoundKeyMetadata::RelyingPartyAndCredentialId(
        relying_party_, credential_id_);
    bbk_meta.browser_bound_key_id = browser_bound_key_id_;
    bbk_meta.last_used = last_used_;
    return {std::move(bbk_meta)};
  }

 protected:
  const std::vector<uint8_t> browser_bound_key_id_ = {11, 12, 13, 14};
  const std::vector<uint8_t> credential_id_ = {21, 22, 23, 24};
  const std::string relying_party_ = "relying.test";
  base::Time last_used_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<BrowserBoundKeyDeleterServiceAndroid> deleter_;
  raw_ptr<webauthn::MockInternalAuthenticator> authenticator_;
  raw_ptr<MockPasskeyBrowserBinder> passkey_browser_binder_;

 private:
  base::test::ScopedFeatureList feature_list_{
      blink::features::kSecurePaymentConfirmationBrowserBoundKeys};
};

TEST_F(BrowserBoundKeyDeleterServiceAndroidTest, RemoveInvalidBBKs) {
  base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)>
      get_all_browser_bound_keys_captured_callback;
  EXPECT_CALL(*authenticator_, IsGetMatchingCredentialIdsSupported())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys(/*callback=*/_))
      .WillOnce(MoveArg<0>(&get_all_browser_bound_keys_captured_callback));

  deleter_->RemoveInvalidBBKs();

  webauthn::GetMatchingCredentialIdsCallback
      get_matching_credential_ids_captured_callback;
  EXPECT_CALL(*authenticator_,
              GetMatchingCredentialIds(
                  relying_party_, UnorderedElementsAre(credential_id_),
                  /*require_third_party_payment_bit=*/false,
                  /*callback=*/_))
      .WillOnce(MoveArg<3>(&get_matching_credential_ids_captured_callback));

  // Callback to `FilterAndDeleteInvalidBBKs()`.
  std::move(get_all_browser_bound_keys_captured_callback)
      .Run(CreateBBKMetadataVector());

  base::OnceClosure delete_browser_bound_keys_captured_callback;
  EXPECT_CALL(
      *passkey_browser_binder_,
      DeleteBrowserBoundKeys(
          /*callback=*/_, UnorderedElementsAre(BrowserBoundKeyMetadataMatcher(
                              relying_party_, credential_id_,
                              browser_bound_key_id_, last_used_))))
      .WillOnce(MoveArg<0>(&delete_browser_bound_keys_captured_callback));

  // Callback to `BarrierCallback` in `FilterAndDeleteInvalidBBKs()`.
  // Empty vector means no matching credential IDs found, so the BBK
  // metadata will be deleted.
  std::move(get_matching_credential_ids_captured_callback).Run({});

  passkey_browser_binder_ = nullptr;
  // Making sure that the `DeleteBrowserBoundKeys` callback can be run. This
  // should only be destroying the `PasskeyBrowserBinder` instance.
  std::move(delete_browser_bound_keys_captured_callback).Run();
}

TEST_F(BrowserBoundKeyDeleterServiceAndroidTest,
       RemoveInvalidBBKs_WithoutInvalidBBKs) {
  base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)>
      get_all_browser_bound_keys_captured_callback;
  EXPECT_CALL(*authenticator_, IsGetMatchingCredentialIdsSupported())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys(/*callback=*/_))
      .WillOnce(MoveArg<0>(&get_all_browser_bound_keys_captured_callback));

  deleter_->RemoveInvalidBBKs();

  webauthn::GetMatchingCredentialIdsCallback
      get_matching_credential_ids_captured_callback;
  EXPECT_CALL(*authenticator_,
              GetMatchingCredentialIds(
                  relying_party_, UnorderedElementsAre(credential_id_),
                  /*require_third_party_payment_bit=*/false,
                  /*callback=*/_))
      .WillOnce(MoveArg<3>(&get_matching_credential_ids_captured_callback));

  // Callback to `FilterAndDeleteInvalidBBKs()`.
  std::move(get_all_browser_bound_keys_captured_callback)
      .Run(CreateBBKMetadataVector());

  // Vector of BBK metadata to be deleted should be empty.
  base::OnceClosure delete_browser_bound_keys_captured_callback;
  EXPECT_CALL(*passkey_browser_binder_,
              DeleteBrowserBoundKeys(
                  /*callback=*/_, /*bbk_metas=*/IsEmpty()))
      .WillOnce(MoveArg<0>(&delete_browser_bound_keys_captured_callback));

  // Callback to `BarrierCallback` in `FilterAndDeleteInvalidBBKs()`.
  // Vector contains the credential ID, meaning the BBK metadata is valid and
  // should not be deleted.
  std::move(get_matching_credential_ids_captured_callback)
      .Run({credential_id_});

  passkey_browser_binder_ = nullptr;
  // Making sure that the `DeleteBrowserBoundKeys` callback can be run. This
  // should only be destroying the `PasskeyBrowserBinder` instance.
  std::move(delete_browser_bound_keys_captured_callback).Run();
}

TEST_F(BrowserBoundKeyDeleterServiceAndroidTest,
       RemoveInvalidBBKs_WithoutBBKs) {
  base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)>
      get_all_browser_bound_keys_captured_callback;
  EXPECT_CALL(*authenticator_, IsGetMatchingCredentialIdsSupported())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys(/*callback=*/_))
      .WillOnce(MoveArg<0>(&get_all_browser_bound_keys_captured_callback));

  deleter_->RemoveInvalidBBKs();

  // Since there is no BBK metadata, `GetMatchingCredentialIds` should not be
  // called.
  EXPECT_CALL(*authenticator_, GetMatchingCredentialIds).Times(0);

  // Callback to `FilterAndDeleteInvalidBBKs()`.
  // Returning empty vector simulates no BBK metadata stored.
  std::move(get_all_browser_bound_keys_captured_callback).Run({});
}

TEST_F(BrowserBoundKeyDeleterServiceAndroidTest,
       RemoveInvalidBBKs_IsGetMatchingCredentialIdsSupportedFalse) {
  EXPECT_CALL(*authenticator_, IsGetMatchingCredentialIdsSupported())
      .WillRepeatedly(Return(false));
  // Since `IsGetMatchingCredentialIdsSupported()` returns false,
  // `GetAllBrowserBoundKeys()` should not be called.
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys).Times(0);

  deleter_->RemoveInvalidBBKs();
}

TEST_F(BrowserBoundKeyDeleterServiceAndroidTest,
       RemoveInvalidBBKs_BbkKeyStoreIsNull) {
  auto web_data_service = base::MakeRefCounted<MockWebPaymentsWebDataService>();
  auto deleter = std::make_unique<BrowserBoundKeyDeleterServiceAndroid>(
      web_data_service, nullptr);

  auto authenticator = std::make_unique<webauthn::MockInternalAuthenticator>(
      /*web_contents=*/nullptr);
  auto authenticator_ptr = authenticator.get();
  deleter->SetInternalAuthenticatorForTesting(std::move(authenticator));

  EXPECT_CALL(*authenticator_ptr, IsGetMatchingCredentialIdsSupported).Times(0);
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys).Times(0);

  deleter->RemoveInvalidBBKs();
}

TEST_F(BrowserBoundKeyDeleterServiceAndroidTest,
       RemoveInvalidBBKs_WebDataServiceIsNull) {
  auto key_store = base::MakeRefCounted<MockBrowserBoundKeyStore>();
  auto deleter = std::make_unique<BrowserBoundKeyDeleterServiceAndroid>(
      nullptr, key_store);

  auto authenticator = std::make_unique<webauthn::MockInternalAuthenticator>(
      /*web_contents=*/nullptr);
  auto authenticator_ptr = authenticator.get();
  deleter->SetInternalAuthenticatorForTesting(std::move(authenticator));

  EXPECT_CALL(*authenticator_ptr, IsGetMatchingCredentialIdsSupported).Times(0);
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys).Times(0);

  deleter->RemoveInvalidBBKs();
}

class BrowserBoundKeyDeleterServiceAndroidBbkFeatureDisabledTest
    : public BrowserBoundKeyDeleterServiceAndroidTest {
 public:
  BrowserBoundKeyDeleterServiceAndroidBbkFeatureDisabledTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kSecurePaymentConfirmationBrowserBoundKeys);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BrowserBoundKeyDeleterServiceAndroidBbkFeatureDisabledTest,
       RemoveInvalidBBKs) {
  EXPECT_CALL(*authenticator_, IsGetMatchingCredentialIdsSupported())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys).Times(0);

  deleter_->RemoveInvalidBBKs();
}

}  // namespace payments
