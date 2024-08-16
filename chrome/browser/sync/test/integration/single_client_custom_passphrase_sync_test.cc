// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_engine_stopped_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/nigori_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using bookmarks_helper::AddURL;
using bookmarks_helper::BookmarksTitleChecker;
using bookmarks_helper::CreateBookmarkServerEntity;
using bookmarks_helper::ServerBookmarksEqualityChecker;
using fake_server::FakeServer;
using fake_server::GetServerNigori;
using fake_server::SetNigoriInFakeServer;
using sync_pb::NigoriSpecifics;
using syncer::BuildCustomPassphraseNigoriSpecifics;
using syncer::Cryptographer;
using syncer::DataTypeSet;
using syncer::GetEncryptedBookmarkEntitySpecifics;
using syncer::InitCustomPassphraseCryptographerFromNigori;
using syncer::KeyParamsForTesting;
using syncer::LoopbackServerEntity;
using syncer::PassphraseType;
using syncer::Pbkdf2PassphraseKeyParamsForTesting;
using syncer::ProtoPassphraseInt32ToEnum;
using syncer::ScryptPassphraseKeyParamsForTesting;
using syncer::SyncEngineStoppedChecker;
using testing::ElementsAre;

// Intercepts all bookmark entity names as committed to the FakeServer.
class CommittedBookmarkEntityNameObserver : public FakeServer::Observer {
 public:
  explicit CommittedBookmarkEntityNameObserver(FakeServer* fake_server)
      : fake_server_(fake_server) {
    fake_server->AddObserver(this);
  }

  ~CommittedBookmarkEntityNameObserver() override {
    fake_server_->RemoveObserver(this);
  }

  void OnCommit(DataTypeSet committed_data_types) override {
    sync_pb::ClientToServerMessage message;
    fake_server_->GetLastCommitMessage(&message);
    for (const sync_pb::SyncEntity& entity : message.commit().entries()) {
      if (syncer::GetDataTypeFromSpecifics(entity.specifics()) ==
          syncer::BOOKMARKS) {
        committed_names_.insert(entity.name());
      }
    }
  }

  const std::set<std::string> GetCommittedEntityNames() const {
    return committed_names_;
  }

 private:
  const raw_ptr<FakeServer> fake_server_;
  std::set<std::string> committed_names_;
};

// These tests use a gray-box testing approach to verify that the data committed
// to the server is encrypted properly, and that properly-encrypted data from
// the server is successfully decrypted by the client. They also verify that the
// key derivation methods are set, read and handled properly. They do not,
// however, directly ensure that two clients syncing through the same account
// will be able to access each others' data in the presence of a custom
// passphrase. For this, a separate two-client test is be used.
class SingleClientCustomPassphraseSyncTest : public SyncTest {
 public:
  SingleClientCustomPassphraseSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientCustomPassphraseSyncTest(
      const SingleClientCustomPassphraseSyncTest&) = delete;
  SingleClientCustomPassphraseSyncTest& operator=(
      const SingleClientCustomPassphraseSyncTest&) = delete;

  ~SingleClientCustomPassphraseSyncTest() override = default;

  // Waits until the given set of bookmarks appears on the server, encrypted
  // according to the server-side Nigori and with the given passphrase.
  bool WaitForEncryptedServerBookmarks(
      const std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark>&
          expected_bookmarks,
      const std::string& passphrase) {
    std::unique_ptr<Cryptographer> cryptographer =
        CreateCryptographerFromServerNigori(passphrase);
    return ServerBookmarksEqualityChecker(expected_bookmarks,
                                          cryptographer.get())
        .Wait();
  }

  bool WaitForUnencryptedServerBookmarks(
      const std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark>&
          expected_bookmarks) {
    return ServerBookmarksEqualityChecker(expected_bookmarks,
                                          /*cryptographer=*/nullptr)
        .Wait();
  }

  bool WaitForNigori(PassphraseType expected_passphrase_type) {
    return ServerPassphraseTypeChecker(expected_passphrase_type).Wait();
  }

  bool WaitForPassphraseRequired() {
    return PassphraseRequiredChecker(GetSyncService()).Wait();
  }

  bool WaitForPassphraseAccepted() {
    return PassphraseAcceptedChecker(GetSyncService()).Wait();
  }

  bool WaitForClientBookmarkWithTitle(std::string title) {
    return BookmarksTitleChecker(/*profile_index=*/0, title,
                                 /*expected_count=*/1)
        .Wait();
  }

  syncer::SyncServiceImpl* GetSyncService() {
    return SyncTest::GetSyncService(0);
  }

  // When the cryptographer is initialized with a passphrase, it uses the key
  // derivation method and other parameters from the server-side Nigori. Thus,
  // checking that the server-side Nigori contains the desired key derivation
  // method and checking that the server-side encrypted bookmarks can be
  // decrypted using a cryptographer initialized with this function is
  // sufficient to determine that a given key derivation method is being
  // correctly used for encryption.
  std::unique_ptr<Cryptographer> CreateCryptographerFromServerNigori(
      const std::string& passphrase) {
    NigoriSpecifics nigori;
    EXPECT_TRUE(GetServerNigori(GetFakeServer(), &nigori));
    EXPECT_EQ(ProtoPassphraseInt32ToEnum(nigori.passphrase_type()),
              PassphraseType::kCustomPassphrase);
    return InitCustomPassphraseCryptographerFromNigori(nigori, passphrase);
  }

  void InjectEncryptedServerBookmark(const std::string& title,
                                     const GURL& url,
                                     const KeyParamsForTesting& key_params) {
    std::unique_ptr<LoopbackServerEntity> server_entity =
        CreateBookmarkServerEntity(title, url);
    server_entity->SetSpecifics(GetEncryptedBookmarkEntitySpecifics(
        server_entity->GetSpecifics().bookmark(), key_params));
    GetFakeServer()->InjectEntity(std::move(server_entity));
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       ShouldSetNewPassphraseAndCommitEncryptedData) {
  const std::string title1 = "Hello world";
  const std::string title2 = "Bookmark #2";
  const GURL page_url1("https://google.com/");
  const GURL page_url2("https://example.com/");

  ASSERT_TRUE(SetupSync());
  GetSyncService()->GetUserSettings()->SetEncryptionPassphrase("hunter2");

  ASSERT_TRUE(AddURL(/*profile=*/0, title1, page_url1));
  ASSERT_TRUE(AddURL(/*profile=*/0, title2, page_url2));

  ASSERT_TRUE(WaitForNigori(PassphraseType::kCustomPassphrase));
  NigoriSpecifics nigori;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &nigori));
  EXPECT_EQ(nigori.custom_passphrase_key_derivation_method(),
            sync_pb::NigoriSpecifics::SCRYPT_8192_8_11);

  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      {{title1, page_url1}, {title2, page_url2}},
      /*passphrase=*/"hunter2"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       ShouldDecryptPbkdf2KeyEncryptedData) {
  const KeyParamsForTesting kKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("hunter2");
  InjectEncryptedServerBookmark("PBKDF2-encrypted bookmark",
                                GURL("http://example.com/doesnt-matter"),
                                kKeyParams);
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());
  ASSERT_TRUE(SetupSync(WAIT_FOR_SYNC_SETUP_TO_COMPLETE));
  EXPECT_TRUE(GetSyncService()->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  EXPECT_TRUE(WaitForPassphraseAccepted());

  EXPECT_TRUE(WaitForClientBookmarkWithTitle("PBKDF2-encrypted bookmark"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       ShouldEncryptDataWithPbkdf2Key) {
  const KeyParamsForTesting kKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());
  ASSERT_TRUE(SetupSync(WAIT_FOR_SYNC_SETUP_TO_COMPLETE));
  EXPECT_TRUE(GetSyncService()->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  EXPECT_TRUE(WaitForPassphraseAccepted());

  const std::string kTitle = "Should be encrypted";
  const GURL kURL("https://google.com/encrypted");
  ASSERT_TRUE(AddURL(/*profile=*/0, kTitle, kURL));

  EXPECT_TRUE(WaitForEncryptedServerBookmarks({{kTitle, kURL}},
                                              /*passphrase=*/"hunter2"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       ShouldDecryptScryptKeyEncryptedData) {
  const KeyParamsForTesting kKeyParams =
      ScryptPassphraseKeyParamsForTesting("hunter2");
  InjectEncryptedServerBookmark("scypt-encrypted bookmark",
                                GURL("http://example.com/doesnt-matter"),
                                kKeyParams);
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());

  ASSERT_TRUE(SetupSync(WAIT_FOR_SYNC_SETUP_TO_COMPLETE));
  EXPECT_TRUE(GetSyncService()->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  EXPECT_TRUE(WaitForPassphraseAccepted());

  EXPECT_TRUE(WaitForClientBookmarkWithTitle("scypt-encrypted bookmark"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       ShouldEncryptDataWithScryptKey) {
  const KeyParamsForTesting kKeyParams =
      ScryptPassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());
  ASSERT_TRUE(SetupSync(WAIT_FOR_SYNC_SETUP_TO_COMPLETE));
  EXPECT_TRUE(GetSyncService()->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  EXPECT_TRUE(WaitForPassphraseAccepted());

  const std::string kTitle = "Should be encrypted";
  const GURL kURL("https://google.com/encrypted");
  ASSERT_TRUE(AddURL(/*profile=*/0, kTitle, kURL));

  EXPECT_TRUE(WaitForEncryptedServerBookmarks({{kTitle, kURL}},
                                              /*passphrase=*/"hunter2"));
}

// PRE_* tests aren't supported on Android browser tests.
#if !BUILDFLAG(IS_ANDROID)
// Populates custom passphrase Nigori without keystore keys to the client.
IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       PRE_CanDecryptWithKeystoreKeys) {
  const KeyParamsForTesting key_params =
      Pbkdf2PassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(key_params),
                        GetFakeServer());
  ASSERT_TRUE(SetupSync(WAIT_FOR_SYNC_SETUP_TO_COMPLETE));
  ASSERT_TRUE(GetSyncService()->GetUserSettings()->SetDecryptionPassphrase(
      key_params.password));
  ASSERT_TRUE(WaitForPassphraseAccepted());
}

// Client should be able to decrypt with keystore keys, regardless whether they
// were stored in NigoriSpecifics. It's not a normal state, when the server
// stores some data encrypted with keystore keys, but client is able to
// reencrypt the data and recover from this state.
IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       CanDecryptWithKeystoreKeys) {
  const password_manager::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(0);
  passwords_helper::InjectKeystoreEncryptedServerPassword(password_form,
                                                          GetFakeServer());
  ASSERT_TRUE(SetupClients());
  EXPECT_TRUE(
      PasswordFormsChecker(/*index=*/0, /*expected_forms=*/{password_form})
          .Wait());
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       DoesNotLeakUnencryptedData) {
  const std::string title = "Should be encrypted";
  const GURL page_url("https://google.com/encrypted");
  ASSERT_TRUE(SetupClients());

  // Create local bookmarks before setting up sync.
  CommittedBookmarkEntityNameObserver observer(GetFakeServer());
  ASSERT_TRUE(AddURL(/*profile=*/0, title, page_url));

  // Mimic custom passphrase being set during initial sync setup.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  GetSyncService()->SetSyncFeatureRequested();
  GetSyncService()->GetUserSettings()->SetEncryptionPassphrase("hunter2");
  GetClient(0)->FinishSyncSetup();

  ASSERT_TRUE(WaitForNigori(PassphraseType::kCustomPassphrase));
  // Ensure that only encrypted bookmarks were committed and that they are
  // encrypted using custom passprhase.
  EXPECT_TRUE(WaitForEncryptedServerBookmarks({{title, page_url}},
                                              /*passphrase=*/"hunter2"));
  EXPECT_THAT(observer.GetCommittedEntityNames(), ElementsAre("encrypted"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       ReencryptsDataWhenPassphraseIsSet) {
  const std::string title = "Re-encryption is great";
  const GURL page_url("https://google.com/re-encrypted");
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(WaitForNigori(PassphraseType::kKeystorePassphrase));
  ASSERT_TRUE(AddURL(/*profile=*/0, title, page_url));
  const std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark> expected =
      {{title, page_url}};
  ASSERT_TRUE(WaitForUnencryptedServerBookmarks(expected));

  GetSyncService()->GetUserSettings()->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForNigori(PassphraseType::kCustomPassphrase));

  // If WaitForEncryptedServerBookmarks() succeeds, that means that a
  // cryptographer initialized with only the key params was able to decrypt the
  // data, so the data must be encrypted using a passphrase-derived key (and not
  // e.g. the previous keystore key which was stored in the Nigori keybag),
  // because that cryptographer has never seen the server-side Nigori.
  EXPECT_TRUE(
      WaitForEncryptedServerBookmarks(expected, /*passphrase=*/"hunter2"));
}

// Tests that on receiving CLIENT_DATA_OBSOLETE passphrase is silently restored,
// e.g. user input is not needed.
IN_PROC_BROWSER_TEST_F(
    SingleClientCustomPassphraseSyncTest,
    ShouldRestorePassphraseOnClientDataObsoleteResponseWhenPassphraseSetByDecryption) {
  // Set up sync with custom passphrase.
  ASSERT_TRUE(SetupSync());
  const KeyParamsForTesting kKeyParams =
      ScryptPassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());
  ASSERT_TRUE(WaitForPassphraseRequired());
  ASSERT_TRUE(GetSyncService()->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  ASSERT_TRUE(WaitForPassphraseAccepted());

  // Mimic going through CLIENT_DATA_OBSOLETE state.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::CLIENT_DATA_OBSOLETE);
  // Trigger sync by making one more change.
  ASSERT_TRUE(AddURL(/*profile=*/0, /*title=*/"title1",
                     GURL("https://www.google.com")));
  ASSERT_TRUE(SyncEngineStoppedChecker(GetSyncService()).Wait());
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::SUCCESS);
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // Make sure the client is still able to decrypt the data.
  EXPECT_TRUE(WaitForPassphraseAccepted());
  const std::string kEncryptedBookmarkTitle = "title2";
  InjectEncryptedServerBookmark(kEncryptedBookmarkTitle,
                                GURL("https://www.google.com"), kKeyParams);
  EXPECT_TRUE(WaitForClientBookmarkWithTitle(kEncryptedBookmarkTitle));
}

// Similar to the above, but passphrase is obtained by
// SetEncryptionPassphrase(). Regression test for crbug.com/1298062.
IN_PROC_BROWSER_TEST_F(
    SingleClientCustomPassphraseSyncTest,
    ShouldRestorePassphraseOnClientDataObsoleteResponseWhenPassphraseSetByEncryption) {
  // Set up sync with custom passphrase.
  ASSERT_TRUE(SetupSync());
  const std::string kPassphrase = "hunter2";
  // Mimic user just enabled custom passphrase.
  GetSyncService()->GetUserSettings()->SetEncryptionPassphrase(kPassphrase);
  ASSERT_TRUE(WaitForNigori(PassphraseType::kCustomPassphrase));

  // Mimic going through CLIENT_DATA_OBSOLETE state.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::CLIENT_DATA_OBSOLETE);
  // Trigger sync by making one more change.
  ASSERT_TRUE(AddURL(/*profile=*/0, /*title=*/"title1",
                     GURL("https://www.google.com")));
  ASSERT_TRUE(SyncEngineStoppedChecker(GetSyncService()).Wait());
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::SUCCESS);
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // Make sure the client is still able to decrypt the data.
  EXPECT_TRUE(WaitForPassphraseAccepted());
  const std::string kEncryptedBookmarkTitle = "title2";

  NigoriSpecifics nigori;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &nigori));
  const KeyParamsForTesting key_params{
      syncer::InitCustomPassphraseKeyDerivationParamsFromNigori(nigori),
      kPassphrase};

  InjectEncryptedServerBookmark(kEncryptedBookmarkTitle,
                                GURL("https://www.google.com"), key_params);
  EXPECT_TRUE(WaitForClientBookmarkWithTitle(kEncryptedBookmarkTitle));
}

}  // namespace
