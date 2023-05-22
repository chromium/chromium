// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/cookie_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_disabled_checker.h"
#include "chrome/browser/sync/test/integration/sync_engine_stopped_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/nigori_test_utils.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/test/fake_security_domains_server.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "crypto/ec_private_key.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/sync/sync_error_notifier.h"
#include "chrome/browser/ash/sync/sync_error_notifier_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

using fake_server::GetServerNigori;
using fake_server::SetNigoriInFakeServer;
using syncer::BuildCustomPassphraseNigoriSpecifics;
using syncer::BuildKeystoreNigoriSpecifics;
using syncer::BuildTrustedVaultNigoriSpecifics;
using syncer::KeyParamsForTesting;
using syncer::KeystoreKeyParamsForTesting;
using syncer::Pbkdf2PassphraseKeyParamsForTesting;
using syncer::TrustedVaultKeyParamsForTesting;
using testing::Eq;
using testing::NotNull;
using testing::SizeIs;

const char kGaiaId[] = "gaia_id_for_user_gmail.com";
#if !BUILDFLAG(IS_CHROMEOS_ASH)
const char kAccountEmail[] = "user@gmail.com";
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

MATCHER_P(IsDataEncryptedWith, key_params, "") {
  const sync_pb::EncryptedData& encrypted_data = arg;
  std::unique_ptr<syncer::Nigori> nigori = syncer::Nigori::CreateByDerivation(
      key_params.derivation_params, key_params.password);
  return encrypted_data.key_name() == nigori->GetKeyName();
}

MATCHER_P4(StatusLabelsMatch,
           message_type,
           status_label_string_id,
           button_string_id,
           action_type,
           "") {
  if (arg.message_type != message_type) {
    *result_listener << "Wrong message type";
    return false;
  }
  if (arg.status_label_string_id != status_label_string_id) {
    *result_listener << "Wrong status label";
    return false;
  }
  if (arg.button_string_id != button_string_id) {
    *result_listener << "Wrong button string";
    return false;
  }
  if (arg.action_type != action_type) {
    *result_listener << "Wrong action type";
    return false;
  }
  return true;
}

GURL GetFakeTrustedVaultRetrievalURL(
    const net::test_server::EmbeddedTestServer& test_server,
    const std::vector<uint8_t>& encryption_key) {
  // encryption_keys_retrieval.html would populate encryption key to sync
  // service upon loading. Key is provided as part of URL and needs to be
  // encoded with Base64, because |encryption_key| is binary.
  const std::string base64_encoded_key = base::Base64Encode(encryption_key);
  return test_server.GetURL(
      base::StringPrintf("/sync/encryption_keys_retrieval.html?%s#%s", kGaiaId,
                         base64_encoded_key.c_str()));
}

GURL GetFakeTrustedVaultRecoverabilityURL(
    const net::test_server::EmbeddedTestServer& test_server,
    const std::vector<uint8_t>& public_key) {
  // encryption_keys_recoverability.html would populate encryption key to sync
  // service upon loading. Key is provided as part of URL and needs to be
  // encoded with Base64, because |public_key| is binary.
  const std::string base64_encoded_public_key = base::Base64Encode(public_key);
  return test_server.GetURL(
      base::StringPrintf("/sync/encryption_keys_recoverability.html?%s#%s",
                         kGaiaId, base64_encoded_public_key.c_str()));
}

std::string ComputeKeyName(const KeyParamsForTesting& key_params) {
  return syncer::Nigori::CreateByDerivation(key_params.derivation_params,
                                            key_params.password)
      ->GetKeyName();
}

// Helper function to install server redirects in the test HTTP server.
std::unique_ptr<net::test_server::HttpResponse> HttpServerRedirect(
    const GURL& from_prefix,
    const GURL& to,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.GetURL().spec(), from_prefix.spec())) {
    return nullptr;
  }
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", to.spec());
  http_response->set_content_type("text/html");
  http_response->set_content(base::StringPrintf(
      "<html><head></head><body>Redirecting to %s</body></html>",
      to.spec().c_str()));
  return http_response;
}

// Used to wait until a tab closes.
class TabClosedChecker : public StatusChangeChecker,
                         public content::WebContentsObserver {
 public:
  explicit TabClosedChecker(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    DCHECK(web_contents);
  }

  TabClosedChecker(const TabClosedChecker&) = delete;
  TabClosedChecker& operator=(const TabClosedChecker&) = delete;

  ~TabClosedChecker() override = default;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for the tab to be closed";
    return closed_;
  }

  // content::WebContentsObserver overrides.
  void WebContentsDestroyed() override {
    closed_ = true;
    CheckExitCondition();
  }

 private:
  bool closed_ = false;
};

// Used to wait until IsTrustedVaultKeyRequiredForPreferredDataTypes() returns
// true.
class TrustedVaultKeyRequiredForPreferredDataTypesChecker
    : public SingleClientStatusChangeChecker {
 public:
  explicit TrustedVaultKeyRequiredForPreferredDataTypesChecker(
      syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}
  ~TrustedVaultKeyRequiredForPreferredDataTypesChecker() override = default;

 protected:
  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting until trusted vault key is required for preferred "
           "datatypes";
    return service()
        ->GetUserSettings()
        ->IsTrustedVaultKeyRequiredForPreferredDataTypes();
  }
};

// Used to wait until IsTrustedVaultRecoverabilityDegraded() returns false.
class TrustedVaultRecoverabilityDegradedStateChecker
    : public SingleClientStatusChangeChecker {
 public:
  TrustedVaultRecoverabilityDegradedStateChecker(
      syncer::SyncServiceImpl* service,
      bool degraded)
      : SingleClientStatusChangeChecker(service), degraded_(degraded) {}
  ~TrustedVaultRecoverabilityDegradedStateChecker() override = default;

 protected:
  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting until trusted vault recoverability degraded state is "
        << degraded_;
    return service()
               ->GetUserSettings()
               ->IsTrustedVaultRecoverabilityDegraded() == degraded_;
  }

  const bool degraded_;
};

class FakeSecurityDomainsServerMemberStatusChecker
    : public StatusChangeChecker,
      public trusted_vault::FakeSecurityDomainsServer::Observer {
 public:
  FakeSecurityDomainsServerMemberStatusChecker(
      int expected_member_count,
      const std::vector<uint8_t>& expected_trusted_vault_key,
      trusted_vault::FakeSecurityDomainsServer* server)
      : expected_member_count_(expected_member_count),
        expected_trusted_vault_key_(expected_trusted_vault_key),
        server_(server) {
    server_->AddObserver(this);
  }

  ~FakeSecurityDomainsServerMemberStatusChecker() override {
    server_->RemoveObserver(this);
  }

 protected:
  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for security domains server to have members with"
           " expected key.";
    if (server_->GetMemberCount() != expected_member_count_) {
      *os << "Security domains server member count ("
          << server_->GetMemberCount() << ") doesn't match expected value ("
          << expected_member_count_ << ").";
      return false;
    }
    if (!server_->AllMembersHaveKey(expected_trusted_vault_key_)) {
      *os << "Some members in security domains service don't have expected "
             "key.";
      return false;
    }
    return true;
  }

 private:
  // FakeSecurityDomainsServer::Observer implementation.
  void OnRequestHandled() override { CheckExitCondition(); }

  int expected_member_count_;
  std::vector<uint8_t> expected_trusted_vault_key_;
  const raw_ptr<trusted_vault::FakeSecurityDomainsServer> server_;
};

class SingleClientNigoriSyncTest : public SyncTest {
 public:
  SingleClientNigoriSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientNigoriSyncTest(const SingleClientNigoriSyncTest&) = delete;
  SingleClientNigoriSyncTest& operator=(const SingleClientNigoriSyncTest&) =
      delete;

  ~SingleClientNigoriSyncTest() override = default;

  bool WaitForPasswordForms(
      const std::vector<password_manager::PasswordForm>& forms) const {
    return PasswordFormsChecker(0, forms).Wait();
  }
};

class SingleClientNigoriSyncTestWithNotAwaitQuiescence
    : public SingleClientNigoriSyncTest {
 public:
  SingleClientNigoriSyncTestWithNotAwaitQuiescence() = default;

  SingleClientNigoriSyncTestWithNotAwaitQuiescence(
      const SingleClientNigoriSyncTestWithNotAwaitQuiescence&) = delete;
  SingleClientNigoriSyncTestWithNotAwaitQuiescence& operator=(
      const SingleClientNigoriSyncTestWithNotAwaitQuiescence&) = delete;

  ~SingleClientNigoriSyncTestWithNotAwaitQuiescence() override = default;

  bool TestUsesSelfNotifications() override {
    // This test fixture is used with tests, which expect SetupSync() to be
    // waiting for completion, but not for quiescense, because it can't be
    // achieved and isn't needed.
    return false;
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTest,
                       ShouldCommitKeystoreNigoriWhenReceivedDefault) {
  // SetupSync() should make FakeServer send default NigoriSpecifics.
  ASSERT_TRUE(SetupSync());
  // TODO(crbug/922900): we may want to actually wait for specifics update in
  // fake server. Due to implementation details it's not currently needed.
  sync_pb::NigoriSpecifics specifics;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &specifics));

  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  EXPECT_THAT(
      specifics.encryption_keybag(),
      IsDataEncryptedWith(KeystoreKeyParamsForTesting(keystore_keys.back())));
  EXPECT_THAT(specifics.passphrase_type(),
              Eq(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE));
  EXPECT_TRUE(specifics.keybag_is_frozen());
  EXPECT_TRUE(specifics.has_keystore_migration_time());
}

// Tests that client can decrypt passwords, encrypted with implicit passphrase.
// Test first injects implicit passphrase Nigori and encrypted password form to
// fake server and then checks that client successfully received and decrypted
// this password form.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTest,
                       ShouldDecryptWithImplicitPassphraseNigori) {
  const KeyParamsForTesting kKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("passphrase");
  sync_pb::NigoriSpecifics specifics;
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          kKeyParams.password, kKeyParams.derivation_params);
  ASSERT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));
  SetNigoriInFakeServer(specifics, GetFakeServer());

  const password_manager::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(0);
  passwords_helper::InjectEncryptedServerPassword(
      password_form, kKeyParams.password, kKeyParams.derivation_params,
      GetFakeServer());

  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  EXPECT_TRUE(WaitForPasswordForms({password_form}));
}

// Tests that client can decrypt passwords, encrypted with keystore key in case
// Nigori node contains only this key. We first inject keystore Nigori and
// encrypted password form to fake server and then check that client
// successfully received and decrypted this password form.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTest,
                       ShouldDecryptWithKeystoreNigori) {
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  SetNigoriInFakeServer(BuildKeystoreNigoriSpecifics(
                            /*keybag_keys_params=*/{kKeystoreKeyParams},
                            /*keystore_decryptor_params=*/kKeystoreKeyParams,
                            /*keystore_key_params=*/kKeystoreKeyParams),
                        GetFakeServer());

  const password_manager::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(0);
  passwords_helper::InjectEncryptedServerPassword(
      password_form, kKeystoreKeyParams.password,
      kKeystoreKeyParams.derivation_params, GetFakeServer());
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForPasswordForms({password_form}));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriSyncTest,
    UnexpectedEncryptedIncrementalUpdateShouldBeDecryptedAndReCommitted) {
  // Init NIGORI with a single encryption key.
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  SetNigoriInFakeServer(BuildKeystoreNigoriSpecifics(
                            /*keybag_keys_params=*/{kKeystoreKeyParams},
                            /*keystore_decryptor_params=*/kKeystoreKeyParams,
                            /*keystore_key_params=*/kKeystoreKeyParams),
                        GetFakeServer());

  ASSERT_TRUE(SetupSync());

  // Despite BOOKMARKS not being an encrypted type, send an update encrypted
  // with the single key known to this client. This happens after SetupSync(),
  // so it's an incremental update.
  ASSERT_FALSE(
      GetSyncService(0)->GetUserSettings()->GetEncryptedDataTypes().Has(
          syncer::ModelType::BOOKMARKS));
  const std::string kTitle = "Bookmark title";
  const GURL kUrl = GURL("https://g.com");
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmarks_helper::CreateBookmarkServerEntity(kTitle, kUrl);
  bookmark->SetSpecifics(syncer::GetEncryptedBookmarkEntitySpecifics(
      bookmark->GetSpecifics().bookmark(), kKeystoreKeyParams));
  GetFakeServer()->InjectEntity(std::move(bookmark));

  // The client should decrypt the update and re-commit an unencrypted version.
  EXPECT_TRUE(bookmarks_helper::BookmarksTitleChecker(0, kTitle, 1).Wait());
  EXPECT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {{kTitle, kUrl}},
                  /*cryptographer=*/nullptr)
                  .Wait());
}

// Tests that client can decrypt passwords, encrypted with default key, while
// Nigori node is in backward-compatible keystore mode (i.e. default key isn't
// a keystore key, but keystore decryptor token contains this key and encrypted
// with a keystore key).
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTest,
                       ShouldDecryptWithBackwardCompatibleKeystoreNigori) {
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  const KeyParamsForTesting kDefaultKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("password");
  SetNigoriInFakeServer(
      BuildKeystoreNigoriSpecifics(
          /*keybag_keys_params=*/{kDefaultKeyParams, kKeystoreKeyParams},
          /*keystore_decryptor_params*/ {kDefaultKeyParams},
          /*keystore_key_params=*/kKeystoreKeyParams),
      GetFakeServer());
  const password_manager::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(0);
  passwords_helper::InjectEncryptedServerPassword(
      password_form, kDefaultKeyParams.password,
      kDefaultKeyParams.derivation_params, GetFakeServer());
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForPasswordForms({password_form}));
}

IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTest, ShouldRotateKeystoreKey) {
  ASSERT_TRUE(SetupSync());

  GetFakeServer()->TriggerKeystoreKeyRotation();
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(2));
  const KeyParamsForTesting new_keystore_key_params =
      KeystoreKeyParamsForTesting(keystore_keys[1]);
  const std::string expected_key_bag_key_name =
      ComputeKeyName(new_keystore_key_params);
  EXPECT_TRUE(ServerNigoriKeyNameChecker(expected_key_bag_key_name).Wait());
}

// Performs initial sync with backward compatible keystore Nigori.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTest,
                       PRE_ShouldCompleteKeystoreMigrationAfterRestart) {
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  const KeyParamsForTesting kDefaultKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("password");
  SetNigoriInFakeServer(
      BuildKeystoreNigoriSpecifics(
          /*keybag_keys_params=*/{kDefaultKeyParams, kKeystoreKeyParams},
          /*keystore_decryptor_params*/ {kDefaultKeyParams},
          /*keystore_key_params=*/kKeystoreKeyParams),
      GetFakeServer());

  ASSERT_TRUE(SetupSync());
  const std::string expected_key_bag_key_name =
      ComputeKeyName(kKeystoreKeyParams);
}

// After browser restart the client should commit full keystore Nigori (e.g. it
// should use keystore key as encryption key).
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTest,
                       ShouldCompleteKeystoreMigrationAfterRestart) {
  ASSERT_TRUE(SetupClients());
  const std::string expected_key_bag_key_name =
      ComputeKeyName(KeystoreKeyParamsForTesting(
          /*raw_key=*/GetFakeServer()->GetKeystoreKeys().back()));
  EXPECT_TRUE(ServerNigoriKeyNameChecker(expected_key_bag_key_name).Wait());
}

// Tests that client can decrypt |pending_keys| with implicit passphrase in
// backward-compatible keystore mode, when |keystore_decryptor_token| is
// non-decryptable (corrupted). Additionally verifies that there is no
// regression causing crbug.com/1042203.
IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriSyncTest,
    ShouldDecryptWithImplicitPassphraseInBackwardCompatibleKeystoreMode) {
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));

  // Emulates mismatch between keystore key returned by the server and keystore
  // key used in NigoriSpecifics.
  std::vector<uint8_t> corrupted_keystore_key = keystore_keys[0];
  corrupted_keystore_key.push_back(42u);
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(corrupted_keystore_key);
  const KeyParamsForTesting kDefaultKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("password");
  SetNigoriInFakeServer(
      BuildKeystoreNigoriSpecifics(
          /*keybag_keys_params=*/{kDefaultKeyParams, kKeystoreKeyParams},
          /*keystore_decryptor_params*/ {kDefaultKeyParams},
          /*keystore_key_params=*/kKeystoreKeyParams),
      GetFakeServer());

  const password_manager::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(0);
  passwords_helper::InjectEncryptedServerPassword(
      password_form, kDefaultKeyParams.password,
      kDefaultKeyParams.derivation_params, GetFakeServer());
  ASSERT_TRUE(SetupSync(NO_WAITING));

  EXPECT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      "password"));
  EXPECT_TRUE(WaitForPasswordForms({password_form}));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriSyncTest,
    ShouldFollowRewritingKeystoreMigrationWhenDataNonDecryptable) {
  // Setup with implicit passphrase.
  const KeyParamsForTesting kPassphraseKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("passphrase");
  sync_pb::NigoriSpecifics specifics;
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          kPassphraseKeyParams.password,
          kPassphraseKeyParams.derivation_params);
  ASSERT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));
  SetNigoriInFakeServer(specifics, GetFakeServer());

  // Mimic passwords encrypted with implicit passphrase stored by the server.
  const password_manager::PasswordForm password_form1 =
      passwords_helper::CreateTestPasswordForm(1);
  passwords_helper::InjectEncryptedServerPassword(
      password_form1, kPassphraseKeyParams.password,
      kPassphraseKeyParams.derivation_params, GetFakeServer());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());

  // Add local passwords.
  const password_manager::PasswordForm password_form2 =
      passwords_helper::CreateTestPasswordForm(2);
  passwords_helper::GetProfilePasswordStoreInterface(0)->AddLogin(
      password_form2);

  // Mimic server-side keystore migration:
  // 1. Issue CLIENT_DATA_OBSOLETE.
  // 2. Delete server-side passwords (without creating tombstones).
  // 3. Rewrite server-side nigori with keystore one (this also triggers an
  // invalidation, so client should see CLIENT_DATA_OBSOLETE).
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::CLIENT_DATA_OBSOLETE);
  GetFakeServer()->DeleteAllEntitiesForModelType(syncer::PASSWORDS);

  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  SetNigoriInFakeServer(BuildKeystoreNigoriSpecifics(
                            /*keybag_keys_params=*/{kKeystoreKeyParams},
                            /*keystore_decryptor_params*/ {kKeystoreKeyParams},
                            /*keystore_key_params=*/kKeystoreKeyParams),
                        GetFakeServer());
  // Nigori change triggers invalidation, so client should observe
  // CLIENT_DATA_OBSOLETE and stop the engine.
  ASSERT_TRUE(syncer::SyncEngineStoppedChecker(GetSyncService(0)).Wait());

  // Make server return SUCCESS so that sync can initialize.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::SUCCESS);
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // Verify client and server side state (|password_form1| is lost, while
  // |password_form2| is retained and committed to the server).
  EXPECT_TRUE(WaitForPasswordForms({password_form2}));
  EXPECT_TRUE(ServerPasswordsEqualityChecker(
                  {password_form2}, kKeystoreKeyParams.password,
                  kKeystoreKeyParams.derivation_params)
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriSyncTest,
    ShouldFollowRewritingKeystoreMigrationWhenDataDecryptable) {
  // Setup with implicit passphrase.
  const KeyParamsForTesting kPassphraseKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("passphrase");
  sync_pb::NigoriSpecifics specifics;
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          kPassphraseKeyParams.password,
          kPassphraseKeyParams.derivation_params);
  ASSERT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));
  SetNigoriInFakeServer(specifics, GetFakeServer());

  // Mimic passwords encrypted with implicit passphrase stored by the server.
  const password_manager::PasswordForm password_form1 =
      passwords_helper::CreateTestPasswordForm(1);
  passwords_helper::InjectEncryptedServerPassword(
      password_form1, kPassphraseKeyParams.password,
      kPassphraseKeyParams.derivation_params, GetFakeServer());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());

  // Mimic that passphrase is provided by the user.
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kPassphraseKeyParams.password));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(WaitForPasswordForms({password_form1}));

  // Add local passwords.
  const password_manager::PasswordForm password_form2 =
      passwords_helper::CreateTestPasswordForm(2);
  passwords_helper::GetProfilePasswordStoreInterface(0)->AddLogin(
      password_form2);

  // Mimic server-side keystore migration:
  // 1. Issue CLIENT_DATA_OBSOLETE.
  // 2. Delete server-side passwords (without creating tombstones).
  // 3. Rewrite server-side nigori with keystore one (this also triggers an
  // invalidation, so client should see CLIENT_DATA_OBSOLETE).
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::CLIENT_DATA_OBSOLETE);
  GetFakeServer()->DeleteAllEntitiesForModelType(syncer::PASSWORDS);

  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  SetNigoriInFakeServer(BuildKeystoreNigoriSpecifics(
                            /*keybag_keys_params=*/{kKeystoreKeyParams},
                            /*keystore_decryptor_params*/ {kKeystoreKeyParams},
                            /*keystore_key_params=*/kKeystoreKeyParams),
                        GetFakeServer());
  // Nigori change triggers invalidation, so client should observe
  // CLIENT_DATA_OBSOLETE and stop the engine.
  ASSERT_TRUE(syncer::SyncEngineStoppedChecker(GetSyncService(0)).Wait());

  // Make server return SUCCESS so that sync can initialize.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::SUCCESS);
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // Verify client and server side state. Both passwords should be stored and
  // encrypted with keystore passphrase.
  EXPECT_TRUE(WaitForPasswordForms({password_form1, password_form2}));
  EXPECT_TRUE(ServerPasswordsEqualityChecker(
                  {password_form1, password_form2}, kKeystoreKeyParams.password,
                  kKeystoreKeyParams.derivation_params)
                  .Wait());
}

// Performs initial sync for Nigori, but doesn't allow initialized Nigori to be
// committed.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTestWithNotAwaitQuiescence,
                       PRE_ShouldCompleteKeystoreInitializationAfterRestart) {
  GetFakeServer()->TriggerCommitError(sync_pb::SyncEnums::THROTTLED);

  // Do not wait for commits due to commit error.
  ASSERT_TRUE(SetupSync(WAIT_FOR_SYNC_SETUP_TO_COMPLETE));

  sync_pb::NigoriSpecifics specifics;
  ASSERT_TRUE(GetServerNigori(GetFakeServer(), &specifics));
  ASSERT_THAT(specifics.passphrase_type(),
              Eq(sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE));
}

// After browser restart the client should commit initialized Nigori.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTestWithNotAwaitQuiescence,
                       ShouldCompleteKeystoreInitializationAfterRestart) {
  sync_pb::NigoriSpecifics specifics;
  ASSERT_TRUE(GetServerNigori(GetFakeServer(), &specifics));
  ASSERT_THAT(specifics.passphrase_type(),
              Eq(sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE));

  ASSERT_TRUE(SetupClients());
  EXPECT_TRUE(
      ServerPassphraseTypeChecker(syncer::PassphraseType::kKeystorePassphrase)
          .Wait());
}

class SingleClientNigoriWithWebApiTest : public SyncTest {
 public:
  SingleClientNigoriWithWebApiTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientNigoriWithWebApiTest(const SingleClientNigoriWithWebApiTest&) =
      delete;
  SingleClientNigoriWithWebApiTest& operator=(
      const SingleClientNigoriWithWebApiTest&) = delete;

  ~SingleClientNigoriWithWebApiTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    const GURL& base_url = embedded_test_server()->base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(
        syncer::kTrustedVaultServiceURL,
        trusted_vault::FakeSecurityDomainsServer::GetServerURL(
            embedded_test_server()->base_url())
            .spec());

    SyncTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    security_domains_server_ =
        std::make_unique<trusted_vault::FakeSecurityDomainsServer>(
            embedded_test_server()->base_url());
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &trusted_vault::FakeSecurityDomainsServer::HandleRequest,
        base::Unretained(security_domains_server_.get())));

    // Install a redirect from the actual degraded recoverability URL as
    // determined by GaiaUrls to |recoverability_url|, which runs Javascript
    // code to mimic adding recovery method with key
    // |kTestRecoveryMethodPublicKey|. Note that this needs to be installed
    // before the analogous below for retrieval, because they share prefix.
    const GURL recoverability_url = GetFakeTrustedVaultRecoverabilityURL(
        *embedded_test_server(), kTestRecoveryMethodPublicKey);
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &HttpServerRedirect,
        /*from_prefix=*/
        GaiaUrls::GetInstance()
            ->signin_chrome_sync_keys_recoverability_degraded_url(),
        /*to=*/recoverability_url));

    // Install a redirect from the actual retrieval URL as determined by
    // GaiaUrls to |retrieval_url|, which runs Javascript code to mimic
    // retrieval of key |kTestEncryptionKey|.
    const GURL retrieval_url = GetFakeTrustedVaultRetrievalURL(
        *embedded_test_server(), kTestEncryptionKey);
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &HttpServerRedirect,
        /*from_prefix=*/
        GaiaUrls::GetInstance()->signin_chrome_sync_keys_retrieval_url(),
        /*to=*/retrieval_url));

    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDown() override {
    // Test server shutdown is required before |security_domains_server_| can be
    // destroyed.
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    SyncTest::TearDown();
  }

  trusted_vault::FakeSecurityDomainsServer* GetSecurityDomainsServer() {
    return security_domains_server_.get();
  }

 protected:
  // Arbitrary encryption key that the Gaia retrieval page returns via
  // Javascript API if the retrieval page is visited.
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};

  // Arbitrary (but valid) public key of a recovery method that gets
  // automatically added if the Gaia recoverability page is visited.
  const std::vector<uint8_t> kTestRecoveryMethodPublicKey =
      trusted_vault::SecureBoxKeyPair::GenerateRandom()
          ->public_key()
          .ExportToBytes();

 private:
  std::unique_ptr<trusted_vault::FakeSecurityDomainsServer>
      security_domains_server_;
};

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldAcceptEncryptionKeysFromTheWebIfSyncEnabled) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(ShouldShowSyncKeysMissingError(GetSyncService(0),
                                             GetProfile(0)->GetPrefs()));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Verify the profile-menu error string.
  ASSERT_THAT(
      GetAvatarSyncErrorType(GetProfile(0)),
      Eq(AvatarSyncErrorType::kTrustedVaultKeyMissingForPasswordsError));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // Verify the string that would be displayed in settings.
  ASSERT_THAT(GetSyncStatusLabels(GetProfile(0)),
              StatusLabelsMatch(
                  SyncStatusMessageType::kPasswordsOnlySyncError,
                  IDS_SETTINGS_EMPTY_STRING, IDS_SYNC_STATUS_NEEDS_KEYS_BUTTON,
                  SyncStatusActionType::kRetrieveTrustedVaultKeys));

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  // Mimic opening a web page where the user can interact with the retrieval
  // flow.
  OpenTabForSyncKeyRetrieval(
      GetBrowser(0), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes, which indicates successful completion.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());

  EXPECT_TRUE(PasswordSyncActiveChecker(GetSyncService(0)).Wait());
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  EXPECT_FALSE(ShouldShowSyncKeysMissingError(GetSyncService(0),
                                              GetProfile(0)->GetPrefs()));
  EXPECT_THAT(GetSyncStatusLabels(GetProfile(0)),
              StatusLabelsMatch(
                  SyncStatusMessageType::kSynced, IDS_SYNC_ACCOUNT_SYNCING,
                  IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Verify the profile-menu error string is empty.
  EXPECT_FALSE(GetAvatarSyncErrorType(GetProfile(0)).has_value());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldAcceptTrustedVaultKeysUponAshSystemNotification) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  ASSERT_TRUE(SetupClients());

  NotificationDisplayServiceTester display_service(GetProfile(0));

  // SyncErrorNotifier needs explicit instantiation in tests, because the test
  // profile at hands doesn't exercise ChromeBrowserMainExtraPartsAsh.
  const ash::SyncErrorNotifier* const sync_error_notifier =
      ash::SyncErrorNotifierFactory::GetForProfile(GetProfile(0));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Verify that a notification was displayed.
  const std::string notification_id =
      sync_error_notifier->GetNotificationIdForTesting();
  absl::optional<message_center::Notification> notification =
      display_service.GetNotification(notification_id);
  ASSERT_TRUE(notification);
  EXPECT_THAT(notification->title(),
              Eq(l10n_util::GetStringUTF16(
                  IDS_SYNC_ERROR_PASSWORDS_BUBBLE_VIEW_TITLE)));
  EXPECT_THAT(
      notification->message(),
      Eq(l10n_util::GetStringUTF16(
          IDS_SYNC_NEEDS_KEYS_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE)));

  // Mimic the user clickling on the system notification, which opens up a
  // tab where the user can interact with the retrieval flow.
  display_service.SimulateClick(NotificationHandler::Type::TRANSIENT,
                                notification_id, /*action_index=*/absl::nullopt,
                                /*reply=*/absl::nullopt);

  // Wait until the page closes, which indicates successful completion.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());

  EXPECT_TRUE(PasswordSyncActiveChecker(GetSyncService(0)).Wait());
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    ShouldImproveTrustedVaultRecoverabilityUponAshSystemNotification) {
  // Mimic the key being available upon startup but recoverability degraded.
  const std::vector<uint8_t> trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{trusted_vault_key}),
                        GetFakeServer());
  ASSERT_TRUE(SetupClients());
  GetSyncService(0)->AddTrustedVaultDecryptionKeysFromWeb(
      kGaiaId, GetSecurityDomainsServer()->GetAllTrustedVaultKeys(),
      /*last_key_version=*/GetSecurityDomainsServer()->GetCurrentEpoch());

  NotificationDisplayServiceTester display_service(GetProfile(0));

  // SyncErrorNotifier needs explicit instantiation in tests, because the test
  // profile at hands doesn't exercise ChromeBrowserMainExtraPartsAsh.
  const ash::SyncErrorNotifier* const sync_error_notifier =
      ash::SyncErrorNotifierFactory::GetForProfile(GetProfile(0));

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/true)
                  .Wait());

  // Verify that a notification was displayed.
  const std::string notification_id =
      sync_error_notifier->GetNotificationIdForTesting();
  absl::optional<message_center::Notification> notification =
      display_service.GetNotification(notification_id);
  ASSERT_TRUE(notification);
  EXPECT_THAT(notification->title(),
              Eq(l10n_util::GetStringUTF16(
                  IDS_SYNC_NEEDS_VERIFICATION_BUBBLE_VIEW_TITLE)));
  EXPECT_THAT(
      notification->message(),
      Eq(l10n_util::GetStringUTF16(
          IDS_SYNC_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE)));

  // Mimic the user clickling on the system notification, which opens up a
  // tab where the user can interact with the degraded recoverability flow.
  display_service.SimulateClick(NotificationHandler::Type::TRANSIENT,
                                notification_id, /*action_index=*/absl::nullopt,
                                /*reply=*/absl::nullopt);

  // Wait until the page closes, which indicates successful completion.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());

  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldAcceptEncryptionKeysFromSubFrameIfSyncEnabled) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Mimic opening a page that embeds the retrieval page as a cross-origin
  // iframe.
  chrome::AddTabAt(
      GetBrowser(0),
      embedded_test_server()->GetURL(
          "foo.com", base::StringPrintf(
                         "/sync/encryption_keys_retrieval_with_iframe.html?%s",
                         GaiaUrls::GetInstance()
                             ->signin_chrome_sync_keys_retrieval_url()
                             .spec()
                             .c_str())),
      /*index=*/0,
      /*foreground=*/true);

  // Wait until the keys-missing error gets resolved.
  EXPECT_TRUE(PasswordSyncActiveChecker(GetSyncService(0)).Wait());
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
}

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       PRE_ShouldAcceptEncryptionKeysFromTheWebBeforeSignIn) {
  ASSERT_TRUE(SetupClients());

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  // Mimic opening a web page where the user can interact with the retrieval
  // flow, while the user is signed out.
  OpenTabForSyncKeyRetrieval(
      GetBrowser(0), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes, which indicates successful completion.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldAcceptEncryptionKeysFromTheWebBeforeSignIn) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  // Sign in and start sync.
  EXPECT_TRUE(SetupSync());

  ASSERT_THAT(GetSyncService(0)->GetUserSettings()->GetPassphraseType(),
              Eq(syncer::PassphraseType::kTrustedVaultPassphrase));
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultRecoverabilityDegraded());
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  EXPECT_FALSE(ShouldShowSyncKeysMissingError(GetSyncService(0),
                                              GetProfile(0)->GetPrefs()));
  EXPECT_FALSE(ShouldShowTrustedVaultDegradedRecoverabilityError(
      GetSyncService(0), GetProfile(0)->GetPrefs()));
  EXPECT_THAT(GetSyncStatusLabels(GetProfile(0)),
              StatusLabelsMatch(
                  SyncStatusMessageType::kSynced, IDS_SYNC_ACCOUNT_SYNCING,
                  IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Verify the profile-menu error string is empty.
  EXPECT_FALSE(GetAvatarSyncErrorType(GetProfile(0)).has_value());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    PRE_ShouldClearEncryptionKeysFromTheWebWhenSigninCookiesCleared) {
  ASSERT_TRUE(SetupClients());

  // Explicitly add signin cookie (normally it would be done during the keys
  // retrieval or before it).
  cookie_helper::AddSigninCookie(GetProfile(0));

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  TrustedVaultKeysChangedStateChecker keys_fetched_checker(GetSyncService(0));
  // Mimic opening a web page where the user can interact with the retrieval
  // flow, while the user is signed out.
  OpenTabForSyncKeyRetrieval(
      GetBrowser(0), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes, which indicates successful completion.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());
  EXPECT_TRUE(keys_fetched_checker.Wait());

  // TrustedVaultClient handles IdentityManager state changes after refresh
  // tokens are loaded.
  // TODO(crbug.com/1148328): |keys_cleared_checker| should be sufficient alone
  // once test properly manipulates AccountsInCookieJarInfo (this likely
  // involves using FakeGaia).
  signin::WaitForRefreshTokensLoaded(
      IdentityManagerFactory::GetForProfile(GetProfile(0)));

  // Mimic signin cookie clearing.
  TrustedVaultKeysChangedStateChecker keys_cleared_checker(GetSyncService(0));
  cookie_helper::DeleteSigninCookies(GetProfile(0));
  EXPECT_TRUE(keys_cleared_checker.Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    ShouldClearEncryptionKeysFromTheWebWhenSigninCookiesCleared) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  // Sign in and start sync.
  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  EXPECT_TRUE(ShouldShowSyncKeysMissingError(GetSyncService(0),
                                             GetProfile(0)->GetPrefs()));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    ShouldRemotelyTransitFromTrustedVaultToKeystorePassphrase) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(ShouldShowSyncKeysMissingError(GetSyncService(0),
                                             GetProfile(0)->GetPrefs()));

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  // Mimic opening a web page where the user can interact with the retrieval
  // flow.
  OpenTabForSyncKeyRetrieval(
      GetBrowser(0), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes, which indicates successful completion.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());

  // Mimic remote transition to keystore passphrase.
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  const KeyParamsForTesting kTrustedVaultKeyParams =
      TrustedVaultKeyParamsForTesting(kTestEncryptionKey);
  SetNigoriInFakeServer(
      BuildKeystoreNigoriSpecifics(
          /*keybag_keys_params=*/{kTrustedVaultKeyParams, kKeystoreKeyParams},
          /*keystore_decryptor_params*/ {kKeystoreKeyParams},
          /*keystore_key_params=*/kKeystoreKeyParams),
      GetFakeServer());

  // Ensure that client can decrypt with both |kTrustedVaultKeyParams|
  // and |kKeystoreKeyParams|.
  const password_manager::PasswordForm password_form1 =
      passwords_helper::CreateTestPasswordForm(1);
  const password_manager::PasswordForm password_form2 =
      passwords_helper::CreateTestPasswordForm(2);

  passwords_helper::InjectEncryptedServerPassword(
      password_form1, kKeystoreKeyParams.password,
      kKeystoreKeyParams.derivation_params, GetFakeServer());
  passwords_helper::InjectEncryptedServerPassword(
      password_form2, kTrustedVaultKeyParams.password,
      kTrustedVaultKeyParams.derivation_params, GetFakeServer());

  EXPECT_TRUE(PasswordFormsChecker(0, {password_form1, password_form2}).Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    ShouldRemotelyTransitFromTrustedVaultToCustomPassphrase) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(ShouldShowSyncKeysMissingError(GetSyncService(0),
                                             GetProfile(0)->GetPrefs()));

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  // Mimic opening a web page where the user can interact with the retrieval
  // flow.
  OpenTabForSyncKeyRetrieval(
      GetBrowser(0), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes, which indicates successful completion.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());

  // Mimic remote transition to custom passphrase.
  const KeyParamsForTesting kCustomPassphraseKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("passphrase");
  const KeyParamsForTesting kTrustedVaultKeyParams =
      TrustedVaultKeyParamsForTesting(kTestEncryptionKey);
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(
                            kCustomPassphraseKeyParams, kTrustedVaultKeyParams),
                        GetFakeServer());

  EXPECT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kCustomPassphraseKeyParams.password));
  EXPECT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());

  // Ensure that client can decrypt with both |kTrustedVaultKeyParams|
  // and |kCustomPassphraseKeyParams|.
  const password_manager::PasswordForm password_form1 =
      passwords_helper::CreateTestPasswordForm(1);
  const password_manager::PasswordForm password_form2 =
      passwords_helper::CreateTestPasswordForm(2);

  passwords_helper::InjectEncryptedServerPassword(
      password_form1, kCustomPassphraseKeyParams.password,
      kCustomPassphraseKeyParams.derivation_params, GetFakeServer());
  passwords_helper::InjectEncryptedServerPassword(
      password_form2, kTrustedVaultKeyParams.password,
      kTrustedVaultKeyParams.derivation_params, GetFakeServer());

  EXPECT_TRUE(PasswordFormsChecker(0, {password_form1, password_form2}).Wait());
}

// Flakily failing on Mac: https://crbug.com/1447428
#if BUILDFLAG(IS_MAC)
#define MAYBE_ShoudRecordTrustedVaultErrorShownOnStartupWhenErrorShown \
  DISABLED_ShoudRecordTrustedVaultErrorShownOnStartupWhenErrorShown
#else
#define MAYBE_ShoudRecordTrustedVaultErrorShownOnStartupWhenErrorShown \
  ShoudRecordTrustedVaultErrorShownOnStartupWhenErrorShown
#endif
IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    MAYBE_ShoudRecordTrustedVaultErrorShownOnStartupWhenErrorShown) {
  // 4 days is an arbitrary value between 3 days and 7 days to allow testing
  // histogram suffixes.
  const base::Time migration_time = base::Time::Now() - base::Days(4);

  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(
      BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}, migration_time),
      GetFakeServer());

  base::HistogramTester histogram_tester;

  // The manual sequence below, instead of invoking SetupSync() manually,
  // reproduces a more realistic case of the first-time turn-sync-on experience,
  // with a temporary stage where the user is signed in without sync-the-feature
  // being enabled. Except on Ash where the two steps happen at once.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(ShouldShowSyncKeysMissingError(GetSyncService(0),
                                             GetProfile(0)->GetPrefs()));

  histogram_tester.ExpectUniqueSample("Sync.TrustedVaultErrorShownOnStartup",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultErrorShownOnStartup.MigratedLast28Days",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultErrorShownOnStartup.MigratedLast7Days",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Sync.TrustedVaultErrorShownOnStartup.MigratedLast3Days",
      /*count=*/0);
  histogram_tester.ExpectTotalCount(
      "Sync.TrustedVaultErrorShownOnStartup.MigratedLastDay",
      /*count=*/0);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultErrorShownOnFirstTimeSync2",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    PRE_ShoudRecordTrustedVaultErrorShownOnStartupWhenErrorNotShown) {
  ASSERT_TRUE(SetupClients());

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  TrustedVaultKeysChangedStateChecker keys_fetched_checker(GetSyncService(0));
  // Mimic opening a web page where the user can interact with the retrieval
  // flow, while the user is signed out.
  OpenTabForSyncKeyRetrieval(
      GetBrowser(0), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes, which indicates successful completion.
  ASSERT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());
  ASSERT_TRUE(keys_fetched_checker.Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    ShoudRecordTrustedVaultErrorShownOnStartupWhenErrorNotShown) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_FALSE(ShouldShowSyncKeysMissingError(GetSyncService(0),
                                              GetProfile(0)->GetPrefs()));

  histogram_tester.ExpectUniqueSample("Sync.TrustedVaultErrorShownOnStartup",
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldReportDegradedTrustedVaultRecoverability) {
  base::HistogramTester histogram_tester;

  // 4 days is an arbitrary value between 3 days and 7 days to allow testing
  // histogram suffixes.
  const base::Time migration_time = base::Time::Now() - base::Days(4);

  // Mimic the key being available upon startup but recoverability degraded.
  const std::vector<uint8_t> trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  SetNigoriInFakeServer(
      BuildTrustedVaultNigoriSpecifics(
          /*trusted_vault_keys=*/{trusted_vault_key}, migration_time),
      GetFakeServer());
  ASSERT_TRUE(SetupClients());
  GetSyncService(0)->AddTrustedVaultDecryptionKeysFromWeb(
      kGaiaId, GetSecurityDomainsServer()->GetAllTrustedVaultKeys(),
      /*last_key_version=*/GetSecurityDomainsServer()->GetCurrentEpoch());
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/true)
                  .Wait());

  EXPECT_TRUE(ShouldShowTrustedVaultDegradedRecoverabilityError(
      GetSyncService(0), GetProfile(0)->GetPrefs()));

  ASSERT_THAT(GetSyncService(0)->GetUserSettings()->GetPassphraseType(),
              Eq(syncer::PassphraseType::kTrustedVaultPassphrase));
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(ShouldShowSyncKeysMissingError(GetSyncService(0),
                                              GetProfile(0)->GetPrefs()));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Verify the profile-menu error string.
  EXPECT_THAT(GetAvatarSyncErrorType(GetProfile(0)),
              Eq(AvatarSyncErrorType::
                     kTrustedVaultRecoverabilityDegradedForPasswordsError));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // No messages expected in settings.
  EXPECT_THAT(GetSyncStatusLabels(GetProfile(0)),
              StatusLabelsMatch(
                  SyncStatusMessageType::kSynced, IDS_SYNC_ACCOUNT_SYNCING,
                  IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction));

  // Mimic opening a web page where the user can interact with the degraded
  // recoverability flow. Before that, there needs to be an existing tab for the
  // second tab to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);
  OpenTabForSyncKeyRecoverabilityDegraded(
      GetBrowser(0), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());
  EXPECT_FALSE(ShouldShowTrustedVaultDegradedRecoverabilityError(
      GetSyncService(0), GetProfile(0)->GetPrefs()));
  EXPECT_FALSE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Verify the profile-menu error string is empty.
  EXPECT_FALSE(GetAvatarSyncErrorType(GetProfile(0)).has_value());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup",
      /*sample=*/true, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLast28Days",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLast7Days",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLast3Days",
      /*count=*/0);
  histogram_tester.ExpectTotalCount(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLastDay",
      /*count=*/0);

  // TODO(crbug.com/1201659): Verify the recovery method hint added to the fake
  // server.
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    ShouldDeferAddingTrustedVaultRecoverabilityMethodUntilSignIn) {
  const int kTestMethodTypeHint = 8;

  // Mimic the account being already using a trusted vault passphrase.
  const std::vector<uint8_t> trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{trusted_vault_key}),
                        GetFakeServer());
  ASSERT_TRUE(SetupClients());

  // Mimic the key being available upon startup but recoverability degraded.
  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  GetSyncService(0)->AddTrustedVaultDecryptionKeysFromWeb(
      kGaiaId, GetSecurityDomainsServer()->GetAllTrustedVaultKeys(),
      /*last_key_version=*/GetSecurityDomainsServer()->GetCurrentEpoch());

  // Mimic a recovery method being added before or during sign-in, which should
  // be deferred until sign-in completes.
  base::RunLoop run_loop;
  GetSyncService(0)->AddTrustedVaultRecoveryMethodFromWeb(
      kGaiaId, kTestRecoveryMethodPublicKey, kTestMethodTypeHint,
      run_loop.QuitClosure());

  ASSERT_TRUE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());

  // Sign in now and wait until sync initializes.
  ASSERT_TRUE(SetupSync());

  // Wait until AddTrustedVaultRecoveryMethodFromWeb() completes.
  run_loop.Run();

  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());
  EXPECT_FALSE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    ShouldDeferAddingTrustedVaultRecoverabilityMethodUntilAuthErrorFixed) {
  const int kTestMethodTypeHint = 8;

  // Mimic the account being already using a trusted vault passphrase.
  const std::vector<uint8_t> trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{trusted_vault_key}),
                        GetFakeServer());
  ASSERT_TRUE(SetupClients());

  // Mimic the key being available upon startup but recoverability degraded.
  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  GetSyncService(0)->AddTrustedVaultDecryptionKeysFromWeb(
      kGaiaId, GetSecurityDomainsServer()->GetAllTrustedVaultKeys(),
      /*last_key_version=*/GetSecurityDomainsServer()->GetCurrentEpoch());
  ASSERT_TRUE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());

  // Sign in now and wait until sync initializes.
  ASSERT_TRUE(SetupSync());

  // Enter a persistent auth error state.
  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  ASSERT_TRUE(GetSyncService(0)->GetAuthError().IsPersistentError());

  // Mimic a recovery method being added during a persistent auth error, which
  // should be deferred until the auth error is resolved.
  base::RunLoop run_loop;
  GetSyncService(0)->AddTrustedVaultRecoveryMethodFromWeb(
      kGaiaId, kTestRecoveryMethodPublicKey, kTestMethodTypeHint,
      run_loop.QuitClosure());

  // Mimic the auth error state being resolved.
  ASSERT_TRUE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
  GetClient(0)->ExitSyncPausedStateForPrimaryAccount();

  // Wait until AddTrustedVaultRecoveryMethodFromWeb() completes.
  run_loop.Run();

  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());
  EXPECT_FALSE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    ShouldReportDegradedTrustedVaultRecoverabilityUponResolvedAuthError) {
  // Mimic the key being available upon startup and recoverability good (not
  // degraded).
  const std::vector<uint8_t> trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{trusted_vault_key}),
                        GetFakeServer());
  ASSERT_TRUE(SetupClients());
  GetSyncService(0)->AddTrustedVaultDecryptionKeysFromWeb(
      kGaiaId, GetSecurityDomainsServer()->GetAllTrustedVaultKeys(),
      /*last_key_version=*/GetSecurityDomainsServer()->GetCurrentEpoch());
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
  ASSERT_FALSE(ShouldShowTrustedVaultDegradedRecoverabilityError(
      GetSyncService(0), GetProfile(0)->GetPrefs()));

  // Mimic a server-side persistent auth error together with a degraded
  // recoverability, such as an account recovery flow that resets the account
  // password.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      IdentityManagerFactory::GetForProfile(GetProfile(0)),
      GetSyncService(0)->GetAccountInfo().account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);

  // Mimic resolving the auth error (e.g. user reauth).
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      IdentityManagerFactory::GetForProfile(GetProfile(0)),
      GetSyncService(0)->GetAccountInfo().account_id, GoogleServiceAuthError());

  // The recoverability state should be immediately refreshed.
  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/true)
                  .Wait());
}

// Device registration attempt should be taken upon sign in into primary
// profile. It should be successful when security domain server allows device
// registration with constant key.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldRegisterDeviceWithConstantKey) {
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/1113599): consider checking member public key (requires
  // either ability to overload key generator in the test or exposing public key
  // from the client).
  EXPECT_TRUE(FakeSecurityDomainsServerMemberStatusChecker(
                  /*expected_member_count=*/1,
                  /*expected_trusted_vault_key=*/
                  trusted_vault::GetConstantTrustedVaultKey(),
                  GetSecurityDomainsServer())
                  .Wait());
  EXPECT_FALSE(GetSecurityDomainsServer()->ReceivedInvalidRequest());
}

// If device was successfully registered with constant key, it should silently
// follow key rotation and transit to trusted vault passphrase without going
// through key retrieval flow.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldFollowInitialKeyRotation) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(FakeSecurityDomainsServerMemberStatusChecker(
                  /*expected_member_count=*/1,
                  /*expected_trusted_vault_key=*/
                  trusted_vault::GetConstantTrustedVaultKey(),
                  GetSecurityDomainsServer())
                  .Wait());

  // Rotate trusted vault key and mimic transition to trusted vault passphrase
  // type.
  base::HistogramTester histogram_tester;
  std::vector<uint8_t> new_trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{new_trusted_vault_key}),
                        GetFakeServer());

  // Inject password encrypted with trusted vault key and verify client is able
  // to decrypt it.
  const KeyParamsForTesting trusted_vault_key_params =
      TrustedVaultKeyParamsForTesting(new_trusted_vault_key);
  const password_manager::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(0);
  passwords_helper::InjectEncryptedServerPassword(
      password_form, trusted_vault_key_params.password,
      trusted_vault_key_params.derivation_params, GetFakeServer());
  EXPECT_TRUE(PasswordFormsChecker(0, {password_form}).Wait());
  EXPECT_FALSE(GetSecurityDomainsServer()->ReceivedInvalidRequest());

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDownloadKeysStatus",
      /*sample=*/trusted_vault::TrustedVaultDownloadKeysStatus::kSuccess,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultURLFetchResponse.DownloadKeys",
      /*sample=*/200,
      /*expected_bucket_count=*/1);
}

// Regression test for crbug.com/1267391: after following key rotation the
// client should still send all trusted vault keys (including keys that predate
// key rotation) to the server when adding recovery method.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldFollowKeyRotationAndAddRecoveryMethod) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(FakeSecurityDomainsServerMemberStatusChecker(
                  /*expected_member_count=*/1,
                  /*expected_trusted_vault_key=*/
                  trusted_vault::GetConstantTrustedVaultKey(),
                  GetSecurityDomainsServer())
                  .Wait());

  std::vector<uint8_t> new_trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  // Trigger following key rotation client-side.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{new_trusted_vault_key}),
                        GetFakeServer());

  const int kTestMethodTypeHint = 8;

  // Enter degraded recoverability state.
  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  ASSERT_TRUE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
  ASSERT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/true)
                  .Wait());

  // Mimic a recovery method being added.
  base::RunLoop run_loop;
  GetSyncService(0)->AddTrustedVaultRecoveryMethodFromWeb(
      kGaiaId, kTestRecoveryMethodPublicKey, kTestMethodTypeHint,
      run_loop.QuitClosure());
  run_loop.Run();

  // Verify that recovery method was added. Server rejects the request if client
  // didn't send all keys.
  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());
  EXPECT_FALSE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
}

// This test verifies that client handles security domain reset and able to
// register again after that and follow key rotation.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldFollowKeyRotationAfterSecurityDomainReset) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(FakeSecurityDomainsServerMemberStatusChecker(
                  /*expected_member_count=*/1,
                  /*expected_trusted_vault_key=*/
                  trusted_vault::GetConstantTrustedVaultKey(),
                  GetSecurityDomainsServer())
                  .Wait());

  // Rotate trusted vault key and mimic transition to trusted vault passphrase
  // type.
  std::vector<uint8_t> trusted_vault_key1 =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{trusted_vault_key1}),
                        GetFakeServer());

  // Ensure that client has finished following key rotation by verifying
  // passwords are decryptable.
  const KeyParamsForTesting trusted_vault_key_params1 =
      TrustedVaultKeyParamsForTesting(trusted_vault_key1);
  const password_manager::PasswordForm password_form1 =
      passwords_helper::CreateTestPasswordForm(1);
  passwords_helper::InjectEncryptedServerPassword(
      password_form1, trusted_vault_key_params1.password,
      trusted_vault_key_params1.derivation_params, GetFakeServer());
  ASSERT_TRUE(PasswordFormsChecker(0, {password_form1}).Wait());

  // Reset security domain state and mimic sync data reset.
  GetSecurityDomainsServer()->ResetData();
  GetFakeServer()->ClearServerData();

  // Wait until sync gets disabled to ensure client is aware of reset.
  ASSERT_TRUE(SyncDisabledChecker(GetSyncService(0)).Wait());

  // Make sure that client is able to follow key rotation with fresh security
  // domain state.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(FakeSecurityDomainsServerMemberStatusChecker(
                  /*expected_member_count=*/1,
                  /*expected_trusted_vault_key=*/
                  trusted_vault::GetConstantTrustedVaultKey(),
                  GetSecurityDomainsServer())
                  .Wait());

  std::vector<uint8_t> trusted_vault_key2 =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{trusted_vault_key2}),
                        GetFakeServer());

  const KeyParamsForTesting trusted_vault_key_params2 =
      TrustedVaultKeyParamsForTesting(trusted_vault_key2);
  const password_manager::PasswordForm password_form2 =
      passwords_helper::CreateTestPasswordForm(2);
  passwords_helper::InjectEncryptedServerPassword(
      password_form2, trusted_vault_key_params2.password,
      trusted_vault_key_params2.derivation_params, GetFakeServer());
  // |password_form1| has never been deleted locally, so client should have both
  // forms now.
  EXPECT_TRUE(PasswordFormsChecker(0, {password_form1, password_form2}).Wait());
  EXPECT_FALSE(GetSecurityDomainsServer()->ReceivedInvalidRequest());
}

// ChromeOS doesn't have unconsented primary accounts.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
class SingleClientNigoriWithWebApiAndPasswordsAccountStorageTest
    : public SingleClientNigoriWithWebApiTest {
 public:
  SingleClientNigoriWithWebApiAndPasswordsAccountStorageTest() {
    override_features_.InitAndEnableFeature(
        password_manager::features::kEnablePasswordsAccountStorage);
  }

  ~SingleClientNigoriWithWebApiAndPasswordsAccountStorageTest() override =
      default;

  // SetupClients() must have been already called.
  void SetupSyncTransport() {
    secondary_account_helper::SignInUnconsentedAccount(
        GetProfile(0), &test_url_loader_factory_, kAccountEmail);
    ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
    ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiAndPasswordsAccountStorageTest,
    ShouldAcceptEncryptionKeysFromTheWeb) {
  // Mimic the account using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  ASSERT_TRUE(SetupClients());
  SetupSyncTransport();

  // Chrome isn't trying to sync passwords, because the user hasn't opted in to
  // passwords account storage. So the error shouldn't be surfaced yet.
  ASSERT_FALSE(GetAvatarSyncErrorType(GetProfile(0)).has_value());

  password_manager::features_util::OptInToAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));

  // The error is now shown, because PASSWORDS is trying to sync. The data
  // type isn't active yet though due to the missing encryption keys.
  ASSERT_TRUE(
      TrustedVaultKeyRequiredForPreferredDataTypesChecker(GetSyncService(0))
          .Wait());
  ASSERT_THAT(
      GetAvatarSyncErrorType(GetProfile(0)),
      Eq(AvatarSyncErrorType::kTrustedVaultKeyMissingForPasswordsError));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Let's resolve the error. Mimic opening the web page where the user would
  // interact with the retrieval flow. Add an extra tab so the flow tab can be
  // closed via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);
  OpenTabForSyncKeyRetrieval(
      GetBrowser(0), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);

  // Wait until the page closes, which indicates successful completion.
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());

  // PASSWORDS should become active and the error should disappear.
  EXPECT_TRUE(PasswordSyncActiveChecker(GetSyncService(0)).Wait());
  EXPECT_FALSE(GetAvatarSyncErrorType(GetProfile(0)).has_value());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiAndPasswordsAccountStorageTest,
    ShouldReportDegradedTrustedVaultRecoverability) {
  base::HistogramTester histogram_tester;

  // Mimic the key being available upon startup but recoverability degraded.
  const std::vector<uint8_t> trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{trusted_vault_key}),
                        GetFakeServer());
  ASSERT_TRUE(SetupClients());
  GetSyncService(0)->AddTrustedVaultDecryptionKeysFromWeb(
      kGaiaId, GetSecurityDomainsServer()->GetAllTrustedVaultKeys(),
      /*last_key_version=*/GetSecurityDomainsServer()->GetCurrentEpoch());

  SetupSyncTransport();

  // Chrome isn't trying to sync passwords, because the user hasn't opted in to
  // passwords account storage. So the error shouldn't be surfaced yet.
  ASSERT_FALSE(GetAvatarSyncErrorType(GetProfile(0)).has_value());

  password_manager::features_util::OptInToAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));

  ASSERT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/true)
                  .Wait());

  // The error is now shown, because PASSWORDS is trying to sync.
  ASSERT_THAT(GetAvatarSyncErrorType(GetProfile(0)),
              Eq(AvatarSyncErrorType::
                     kTrustedVaultRecoverabilityDegradedForPasswordsError));

  // Let's resolve the error. Mimic opening a web page where the user would
  // interact with the degraded recoverability flow. Add an extra tab so the
  // flow tab can be closed via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);
  OpenTabForSyncKeyRecoverabilityDegraded(
      GetBrowser(0), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());

  // The error should have disappeared.
  EXPECT_FALSE(GetAvatarSyncErrorType(GetProfile(0)).has_value());

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup",
      /*sample=*/true, /*expected_bucket_count=*/1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
