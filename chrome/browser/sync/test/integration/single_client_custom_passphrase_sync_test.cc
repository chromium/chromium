// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori_test_utils.h"
#include "components/sync/test/fake_server/fake_server_nigori_helper.h"
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
using sync_pb::EncryptedData;
using sync_pb::NigoriSpecifics;
using sync_pb::SyncEntity;
using syncer::CreateCustomPassphraseNigori;
using syncer::Cryptographer;
using syncer::GetEncryptedBookmarkEntitySpecifics;
using syncer::InitCustomPassphraseCryptographerFromNigori;
using syncer::KeyDerivationParams;
using syncer::KeyParamsForTesting;
using syncer::LoopbackServerEntity;
using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::PassphraseType;
using syncer::ProtoPassphraseInt32ToEnum;
using syncer::SyncService;
using testing::ElementsAre;
using testing::SizeIs;

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

  void OnCommit(const std::string& committer_invalidator_client_id,
                ModelTypeSet committed_model_types) override {
    sync_pb::ClientToServerMessage message;
    fake_server_->GetLastCommitMessage(&message);
    for (const sync_pb::SyncEntity& entity : message.commit().entries()) {
      if (syncer::GetModelTypeFromSpecifics(entity.specifics()) ==
          syncer::BOOKMARKS) {
        committed_names_.insert(entity.name());
      }
    }
  }

  const std::set<std::string> GetCommittedEntityNames() const {
    return committed_names_;
  }

 private:
  FakeServer* const fake_server_;
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
  ~SingleClientCustomPassphraseSyncTest() override {}

  // Waits until the given set of bookmarks appears on the server, encrypted
  // according to the server-side Nigori and with the given passphrase.
  bool WaitForEncryptedServerBookmarks(
      const std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark>&
          expected_bookmarks,
      const std::string& passphrase) {
    auto cryptographer = CreateCryptographerFromServerNigori(passphrase);
    return ServerBookmarksEqualityChecker(GetSyncService(), GetFakeServer(),
                                          expected_bookmarks,
                                          cryptographer.get())
        .Wait();
  }

  // Waits until the given set of bookmarks appears on the server, encrypted
  // with the precise KeyParamsForTesting given.
  bool WaitForEncryptedServerBookmarks(
      const std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark>&
          expected_bookmarks,
      const KeyParamsForTesting& key_params) {
    auto cryptographer = syncer::CryptographerImpl::FromSingleKeyForTesting(
        key_params.password, key_params.derivation_params);
    return ServerBookmarksEqualityChecker(GetSyncService(), GetFakeServer(),
                                          expected_bookmarks,
                                          cryptographer.get())
        .Wait();
  }

  bool WaitForUnencryptedServerBookmarks(
      const std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark>&
          expected_bookmarks) {
    return ServerBookmarksEqualityChecker(GetSyncService(), GetFakeServer(),
                                          expected_bookmarks,
                                          /*cryptographer=*/nullptr)
        .Wait();
  }

  bool WaitForNigori(PassphraseType expected_passphrase_type) {
    return ServerNigoriChecker(GetSyncService(), GetFakeServer(),
                               expected_passphrase_type)
        .Wait();
  }

  bool WaitForPassphraseRequiredState(bool desired_state) {
    return PassphraseRequiredStateChecker(GetSyncService(), desired_state)
        .Wait();
  }

  bool WaitForClientBookmarkWithTitle(std::string title) {
    return BookmarksTitleChecker(/*profile_index=*/0, title,
                                 /*expected_count=*/1)
        .Wait();
  }

  syncer::ProfileSyncService* GetSyncService() {
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

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientCustomPassphraseSyncTest);
};

class SingleClientCustomPassphraseDoNotUseScryptSyncTest
    : public SingleClientCustomPassphraseSyncTest {
 public:
  SingleClientCustomPassphraseDoNotUseScryptSyncTest()
      : features_(/*force_disabled=*/false, /*use_for_new_passphrases=*/false) {
  }

 private:
  ScopedScryptFeatureToggler features_;
};

class SingleClientCustomPassphraseUseScryptSyncTest
    : public SingleClientCustomPassphraseSyncTest {
 public:
  SingleClientCustomPassphraseUseScryptSyncTest()
      : features_(/*force_disabled=*/false, /*use_for_new_passphrases=*/true) {}

 private:
  ScopedScryptFeatureToggler features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       CommitsEncryptedData) {
  const std::string title1 = "Hello world";
  const std::string title2 = "Bookmark #2";
  const GURL page_url1("https://google.com/");
  const GURL page_url2("https://example.com/");

  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AddURL(/*profile=*/0, title1, page_url1));
  ASSERT_TRUE(AddURL(/*profile=*/0, title2, page_url2));
  ASSERT_TRUE(WaitForNigori(PassphraseType::kCustomPassphrase));

  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      {{title1, page_url1}, {title2, page_url2}},
      /*passphrase=*/"hunter2"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       CanDecryptPbkdf2KeyEncryptedData) {
  KeyParamsForTesting key_params = {KeyDerivationParams::CreateForPbkdf2(),
                                    "hunter2"};
  InjectEncryptedServerBookmark("PBKDF2-encrypted bookmark",
                                GURL("http://example.com/doesnt-matter"),
                                key_params);
  SetNigoriInFakeServer(CreateCustomPassphraseNigori(key_params),
                        GetFakeServer());
  SetDecryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForPassphraseRequiredState(/*desired_state=*/false));

  EXPECT_TRUE(WaitForClientBookmarkWithTitle("PBKDF2-encrypted bookmark"));
}

// PRE_* tests aren't supported on Android browser tests.
#if !defined(OS_ANDROID)
// Populates custom passphrase Nigori without keystore keys to the client.
IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       PRE_CanDecryptWithKeystoreKeys) {
  const KeyParamsForTesting key_params = {
      KeyDerivationParams::CreateForPbkdf2(), "hunter2"};
  SetNigoriInFakeServer(CreateCustomPassphraseNigori(key_params),
                        GetFakeServer());
  SetDecryptionPassphraseForClient(/*index=*/0, key_params.password);
  ASSERT_TRUE(SetupSync());
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
#endif  // !defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseDoNotUseScryptSyncTest,
                       CommitsEncryptedDataUsingPbkdf2WhenScryptDisabled) {
  const std::string title = "PBKDF2 encrypted";
  const GURL page_url("https://google.com/pbkdf2-encrypted");

  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AddURL(/*profile=*/0, title, page_url));

  ASSERT_TRUE(WaitForNigori(PassphraseType::kCustomPassphrase));
  NigoriSpecifics nigori;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &nigori));
  EXPECT_EQ(nigori.custom_passphrase_key_derivation_method(),
            sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003);
  EXPECT_TRUE(WaitForEncryptedServerBookmarks({{title, page_url}},
                                              /*passphrase=*/"hunter2"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseUseScryptSyncTest,
                       CommitsEncryptedDataUsingScryptWhenScryptEnabled) {
  const std::string title = "scrypt encrypted";
  const GURL page_url("https://google.com/scrypt-encrypted");

  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AddURL(/*profile=*/0, title, page_url));

  ASSERT_TRUE(WaitForNigori(PassphraseType::kCustomPassphrase));
  NigoriSpecifics nigori;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &nigori));
  EXPECT_EQ(nigori.custom_passphrase_key_derivation_method(),
            sync_pb::NigoriSpecifics::SCRYPT_8192_8_11);
  EXPECT_TRUE(WaitForEncryptedServerBookmarks({{title, page_url}},
                                              /*passphrase=*/"hunter2"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseDoNotUseScryptSyncTest,
                       CanDecryptScryptKeyEncryptedDataWhenScryptNotDisabled) {
  KeyParamsForTesting key_params = {
      KeyDerivationParams::CreateForScrypt("someConstantSalt"), "hunter2"};
  InjectEncryptedServerBookmark("scypt-encrypted bookmark",
                                GURL("http://example.com/doesnt-matter"),
                                key_params);
  SetNigoriInFakeServer(CreateCustomPassphraseNigori(key_params),
                        GetFakeServer());
  SetDecryptionPassphraseForClient(/*index=*/0, "hunter2");

  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForPassphraseRequiredState(/*desired_state=*/false));

  EXPECT_TRUE(WaitForClientBookmarkWithTitle("scypt-encrypted bookmark"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseDoNotUseScryptSyncTest,
                       DoesNotLeakUnencryptedData) {
  const std::string title = "Should be encrypted";
  const GURL page_url("https://google.com/encrypted");
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupClients());

  // Create local bookmarks before sync is enabled.
  ASSERT_TRUE(AddURL(/*profile=*/0, title, page_url));

  CommittedBookmarkEntityNameObserver observer(GetFakeServer());
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(WaitForNigori(PassphraseType::kCustomPassphrase));
  // If WaitForEncryptedServerBookmarks() succeeds, that means that a
  // cryptographer initialized with only the key params was able to decrypt the
  // data, so the data must be encrypted using a passphrase-derived key (and not
  // e.g. a keystore key), because that cryptographer has never seen the
  // server-side Nigori. Furthermore, if a bookmark commit has happened only
  // once, we are certain that no bookmarks other than those we've verified to
  // be encrypted have been committed.
  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      {{title, page_url}},
      {KeyDerivationParams::CreateForPbkdf2(), "hunter2"}));
  EXPECT_THAT(observer.GetCommittedEntityNames(), ElementsAre("encrypted"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseDoNotUseScryptSyncTest,
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
  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      expected, {KeyDerivationParams::CreateForPbkdf2(), "hunter2"}));
}

}  // namespace
