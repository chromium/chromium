// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/sync/test/integration/cookie_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/base/time.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori_test_utils.h"
#include "components/sync/test/fake_server/fake_server_nigori_helper.h"
#include "components/sync/trusted_vault/fake_security_domains_server.h"
#include "components/sync/trusted_vault/standalone_trusted_vault_client.h"
#include "components/sync/trusted_vault/trusted_vault_server_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "crypto/ec_private_key.h"
#include "google_apis/gaia/gaia_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/url_constants.h"

namespace {

using fake_server::GetServerNigori;
using fake_server::SetNigoriInFakeServer;
using syncer::BuildKeystoreNigoriSpecifics;
using syncer::BuildTrustedVaultNigoriSpecifics;
using syncer::KeyParamsForTesting;
using syncer::Pbkdf2KeyParamsForTesting;
using testing::NotNull;
using testing::SizeIs;

const char kGaiaId[] = "gaia_id_for_user_gmail.com";

MATCHER_P(IsDataEncryptedWith, key_params, "") {
  const sync_pb::EncryptedData& encrypted_data = arg;
  std::unique_ptr<syncer::Nigori> nigori = syncer::Nigori::CreateByDerivation(
      key_params.derivation_params, key_params.password);
  std::string nigori_name;
  EXPECT_TRUE(nigori->Permute(syncer::Nigori::Type::Password,
                              syncer::kNigoriKeyName, &nigori_name));
  return encrypted_data.key_name() == nigori_name;
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

GURL GetTrustedVaultRetrievalURL(
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

GURL GetTrustedVaultRecoverabilityURL(
    const net::test_server::EmbeddedTestServer& test_server) {
  return test_server.GetURL(base::StringPrintf(
      "/sync/encryption_keys_recoverability.html?%s", kGaiaId));
}

std::string ComputeKeyName(const KeyParamsForTesting& key_params) {
  std::string key_name;
  syncer::Nigori::CreateByDerivation(key_params.derivation_params,
                                     key_params.password)
      ->Permute(syncer::Nigori::Password, syncer::kNigoriKeyName, &key_name);
  return key_name;
}

// Used to wait until a tab closes.
class TabClosedChecker : public StatusChangeChecker,
                         public content::WebContentsObserver {
 public:
  explicit TabClosedChecker(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    DCHECK(web_contents);
  }

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

  DISALLOW_COPY_AND_ASSIGN(TabClosedChecker);
};

// Used to wait until a page's title changes to a certain value (useful to
// detect Javascript events).
class PageTitleChecker : public StatusChangeChecker,
                         public content::WebContentsObserver {
 public:
  PageTitleChecker(const std::string& expected_title,
                   content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        expected_title_(base::UTF8ToUTF16(expected_title)) {
    DCHECK(web_contents);
  }

  ~PageTitleChecker() override = default;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    const std::u16string actual_title = web_contents()->GetTitle();
    *os << "Waiting for page title \"" << base::UTF16ToUTF8(expected_title_)
        << "\"; actual=\"" << base::UTF16ToUTF8(actual_title) << "\"";
    return actual_title == expected_title_;
  }

  // content::WebContentsObserver overrides.
  void DidStopLoading() override { CheckExitCondition(); }
  void TitleWasSet(content::NavigationEntry* entry) override {
    CheckExitCondition();
  }

 private:
  const std::u16string expected_title_;

  DISALLOW_COPY_AND_ASSIGN(PageTitleChecker);
};

// Used to wait until IsTrustedVaultRecoverabilityDegraded() returns false.
class TrustedVaultRecoverabilityNotDegradedChecker
    : public SingleClientStatusChangeChecker {
 public:
  explicit TrustedVaultRecoverabilityNotDegradedChecker(
      syncer::ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}
  ~TrustedVaultRecoverabilityNotDegradedChecker() override = default;

 protected:
  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    return !service()
                ->GetUserSettings()
                ->IsTrustedVaultRecoverabilityDegraded();
  }
};

class FakeSecurityDomainsServerMemberStatusChecker
    : public StatusChangeChecker,
      public syncer::FakeSecurityDomainsServer::Observer {
 public:
  FakeSecurityDomainsServerMemberStatusChecker(
      int expected_member_count,
      const std::vector<uint8_t>& expected_trusted_vault_key,
      syncer::FakeSecurityDomainsServer* server)
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
  syncer::FakeSecurityDomainsServer* const server_;
};

class SingleClientNigoriSyncTest : public SyncTest {
 public:
  SingleClientNigoriSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientNigoriSyncTest() override = default;

  bool WaitForPasswordForms(
      const std::vector<password_manager::PasswordForm>& forms) const {
    return PasswordFormsChecker(0, forms).Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientNigoriSyncTest);
};

class SingleClientNigoriSyncTestWithNotAwaitQuiescence
    : public SingleClientNigoriSyncTest {
 public:
  SingleClientNigoriSyncTestWithNotAwaitQuiescence() = default;
  ~SingleClientNigoriSyncTestWithNotAwaitQuiescence() override = default;

  bool TestUsesSelfNotifications() override {
    // This test fixture is used with tests, which expect SetupSync() to be
    // waiting for completion, but not for quiescense, because it can't be
    // achieved and isn't needed.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientNigoriSyncTestWithNotAwaitQuiescence);
};

class SingleClientNigoriSyncTestWithFullKeystoreMigration
    : public SingleClientNigoriSyncTest {
 public:
  SingleClientNigoriSyncTestWithFullKeystoreMigration() {
    override_features_.InitAndEnableFeature(
        switches::kSyncTriggerFullKeystoreMigration);
  }

  ~SingleClientNigoriSyncTestWithFullKeystoreMigration() override = default;

 private:
  base::test::ScopedFeatureList override_features_;
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
      IsDataEncryptedWith(Pbkdf2KeyParamsForTesting(keystore_keys.back())));
  EXPECT_EQ(specifics.passphrase_type(),
            sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  EXPECT_TRUE(specifics.keybag_is_frozen());
  EXPECT_TRUE(specifics.has_keystore_migration_time());
}

// Tests that client can decrypt passwords, encrypted with implicit passphrase.
// Test first injects implicit passphrase Nigori and encrypted password form to
// fake server and then checks that client successfully received and decrypted
// this password form.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTest,
                       ShouldDecryptWithImplicitPassphraseNigori) {
  const KeyParamsForTesting kKeyParams = {
      syncer::KeyDerivationParams::CreateForPbkdf2(), "passphrase"};
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

  SetDecryptionPassphraseForClient(/*index=*/0, kKeyParams.password);
  ASSERT_TRUE(SetupSync());
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
      Pbkdf2KeyParamsForTesting(keystore_keys.back());
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
      Pbkdf2KeyParamsForTesting(keystore_keys.back());
  const KeyParamsForTesting kDefaultKeyParams = {
      syncer::KeyDerivationParams::CreateForPbkdf2(), "password"};
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
      Pbkdf2KeyParamsForTesting(keystore_keys[1]);
  const std::string expected_key_bag_key_name =
      ComputeKeyName(new_keystore_key_params);
  EXPECT_TRUE(ServerNigoriKeyNameChecker(expected_key_bag_key_name,
                                         GetSyncService(0), GetFakeServer())
                  .Wait());
}

// Performs initial sync with backward compatible keystore Nigori.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTestWithFullKeystoreMigration,
                       PRE_ShouldCompleteKeystoreMigrationAfterRestart) {
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParamsForTesting kKeystoreKeyParams =
      Pbkdf2KeyParamsForTesting(keystore_keys.back());
  const KeyParamsForTesting kDefaultKeyParams = {
      syncer::KeyDerivationParams::CreateForPbkdf2(), "password"};
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
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTestWithFullKeystoreMigration,
                       ShouldCompleteKeystoreMigrationAfterRestart) {
  ASSERT_TRUE(SetupClients());
  const std::string expected_key_bag_key_name =
      ComputeKeyName(Pbkdf2KeyParamsForTesting(
          /*raw_key=*/GetFakeServer()->GetKeystoreKeys().back()));
  EXPECT_TRUE(ServerNigoriKeyNameChecker(expected_key_bag_key_name,
                                         GetSyncService(0), GetFakeServer())
                  .Wait());
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
      Pbkdf2KeyParamsForTesting(corrupted_keystore_key);
  const KeyParamsForTesting kDefaultKeyParams = {
      syncer::KeyDerivationParams::CreateForPbkdf2(), "password"};
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
  SetupSyncNoWaitingForCompletion();

  EXPECT_TRUE(
      PassphraseRequiredStateChecker(GetSyncService(0), /*desired_state=*/true)
          .Wait());
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      "password"));
  EXPECT_TRUE(WaitForPasswordForms({password_form}));
  // TODO(crbug.com/1042251): verify that client fixes NigoriSpecifics once
  // such behavior is supported.
}

// Performs initial sync for Nigori, but doesn't allow initialized Nigori to be
// committed.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTestWithNotAwaitQuiescence,
                       PRE_ShouldCompleteKeystoreInitializationAfterRestart) {
  GetFakeServer()->TriggerCommitError(sync_pb::SyncEnums::THROTTLED);
  ASSERT_TRUE(SetupSync());

  sync_pb::NigoriSpecifics specifics;
  ASSERT_TRUE(GetServerNigori(GetFakeServer(), &specifics));
  ASSERT_EQ(specifics.passphrase_type(),
            sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE);
}

// After browser restart the client should commit initialized Nigori.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTestWithNotAwaitQuiescence,
                       ShouldCompleteKeystoreInitializationAfterRestart) {
  sync_pb::NigoriSpecifics specifics;
  ASSERT_TRUE(GetServerNigori(GetFakeServer(), &specifics));
  ASSERT_EQ(specifics.passphrase_type(),
            sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE);

  ASSERT_TRUE(SetupClients());
  EXPECT_TRUE(ServerNigoriChecker(GetSyncService(0), GetFakeServer(),
                                  syncer::PassphraseType::kKeystorePassphrase)
                  .Wait());
}

class SingleClientNigoriWithWebApiTest : public SyncTest {
 public:
  SingleClientNigoriWithWebApiTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientNigoriWithWebApiTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    const GURL& base_url = embedded_test_server()->base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    SyncTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientNigoriWithWebApiTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldAcceptEncryptionKeysFromTheWebWhileSignedIn) {
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};

  const GURL retrieval_url =
      GetTrustedVaultRetrievalURL(*embedded_test_server(), kTestEncryptionKey);

  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Verify the profile-menu error string.
  ASSERT_EQ(sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_PASSWORDS_ERROR,
            sync_ui_util::GetAvatarSyncErrorType(GetProfile(0)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // Verify the string that would be displayed in settings.
  ASSERT_THAT(sync_ui_util::GetStatusLabels(GetProfile(0)),
              StatusLabelsMatch(sync_ui_util::PASSWORDS_ONLY_SYNC_ERROR,
                                IDS_SETTINGS_EMPTY_STRING,
                                IDS_SYNC_STATUS_NEEDS_KEYS_BUTTON,
                                sync_ui_util::RETRIEVE_TRUSTED_VAULT_KEYS));

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  // Mimic opening a web page where the user can interact with the retrieval
  // flow.
  sync_ui_util::OpenTabForSyncKeyRetrievalWithURLForTesting(GetBrowser(0),
                                                            retrieval_url);
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
  EXPECT_FALSE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));
  EXPECT_THAT(
      sync_ui_util::GetStatusLabels(GetProfile(0)),
      StatusLabelsMatch(sync_ui_util::SYNCED, IDS_SYNC_ACCOUNT_SYNCING,
                        IDS_SETTINGS_EMPTY_STRING, sync_ui_util::NO_ACTION));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Verify the profile-menu error string is empty.
  EXPECT_EQ(sync_ui_util::NO_SYNC_ERROR,
            sync_ui_util::GetAvatarSyncErrorType(GetProfile(0)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       PRE_ShouldAcceptEncryptionKeysFromTheWebBeforeSignIn) {
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};
  const GURL retrieval_url =
      GetTrustedVaultRetrievalURL(*embedded_test_server(), kTestEncryptionKey);

  ASSERT_TRUE(SetupClients());

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  // Mimic opening a web page where the user can interact with the retrieval
  // flow, while the user is signed out.
  sync_ui_util::OpenTabForSyncKeyRetrievalWithURLForTesting(GetBrowser(0),
                                                            retrieval_url);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes, which indicates successful completion.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldAcceptEncryptionKeysFromTheWebBeforeSignIn) {
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};

  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  // Sign in and start sync.
  EXPECT_TRUE(SetupSync());

  ASSERT_EQ(syncer::PassphraseType::kTrustedVaultPassphrase,
            GetSyncService(0)->GetUserSettings()->GetPassphraseType());
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultRecoverabilityDegraded());
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  EXPECT_FALSE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));
  EXPECT_FALSE(sync_ui_util::ShouldShowTrustedVaultDegradedRecoverabilityError(
      GetSyncService(0)));
  EXPECT_THAT(
      sync_ui_util::GetStatusLabels(GetProfile(0)),
      StatusLabelsMatch(sync_ui_util::SYNCED, IDS_SYNC_ACCOUNT_SYNCING,
                        IDS_SETTINGS_EMPTY_STRING, sync_ui_util::NO_ACTION));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Verify the profile-menu error string is empty.
  EXPECT_EQ(sync_ui_util::NO_SYNC_ERROR,
            sync_ui_util::GetAvatarSyncErrorType(GetProfile(0)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    PRE_ShouldClearEncryptionKeysFromTheWebWhenSigninCookiesCleared) {
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};
  const GURL retrieval_url =
      GetTrustedVaultRetrievalURL(*embedded_test_server(), kTestEncryptionKey);

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
  sync_ui_util::OpenTabForSyncKeyRetrievalWithURLForTesting(GetBrowser(0),
                                                            retrieval_url);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes, which indicates successful completion.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());
  EXPECT_TRUE(keys_fetched_checker.Wait());

  // Mimic signin cookie clearing.
  TrustedVaultKeysChangedStateChecker keys_cleared_checker(GetSyncService(0));
  cookie_helper::DeleteSigninCookies(GetProfile(0));
  EXPECT_TRUE(keys_cleared_checker.Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    ShouldClearEncryptionKeysFromTheWebWhenSigninCookiesCleared) {
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};

  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  // Sign in and start sync.
  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  EXPECT_TRUE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    ShouldRemotelyTransitFromTrustedVaultToKeystorePassphrase) {
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};

  const GURL retrieval_url =
      GetTrustedVaultRetrievalURL(*embedded_test_server(), kTestEncryptionKey);

  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  // Mimic opening a web page where the user can interact with the retrieval
  // flow.
  sync_ui_util::OpenTabForSyncKeyRetrievalWithURLForTesting(GetBrowser(0),
                                                            retrieval_url);
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
      Pbkdf2KeyParamsForTesting(keystore_keys.back());
  const KeyParamsForTesting kTrustedVaultKeyParams =
      Pbkdf2KeyParamsForTesting(kTestEncryptionKey);
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
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};

  const GURL retrieval_url =
      GetTrustedVaultRetrievalURL(*embedded_test_server(), kTestEncryptionKey);

  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  // Mimic opening a web page where the user can interact with the retrieval
  // flow.
  sync_ui_util::OpenTabForSyncKeyRetrievalWithURLForTesting(GetBrowser(0),
                                                            retrieval_url);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes, which indicates successful completion.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());

  // Mimic remote transition to custom passphrase.
  const KeyParamsForTesting kCustomPassphraseKeyParams = {
      syncer::KeyDerivationParams::CreateForPbkdf2(), "passphrase"};
  const KeyParamsForTesting kTrustedVaultKeyParams =
      Pbkdf2KeyParamsForTesting(kTestEncryptionKey);
  SetNigoriInFakeServer(CreateCustomPassphraseNigori(kCustomPassphraseKeyParams,
                                                     kTrustedVaultKeyParams),
                        GetFakeServer());

  EXPECT_TRUE(
      PassphraseRequiredStateChecker(GetSyncService(0), /*desired_state=*/true)
          .Wait());
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kCustomPassphraseKeyParams.password));
  EXPECT_TRUE(
      PassphraseRequiredStateChecker(GetSyncService(0), /*desired_state=*/false)
          .Wait());

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

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    ShoudRecordTrustedVaultErrorShownOnStartupWhenErrorShown) {
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};

  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));

  histogram_tester.ExpectUniqueSample("Sync.TrustedVaultErrorShownOnStartup",
                                      /*sample=*/1, /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientNigoriWithWebApiTest,
    PRE_ShoudRecordTrustedVaultErrorShownOnStartupWhenErrorNotShown) {
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};
  const GURL retrieval_url =
      GetTrustedVaultRetrievalURL(*embedded_test_server(), kTestEncryptionKey);

  ASSERT_TRUE(SetupClients());

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  TrustedVaultKeysChangedStateChecker keys_fetched_checker(GetSyncService(0));
  // Mimic opening a web page where the user can interact with the retrieval
  // flow, while the user is signed out.
  sync_ui_util::OpenTabForSyncKeyRetrievalWithURLForTesting(GetBrowser(0),
                                                            retrieval_url);
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
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};

  const GURL retrieval_url =
      GetTrustedVaultRetrievalURL(*embedded_test_server(), kTestEncryptionKey);

  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_FALSE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));

  histogram_tester.ExpectUniqueSample("Sync.TrustedVaultErrorShownOnStartup",
                                      /*sample=*/0, /*expected_count=*/1);
}

// Same as SingleClientNigoriWithWebApiTest but does NOT override
// switches::kGaiaUrl, which means the embedded test server gets treated as
// untrusted origin.
class SingleClientNigoriWithWebApiFromUntrustedOriginTest
    : public SingleClientNigoriWithWebApiTest {
 public:
  SingleClientNigoriWithWebApiFromUntrustedOriginTest() = default;
  ~SingleClientNigoriWithWebApiFromUntrustedOriginTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    SyncTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiFromUntrustedOriginTest,
                       ShouldNotExposeJavascriptApi) {
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};

  const GURL retrieval_url =
      GetTrustedVaultRetrievalURL(*embedded_test_server(), kTestEncryptionKey);

  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  SetupSyncNoWaitingForCompletion();
  ASSERT_TRUE(TrustedVaultKeyRequiredStateChecker(GetSyncService(0),
                                                  /*desired_state=*/true)
                  .Wait());

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  // Mimic opening a web page where the user can interact with the retrieval
  // flow.
  sync_ui_util::OpenTabForSyncKeyRetrievalWithURLForTesting(GetBrowser(0),
                                                            retrieval_url);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the title reflects the function is undefined.
  PageTitleChecker title_checker(
      /*expected_title=*/"UNDEFINED",
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(title_checker.Wait());

  EXPECT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
}

class SingleClientNigoriWithRecoverySyncTest
    : public SingleClientNigoriWithWebApiTest {
 public:
  SingleClientNigoriWithRecoverySyncTest() {
    override_features_.InitAndEnableFeature(
        switches::kSyncSupportTrustedVaultPassphraseRecovery);
  }

  ~SingleClientNigoriWithRecoverySyncTest() override = default;

 private:
  base::test::ScopedFeatureList override_features_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientNigoriWithRecoverySyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithRecoverySyncTest,
                       ShouldReportDegradedTrustedVaultRecoverability) {
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};

  const GURL recoverability_url =
      GetTrustedVaultRecoverabilityURL(*embedded_test_server());

  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  // Mimic the key being available upon startup but recoverability degraded.
  ASSERT_TRUE(SetupClients());
  static_cast<syncer::StandaloneTrustedVaultClient*>(
      GetSyncService(0)->GetSyncClientForTest()->GetTrustedVaultClient())
      ->SetRecoverabilityDegradedForTesting();
  GetSyncService(0)->AddTrustedVaultDecryptionKeysFromWeb(
      kGaiaId, {kTestEncryptionKey}, /*last_key_version=*/1);
  ASSERT_TRUE(SetupSync());

  ASSERT_EQ(syncer::PassphraseType::kTrustedVaultPassphrase,
            GetSyncService(0)->GetUserSettings()->GetPassphraseType());
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));

  EXPECT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultRecoverabilityDegraded());
  EXPECT_TRUE(sync_ui_util::ShouldShowTrustedVaultDegradedRecoverabilityError(
      GetSyncService(0)));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Verify the profile-menu error string is empty.
  EXPECT_EQ(
      sync_ui_util::TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_ERROR,
      sync_ui_util::GetAvatarSyncErrorType(GetProfile(0)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // No messages expected in settings.
  EXPECT_THAT(
      sync_ui_util::GetStatusLabels(GetProfile(0)),
      StatusLabelsMatch(sync_ui_util::SYNCED, IDS_SYNC_ACCOUNT_SYNCING,
                        IDS_SETTINGS_EMPTY_STRING, sync_ui_util::NO_ACTION));

  // Mimic opening a web page where the user can interact with the degraded
  // recoverability flow. Before that, there needs to be an existing tab for the
  // second tab to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);
  // TODO(crbug.com/1081649): This should use a dedicated page, instead of the
  // retrieval page.
  sync_ui_util::OpenTabForSyncKeyRetrievalWithURLForTesting(GetBrowser(0),
                                                            recoverability_url);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  EXPECT_TRUE(
      TrustedVaultRecoverabilityNotDegradedChecker(GetSyncService(0)).Wait());
  EXPECT_FALSE(sync_ui_util::ShouldShowTrustedVaultDegradedRecoverabilityError(
      GetSyncService(0)));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Verify the profile-menu error string is empty.
  EXPECT_EQ(sync_ui_util::NO_SYNC_ERROR,
            sync_ui_util::GetAvatarSyncErrorType(GetProfile(0)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

class SingleClientNigoriSyncTestWithSecurityDomainsServer : public SyncTest {
 public:
  SingleClientNigoriSyncTestWithSecurityDomainsServer()
      : SyncTest(SINGLE_CLIENT) {
    override_features_.InitAndEnableFeature(
        switches::kFollowTrustedVaultKeyRotation);
  }
  SingleClientNigoriSyncTestWithSecurityDomainsServer(
      const SingleClientNigoriSyncTestWithSecurityDomainsServer& other) =
      delete;
  SingleClientNigoriSyncTestWithSecurityDomainsServer& operator=(
      const SingleClientNigoriSyncTestWithSecurityDomainsServer& other) =
      delete;
  ~SingleClientNigoriSyncTestWithSecurityDomainsServer() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    security_domains_server_ =
        std::make_unique<syncer::FakeSecurityDomainsServer>(
            embedded_test_server()->base_url());
    command_line->AppendSwitchASCII(
        switches::kTrustedVaultServiceURL,
        security_domains_server_->server_url().spec());
    SyncTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&syncer::FakeSecurityDomainsServer::HandleRequest,
                            base::Unretained(security_domains_server_.get())));
    embedded_test_server()->StartAcceptingConnections();
    SyncTest::SetUpOnMainThread();
  }

  void TearDown() override {
    // Test server shutdown is required before |security_domains_server_| can be
    // destroyed.
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    SyncTest::TearDown();
  }

  syncer::FakeSecurityDomainsServer* GetSecurityDomainsServer() {
    return security_domains_server_.get();
  }

 private:
  std::unique_ptr<syncer::FakeSecurityDomainsServer> security_domains_server_;
  base::test::ScopedFeatureList override_features_;
};

// Device registration attempt should be taken upon sign in into primary
// profile. It should be successful when security domain server allows device
// registration with constant key.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTestWithSecurityDomainsServer,
                       ShouldRegisterDeviceWithConstantKey) {
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/1113599): consider checking member public key (requires
  // either ability to overload key generator in the test or exposing public key
  // from the client).
  EXPECT_TRUE(
      FakeSecurityDomainsServerMemberStatusChecker(
          /*expected_member_count=*/1,
          /*expected_trusted_vault_key=*/syncer::GetConstantTrustedVaultKey(),
          GetSecurityDomainsServer())
          .Wait());
  EXPECT_FALSE(GetSecurityDomainsServer()->received_invalid_request());
}

// If device was successfully registered with constant key, it should silently
// follow key rotation and transit to trusted vault passphrase without going
// through key retrieval flow.
// TODO(crbug.com/1193177): fix threading issues with FakeSecurityDomainsServer
// and re-enable the test.
IN_PROC_BROWSER_TEST_F(SingleClientNigoriSyncTestWithSecurityDomainsServer,
                       DISABLED_ShouldFollowInitialKeyRotation) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      FakeSecurityDomainsServerMemberStatusChecker(
          /*expected_member_count=*/1,
          /*expected_trusted_vault_key=*/syncer::GetConstantTrustedVaultKey(),
          GetSecurityDomainsServer())
          .Wait());

  // Rotate trusted vault key and mimic transition to trusted vault passphrase
  // type.
  std::vector<uint8_t> new_trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/syncer::GetConstantTrustedVaultKey());
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{new_trusted_vault_key}),
                        GetFakeServer());

  // Inject password encrypted with trusted vault key and verify client is able
  // to decrypt it.
  const KeyParamsForTesting trusted_vault_key_params =
      Pbkdf2KeyParamsForTesting(new_trusted_vault_key);
  const password_manager::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(0);
  passwords_helper::InjectEncryptedServerPassword(
      password_form, trusted_vault_key_params.password,
      trusted_vault_key_params.derivation_params, GetFakeServer());
  EXPECT_TRUE(PasswordFormsChecker(0, {password_form}).Wait());
}

}  // namespace
