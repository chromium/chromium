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
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/base/time.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori.h"
#include "crypto/ec_private_key.h"
#include "google_apis/gaia/gaia_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using encryption_helper::GetServerNigori;
using encryption_helper::SetNigoriInFakeServer;
using testing::NotNull;
using testing::SizeIs;

struct KeyParams {
  syncer::KeyDerivationParams derivation_params;
  std::string password;
};

MATCHER_P(IsDataEncryptedWith, key_params, "") {
  const sync_pb::EncryptedData& encrypted_data = arg;
  std::unique_ptr<syncer::Nigori> nigori = syncer::Nigori::CreateByDerivation(
      key_params.derivation_params, key_params.password);
  std::string nigori_name;
  EXPECT_TRUE(nigori->Permute(syncer::Nigori::Type::Password,
                              syncer::kNigoriKeyName, &nigori_name));
  return encrypted_data.key_name() == nigori_name;
}

GURL GetTrustedVaultRetrievalURL(
    const net::test_server::EmbeddedTestServer& test_server,
    const std::string& encryption_key) {
  const char kGaiaId[] = "gaia_id_for_user_gmail.com";
  return test_server.GetURL(
      base::StringPrintf("/sync/encryption_keys_retrieval.html?%s#%s", kGaiaId,
                         encryption_key.c_str()));
}

KeyParams KeystoreKeyParams(const std::string& key) {
  // Due to mis-encode of keystore keys to base64 we have to always encode such
  // keys to provide backward compatibility.
  std::string encoded_key;
  base::Base64Encode(key, &encoded_key);
  return {syncer::KeyDerivationParams::CreateForPbkdf2(),
          std::move(encoded_key)};
}

std::string ComputeKeyName(const KeyParams& key_params) {
  std::string key_name;
  syncer::Nigori::CreateByDerivation(key_params.derivation_params,
                                     key_params.password)
      ->Permute(syncer::Nigori::Password, syncer::kNigoriKeyName, &key_name);
  return key_name;
}

// Builds NigoriSpecifics with following fields:
// 1. encryption_keybag contains all keys derived from |keybag_keys_params|
// and encrypted with a key derived from |keybag_decryptor_params|.
// keystore_decryptor_token is always saved in encryption_keybag, even if it
// is not derived from any params in |keybag_keys_params|.
// 2. keystore_decryptor_token contains the key derived from
// |keybag_decryptor_params| and encrypted with a key derived from
// |keystore_key_params|.
// 3. passphrase_type is KEYSTORE_PASSHPRASE.
// 4. Other fields are default.
sync_pb::NigoriSpecifics BuildKeystoreNigoriSpecifics(
    const std::vector<KeyParams>& keybag_keys_params,
    const KeyParams& keystore_decryptor_params,
    const KeyParams& keystore_key_params) {
  sync_pb::NigoriSpecifics specifics;

  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          keystore_decryptor_params.password,
          keystore_decryptor_params.derivation_params);
  for (const KeyParams& key_params : keybag_keys_params) {
    cryptographer->EmplaceKey(key_params.password,
                              key_params.derivation_params);
  }

  EXPECT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));

  std::string serialized_keystore_decryptor =
      cryptographer->ExportDefaultKey().SerializeAsString();

  std::unique_ptr<syncer::CryptographerImpl> keystore_cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          keystore_key_params.password, keystore_key_params.derivation_params);
  EXPECT_TRUE(keystore_cryptographer->EncryptString(
      serialized_keystore_decryptor,
      specifics.mutable_keystore_decryptor_token()));

  specifics.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  specifics.set_keystore_migration_time(
      syncer::TimeToProtoTime(base::Time::Now()));
  return specifics;
}

sync_pb::NigoriSpecifics BuildTrustedVaultNigoriSpecifics(
    const std::vector<std::string>& trusted_vault_keys) {
  sync_pb::NigoriSpecifics specifics;
  specifics.set_passphrase_type(
      sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE);
  specifics.set_keybag_is_frozen(true);

  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::CreateEmpty();
  for (const std::string& trusted_vault_key : trusted_vault_keys) {
    const std::string key_name = cryptographer->EmplaceKey(
        trusted_vault_key, syncer::KeyDerivationParams::CreateForPbkdf2());
    cryptographer->SelectDefaultEncryptionKey(key_name);
  }

  EXPECT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));
  return specifics;
}

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

  ~PageTitleChecker() override {}

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    const base::string16 actual_title = web_contents()->GetTitle();
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
  const base::string16 expected_title_;

  DISALLOW_COPY_AND_ASSIGN(PageTitleChecker);
};

class PasswordsDataTypeActiveChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PasswordsDataTypeActiveChecker(syncer::ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}
  ~PasswordsDataTypeActiveChecker() override {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for PASSWORDS to become active";
    return service()->GetActiveDataTypes().Has(syncer::PASSWORDS);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordsDataTypeActiveChecker);
};

class SingleClientNigoriSyncTestWithUssTests
    : public SyncTest,
      public testing::WithParamInterface<bool> {
 public:
  SingleClientNigoriSyncTestWithUssTests() : SyncTest(SINGLE_CLIENT) {
    if (GetParam()) {
      // USS Nigori requires USS implementations to be enabled for all
      // datatypes.
      override_features_.InitWithFeatures(
          /*enabled_features=*/{switches::kSyncUSSPasswords,
                                switches::kSyncUSSNigori},
          /*disabled_features=*/{});
    } else {
      // We test Directory Nigori with default values of USS feature flags of
      // other datatypes.
      override_features_.InitAndDisableFeature(switches::kSyncUSSNigori);
    }
  }

  ~SingleClientNigoriSyncTestWithUssTests() override = default;

  bool WaitForPasswordForms(
      const std::vector<autofill::PasswordForm>& forms) const {
    return PasswordFormsChecker(0, forms).Wait();
  }

 private:
  base::test::ScopedFeatureList override_features_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientNigoriSyncTestWithUssTests);
};

class SingleClientNigoriSyncTestWithNotAwaitQuiescence
    : public SingleClientNigoriSyncTestWithUssTests {
 public:
  SingleClientNigoriSyncTestWithNotAwaitQuiescence() = default;
  ~SingleClientNigoriSyncTestWithNotAwaitQuiescence() = default;

  bool TestUsesSelfNotifications() override {
    // This test fixture is used with tests, which expect SetupSync() to be
    // waiting for completion, but not for quiescense, because it can't be
    // achieved and isn't needed.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientNigoriSyncTestWithNotAwaitQuiescence);
};

IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithUssTests,
                       ShouldCommitKeystoreNigoriWhenReceivedDefault) {
  // SetupSync() should make FakeServer send default NigoriSpecifics.
  ASSERT_TRUE(SetupSync());
  // TODO(crbug/922900): we may want to actually wait for specifics update in
  // fake server. Due to implementation details it's not currently needed.
  sync_pb::NigoriSpecifics specifics;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &specifics));

  const std::vector<std::string>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_TRUE(keystore_keys.size() == 1);
  EXPECT_THAT(specifics.encryption_keybag(),
              IsDataEncryptedWith(KeystoreKeyParams(keystore_keys.back())));
  EXPECT_EQ(specifics.passphrase_type(),
            sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  EXPECT_TRUE(specifics.keybag_is_frozen());
  EXPECT_TRUE(specifics.has_keystore_migration_time());
}

// Tests that client can decrypt passwords, encrypted with implicit passphrase.
// Test first injects implicit passphrase Nigori and encrypted password form to
// fake server and then checks that client successfully received and decrypted
// this password form.
IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithUssTests,
                       ShouldDecryptWithImplicitPassphraseNigori) {
  const KeyParams kKeyParams = {syncer::KeyDerivationParams::CreateForPbkdf2(),
                                "passphrase"};
  sync_pb::NigoriSpecifics specifics;
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          kKeyParams.password, kKeyParams.derivation_params);
  ASSERT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));
  SetNigoriInFakeServer(GetFakeServer(), specifics);

  const autofill::PasswordForm password_form =
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
IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithUssTests,
                       ShouldDecryptWithKeystoreNigori) {
  const std::vector<std::string>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(keystore_keys.back());
  SetNigoriInFakeServer(GetFakeServer(),
                        BuildKeystoreNigoriSpecifics(
                            /*keybag_keys_params=*/{kKeystoreKeyParams},
                            /*keystore_decryptor_params=*/kKeystoreKeyParams,
                            /*keystore_key_params=*/kKeystoreKeyParams));

  const autofill::PasswordForm password_form =
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
IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithUssTests,
                       ShouldDecryptWithBackwardCompatibleKeystoreNigori) {
  const std::vector<std::string>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(keystore_keys.back());
  const KeyParams kDefaultKeyParams = {
      syncer::KeyDerivationParams::CreateForPbkdf2(), "password"};
  SetNigoriInFakeServer(
      GetFakeServer(),
      BuildKeystoreNigoriSpecifics(
          /*keybag_keys_params=*/{kDefaultKeyParams, kKeystoreKeyParams},
          /*keystore_decryptor_params*/ {kDefaultKeyParams},
          /*keystore_key_params=*/kKeystoreKeyParams));
  const autofill::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(0);
  passwords_helper::InjectEncryptedServerPassword(
      password_form, kDefaultKeyParams.password,
      kDefaultKeyParams.derivation_params, GetFakeServer());
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForPasswordForms({password_form}));
}

IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithUssTests,
                       ShouldRotateKeystoreKey) {
  ASSERT_TRUE(SetupSync());

  GetFakeServer()->TriggerKeystoreKeyRotation();
  const std::vector<std::string>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(2));
  const KeyParams new_keystore_key_params = KeystoreKeyParams(keystore_keys[1]);
  const std::string expected_key_bag_key_name =
      ComputeKeyName(new_keystore_key_params);
  EXPECT_TRUE(ServerNigoriKeyNameChecker(expected_key_bag_key_name,
                                         GetSyncService(0), GetFakeServer())
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithUssTests,
                       ShouldExposeExperimentalAuthenticationKey) {
  const std::vector<std::string>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(keystore_keys.back());
  SetNigoriInFakeServer(GetFakeServer(),
                        BuildKeystoreNigoriSpecifics(
                            /*keybag_keys_params=*/{kKeystoreKeyParams},
                            /*keystore_decryptor_params=*/kKeystoreKeyParams,
                            /*keystore_key_params=*/kKeystoreKeyParams));

  ASSERT_TRUE(SetupSync());

  // WARNING: Do *NOT* change these values since the authentication key should
  // be stable across different browser versions.

  // Default birthday determined by LoopbackServer.
  const std::string kDefaultBirthday = "0";
  const std::string kSeparator("|");
  std::string base64_encoded_keystore_key;
  base::Base64Encode(keystore_keys.back(), &base64_encoded_keystore_key);
  const std::string authentication_id_before_hashing =
      std::string("gaia_id_for_user_gmail.com") + kSeparator +
      kDefaultBirthday + kSeparator + base64_encoded_keystore_key;

  EXPECT_EQ(
      GetSyncService(/*index=*/0)->GetExperimentalAuthenticationSecretForTest(),
      authentication_id_before_hashing);
  EXPECT_TRUE(GetSyncService(/*index=*/0)->GetExperimentalAuthenticationKey());
}

INSTANTIATE_TEST_SUITE_P(USS,
                         SingleClientNigoriSyncTestWithUssTests,
                         ::testing::Values(false, true));

// Performs initial sync for Nigori, but doesn't allow initialized Nigori to be
// commited.
IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithNotAwaitQuiescence,
                       PRE_ShouldCompleteKeystoreInitializationAfterRestart) {
  GetFakeServer()->TriggerCommitError(sync_pb::SyncEnums::THROTTLED);
  ASSERT_TRUE(SetupSync());

  sync_pb::NigoriSpecifics specifics;
  ASSERT_TRUE(GetServerNigori(GetFakeServer(), &specifics));
  ASSERT_EQ(specifics.passphrase_type(),
            sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE);
}

// After browser restart the client should commit initialized Nigori.
IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithNotAwaitQuiescence,
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

INSTANTIATE_TEST_SUITE_P(USS,
                         SingleClientNigoriSyncTestWithNotAwaitQuiescence,
                         ::testing::Values(false, true));

class SingleClientNigoriWithWebApiTest : public SyncTest {
 public:
  SingleClientNigoriWithWebApiTest() : SyncTest(SINGLE_CLIENT) {
    // USS Nigori requires USS implementations to be enabled for all
    // datatypes.
    override_features_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncUSSPasswords,
                              switches::kSyncUSSNigori,
                              switches::kSyncSupportTrustedVaultPassphrase,
                              features::kSyncEncryptionKeysWebApi},
        /*disabled_features=*/{});
  }
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
  base::test::ScopedFeatureList override_features_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientNigoriWithWebApiTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldAcceptEncryptionKeysFromTheWebWhileSignedIn) {
  const std::string kTestEncryptionKey = "testpassphrase1";

  const GURL retrieval_url =
      GetTrustedVaultRetrievalURL(*embedded_test_server(), kTestEncryptionKey);

  // Mimic the account being already using a trusted vault passphrase.
  encryption_helper::SetNigoriInFakeServer(
      GetFakeServer(), BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));

  // Mimic opening a web page where the user can interact with the retrieval
  // flow.
  sync_ui_util::OpenTabForSyncKeyRetrievalWithURLForTesting(GetBrowser(0),
                                                            retrieval_url);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the title changes to "OK" via Javascript, which indicates
  // completion.
  PageTitleChecker title_checker(
      /*expected_title=*/"OK",
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(title_checker.Wait());

  EXPECT_TRUE(PasswordsDataTypeActiveChecker(GetSyncService(0)).Wait());
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  EXPECT_FALSE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));
}

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       PRE_ShouldAcceptEncryptionKeysFromTheWebBeforeSignIn) {
  const std::string kTestEncryptionKey = "testpassphrase1";
  const GURL retrieval_url =
      GetTrustedVaultRetrievalURL(*embedded_test_server(), kTestEncryptionKey);

  ASSERT_TRUE(SetupClients());

  // Mimic opening a web page where the user can interact with the retrieval
  // flow, while the user is signed out.
  sync_ui_util::OpenTabForSyncKeyRetrievalWithURLForTesting(GetBrowser(0),
                                                            retrieval_url);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the title changes to "OK" via Javascript, which indicates
  // completion.
  PageTitleChecker title_checker(
      /*expected_title=*/"OK",
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(title_checker.Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientNigoriWithWebApiTest,
                       ShouldAcceptEncryptionKeysFromTheWebBeforeSignIn) {
  const std::string kTestEncryptionKey = "testpassphrase1";

  // Mimic the account being already using a trusted vault passphrase.
  encryption_helper::SetNigoriInFakeServer(
      GetFakeServer(), BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}));

  // Sign in and start sync.
  EXPECT_TRUE(SetupSync());

  ASSERT_EQ(syncer::PassphraseType::kTrustedVaultPassphrase,
            GetSyncService(0)->GetUserSettings()->GetPassphraseType());
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  EXPECT_FALSE(sync_ui_util::ShouldShowSyncKeysMissingError(GetSyncService(0)));
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
  const std::string kTestEncryptionKey = "testpassphrase1";

  const GURL retrieval_url =
      GetTrustedVaultRetrievalURL(*embedded_test_server(), kTestEncryptionKey);

  // Mimic the account being already using a trusted vault passphrase.
  encryption_helper::SetNigoriInFakeServer(
      GetFakeServer(), BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}));

  SetupSyncNoWaitingForCompletion();
  ASSERT_TRUE(TrustedVaultKeyRequiredStateChecker(GetSyncService(0),
                                                  /*desired_state=*/true)
                  .Wait());

  // Mimic opening a web page where the user can interact with the retrival
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

}  // namespace
