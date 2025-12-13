// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service_desktop.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/webauthn/mock_local_credential_management.h"
#include "components/payments/content/browser_binding/mock_browser_bound_key_store.h"
#include "components/payments/content/browser_binding/mock_passkey_browser_binder.h"
#include "components/payments/content/mock_web_payments_web_data_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

using device::DiscoverableCredentialMetadata;
using testing::_;
using testing::AllOf;
using testing::Field;
using testing::IsEmpty;
using testing::Matcher;
using testing::UnorderedElementsAre;

}  // namespace

namespace payments {

namespace {

constexpr base::TimeDelta kBrowserBoundKeyExpirationDuration =
    9 * base::Days(30);

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

Matcher<BrowserBoundKeyMetadata> EqBrowserBoundKeyMetadataMatcher(
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

class BrowserBoundKeyDeleterServiceDesktopTest : public ::testing::Test {
 public:
  BrowserBoundKeyDeleterServiceDesktopTest() {
    // Uses a fixed mock time that can be advanced in tests.
    last_used_ = base::Time::NowFromSystemTime();

    auto web_data_service =
        base::MakeRefCounted<MockWebPaymentsWebDataService>();
    auto key_store = base::MakeRefCounted<MockBrowserBoundKeyStore>();
    auto local_credential_management =
        std::make_unique<MockLocalCredentialManagement>();
    local_credential_management_ = local_credential_management.get();

    deleter_ = std::make_unique<BrowserBoundKeyDeleterServiceDesktop>(
        web_data_service, key_store, std::move(local_credential_management));

    auto passkey_browser_binder =
        std::make_unique<MockPasskeyBrowserBinder>(key_store, web_data_service);
    passkey_browser_binder_ = passkey_browser_binder.get();
    deleter_->SetPasskeyBrowserBinderForTesting(
        std::move(passkey_browser_binder));
  }

  ~BrowserBoundKeyDeleterServiceDesktopTest() override = default;

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

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<BrowserBoundKeyDeleterServiceDesktop> deleter_;
  raw_ptr<MockLocalCredentialManagement> local_credential_management_;
  raw_ptr<MockPasskeyBrowserBinder> passkey_browser_binder_;

 private:
  base::test::ScopedFeatureList feature_list_{
      blink::features::kSecurePaymentConfirmationBrowserBoundKeys};
};

TEST_F(BrowserBoundKeyDeleterServiceDesktopTest,
       RemoveInvalidBBKs_WhenPasskeyDoesNotExist) {
  base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)>
      get_all_browser_bound_keys_captured_callback;
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys(/*callback=*/_))
      .WillOnce(MoveArg<0>(&get_all_browser_bound_keys_captured_callback));

  deleter_->RemoveInvalidBBKs();

  base::OnceCallback<void(
      std::optional<std::vector<DiscoverableCredentialMetadata>>)>
      enumerate_captured_callback;
  EXPECT_CALL(*local_credential_management_, Enumerate(
                                                 /*callback=*/_))
      .WillOnce(MoveArg<0>(&enumerate_captured_callback));

  // Callback to `FilterAndDeleteInvalidBBKs()`.
  std::move(get_all_browser_bound_keys_captured_callback)
      .Run(CreateBBKMetadataVector());

  base::OnceClosure delete_browser_bound_keys_captured_callback;
  EXPECT_CALL(
      *passkey_browser_binder_,
      DeleteBrowserBoundKeys(
          /*callback=*/_, UnorderedElementsAre(EqBrowserBoundKeyMetadataMatcher(
                              relying_party_, credential_id_,
                              browser_bound_key_id_, last_used_))))
      .WillOnce(MoveArg<0>(&delete_browser_bound_keys_captured_callback));

  // Callback to `OnEnumerateComplete()`. Empty vector means no matching
  // credential IDs found, so the BBK metadata will be deleted.
  std::move(enumerate_captured_callback)
      .Run(std::vector<DiscoverableCredentialMetadata>{});

  passkey_browser_binder_ = nullptr;

  // Making sure that the `DeleteBrowserBoundKeys` callback can be run. This
  // should only be destroying the `PasskeyBrowserBinder` instance.
  std::move(delete_browser_bound_keys_captured_callback).Run();
}

TEST_F(BrowserBoundKeyDeleterServiceDesktopTest,
       RemoveInvalidBBKs_UnableToListPasskeysWithExpiredBBKs) {
  base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)>
      get_all_browser_bound_keys_captured_callback;
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys(/*callback=*/_))
      .WillOnce(MoveArg<0>(&get_all_browser_bound_keys_captured_callback));

  deleter_->RemoveInvalidBBKs();

  base::OnceCallback<void(
      std::optional<std::vector<DiscoverableCredentialMetadata>>)>
      enumerate_captured_callback;
  EXPECT_CALL(*local_credential_management_, Enumerate(
                                                 /*callback=*/_))
      .WillOnce(MoveArg<0>(&enumerate_captured_callback));

  // Callback to `FilterAndDeleteInvalidBBKs()`.
  std::move(get_all_browser_bound_keys_captured_callback)
      .Run(CreateBBKMetadataVector());

  base::OnceClosure delete_browser_bound_keys_captured_callback;
  EXPECT_CALL(
      *passkey_browser_binder_,
      DeleteBrowserBoundKeys(
          /*callback=*/_, UnorderedElementsAre(EqBrowserBoundKeyMetadataMatcher(
                              relying_party_, credential_id_,
                              browser_bound_key_id_, last_used_))))
      .WillOnce(MoveArg<0>(&delete_browser_bound_keys_captured_callback));

  // Callback to `OnEnumerateComplete()`. Nullopt means local credential manager
  // is unavailable. Advance time so that the BBK metadata is expired and
  // should be deleted.
  task_environment_.FastForwardBy(kBrowserBoundKeyExpirationDuration +
                                  base::Hours(1));
  std::move(enumerate_captured_callback).Run(std::nullopt);

  passkey_browser_binder_ = nullptr;

  // Making sure that the `DeleteBrowserBoundKeys` callback can be run. This
  // should only be destroying the `PasskeyBrowserBinder` instance.
  std::move(delete_browser_bound_keys_captured_callback).Run();
}

TEST_F(BrowserBoundKeyDeleterServiceDesktopTest,
       RemoveInvalidBBKs_UnableToListPasskeysWithActiveBBKs) {
  base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)>
      get_all_browser_bound_keys_captured_callback;
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys(/*callback=*/_))
      .WillOnce(MoveArg<0>(&get_all_browser_bound_keys_captured_callback));

  deleter_->RemoveInvalidBBKs();

  base::OnceCallback<void(
      std::optional<std::vector<DiscoverableCredentialMetadata>>)>
      enumerate_captured_callback;
  EXPECT_CALL(*local_credential_management_, Enumerate(
                                                 /*callback=*/_))
      .WillOnce(MoveArg<0>(&enumerate_captured_callback));

  // Callback to `FilterAndDeleteInvalidBBKs()`.
  std::move(get_all_browser_bound_keys_captured_callback)
      .Run(CreateBBKMetadataVector());

  // Nothing needs to be deleted because we are not at the expiry duration yet
  // so DeleteBrowserBoundKeys() should not be called.
  EXPECT_CALL(*passkey_browser_binder_, DeleteBrowserBoundKeys).Times(0);

  passkey_browser_binder_ = nullptr;

  // Callback to `OnEnumerateComplete()`. Nullopt means local credential manager
  // is unavailable. Since time isn't advanced to the expiration threshold, the
  // BBK metadata should not be deleted.
  task_environment_.FastForwardBy(kBrowserBoundKeyExpirationDuration -
                                  base::Hours(1));
  std::move(enumerate_captured_callback).Run(std::nullopt);
}

TEST_F(BrowserBoundKeyDeleterServiceDesktopTest,
       RemoveInvalidBBKs_WhenPasskeyExists) {
  base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)>
      get_all_browser_bound_keys_captured_callback;
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys(/*callback=*/_))
      .WillOnce(MoveArg<0>(&get_all_browser_bound_keys_captured_callback));

  deleter_->RemoveInvalidBBKs();

  base::OnceCallback<void(
      std::optional<std::vector<DiscoverableCredentialMetadata>>)>
      enumerate_captured_callback;
  EXPECT_CALL(*local_credential_management_, Enumerate(
                                                 /*callback=*/_))
      .WillOnce(MoveArg<0>(&enumerate_captured_callback));

  // Callback to `FilterAndDeleteInvalidBBKs()`.
  std::move(get_all_browser_bound_keys_captured_callback)
      .Run(CreateBBKMetadataVector());

  // Nothing needs to be deleted because the passkey still exists so
  // DeleteBrowserBoundKeys() should not be called.
  EXPECT_CALL(*passkey_browser_binder_, DeleteBrowserBoundKeys).Times(0);

  passkey_browser_binder_ = nullptr;

  // Callback to `OnEnumerateComplete()` containing the relying party
  // ID/credential ID in the BBK store, meaning the BBK metadata is valid and
  // should not be deleted.
  auto credential = DiscoverableCredentialMetadata();
  credential.rp_id = relying_party_;
  credential.cred_id = credential_id_;
  std::move(enumerate_captured_callback)
      .Run(std::vector<DiscoverableCredentialMetadata>{credential});
}

TEST_F(BrowserBoundKeyDeleterServiceDesktopTest,
       RemoveInvalidBBKs_WithoutBBKs) {
  base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)>
      get_all_browser_bound_keys_captured_callback;
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys(/*callback=*/_))
      .WillOnce(MoveArg<0>(&get_all_browser_bound_keys_captured_callback));

  deleter_->RemoveInvalidBBKs();

  // Since there is no BBK metadata, `Enumerate` should not be called.
  EXPECT_CALL(*local_credential_management_, Enumerate).Times(0);

  passkey_browser_binder_ = nullptr;

  // Callback to `FilterAndDeleteInvalidBBKs()`.
  // Returning empty vector simulates no BBK metadata stored.
  std::move(get_all_browser_bound_keys_captured_callback).Run({});
}

TEST_F(BrowserBoundKeyDeleterServiceDesktopTest,
       RemoveInvalidBBKs_LocalCredentialManagementIsNull) {
  auto web_data_service = base::MakeRefCounted<MockWebPaymentsWebDataService>();
  auto key_store = base::MakeRefCounted<MockBrowserBoundKeyStore>();

  auto deleter = std::make_unique<BrowserBoundKeyDeleterServiceDesktop>(
      web_data_service, key_store,
      /*local_credential_management=*/nullptr);

  auto passkey_browser_binder =
      std::make_unique<MockPasskeyBrowserBinder>(key_store, web_data_service);
  auto passkey_browser_binder_ptr = passkey_browser_binder.get();
  deleter->SetPasskeyBrowserBinderForTesting(std::move(passkey_browser_binder));

  EXPECT_CALL(*passkey_browser_binder_ptr, GetAllBrowserBoundKeys).Times(0);

  deleter->RemoveInvalidBBKs();
}

TEST_F(BrowserBoundKeyDeleterServiceDesktopTest,
       RemoveInvalidBBKs_KeyStoreIsNull) {
  auto web_data_service = base::MakeRefCounted<MockWebPaymentsWebDataService>();
  auto key_store = base::MakeRefCounted<MockBrowserBoundKeyStore>();

  auto deleter = std::make_unique<BrowserBoundKeyDeleterServiceDesktop>(
      web_data_service, /*browser_bound_key_store=*/nullptr,
      std::make_unique<MockLocalCredentialManagement>());

  auto passkey_browser_binder =
      std::make_unique<MockPasskeyBrowserBinder>(key_store, web_data_service);
  auto passkey_browser_binder_ptr = passkey_browser_binder.get();
  deleter->SetPasskeyBrowserBinderForTesting(std::move(passkey_browser_binder));

  EXPECT_CALL(*passkey_browser_binder_ptr, GetAllBrowserBoundKeys).Times(0);

  deleter->RemoveInvalidBBKs();
}

TEST_F(BrowserBoundKeyDeleterServiceDesktopTest,
       RemoveInvalidBBKs_WebDataServiceIsNull) {
  auto web_data_service = base::MakeRefCounted<MockWebPaymentsWebDataService>();
  auto key_store = base::MakeRefCounted<MockBrowserBoundKeyStore>();

  auto deleter = std::make_unique<BrowserBoundKeyDeleterServiceDesktop>(
      /*web_data_service=*/nullptr, key_store,
      std::make_unique<MockLocalCredentialManagement>());

  auto passkey_browser_binder =
      std::make_unique<MockPasskeyBrowserBinder>(key_store, web_data_service);
  auto passkey_browser_binder_ptr = passkey_browser_binder.get();
  deleter->SetPasskeyBrowserBinderForTesting(std::move(passkey_browser_binder));

  EXPECT_CALL(*passkey_browser_binder_ptr, GetAllBrowserBoundKeys).Times(0);

  deleter->RemoveInvalidBBKs();
}

class BrowserBoundKeyDeleterServiceDesktopBbkFeatureDisabledTest
    : public BrowserBoundKeyDeleterServiceDesktopTest {
 public:
  BrowserBoundKeyDeleterServiceDesktopBbkFeatureDisabledTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kSecurePaymentConfirmationBrowserBoundKeys);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BrowserBoundKeyDeleterServiceDesktopBbkFeatureDisabledTest,
       RemoveInvalidBBKs) {
  EXPECT_CALL(*passkey_browser_binder_, GetAllBrowserBoundKeys).Times(0);

  deleter_->RemoveInvalidBBKs();
}

}  // namespace payments
