// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/cryptographer.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/system_encryptor.h"

namespace {

using bookmarks_helper::AddURL;
using bookmarks_helper::CreateBookmarkServerEntity;
using encryption_helper::CreateCustomPassphraseNigori;
using encryption_helper::GetEncryptedBookmarkEntitySpecifics;
using encryption_helper::GetServerNigori;
using encryption_helper::InitCustomPassphraseCryptographerFromNigori;
using encryption_helper::SetNigoriInFakeServer;
using fake_server::FakeServer;
using sync_pb::EncryptedData;
using sync_pb::NigoriSpecifics;
using sync_pb::SyncEntity;
using syncer::Cryptographer;
using syncer::KeyDerivationParams;
using syncer::KeyParams;
using syncer::LoopbackServerEntity;
using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::PassphraseType;
using syncer::ProtoPassphraseTypeToEnum;
using syncer::SyncService;
using syncer::SystemEncryptor;

class DatatypeCommitCountingFakeServerObserver : public FakeServer::Observer {
 public:
  explicit DatatypeCommitCountingFakeServerObserver(FakeServer* fake_server)
      : fake_server_(fake_server) {
    fake_server->AddObserver(this);
  }

  void OnCommit(const std::string& committer_id,
                ModelTypeSet committed_model_types) override {
    for (ModelType type : committed_model_types) {
      ++datatype_commit_counts_[type];
    }
  }

  int GetCommitCountForDatatype(ModelType type) {
    return datatype_commit_counts_[type];
  }

  ~DatatypeCommitCountingFakeServerObserver() override {
    fake_server_->RemoveObserver(this);
  }

 private:
  FakeServer* fake_server_;
  std::map<syncer::ModelType, int> datatype_commit_counts_;
};

// These tests use a gray-box testing approach to verify that the data committed
// to the server is encrypted properly, and that properly-encrypted data from
// the server is successfully decrypted by the client. They also verify that the
// key derivation methods are set, read and handled properly. They do not,
// however, directly ensure that two clients syncing through the same account
// will be able to access each others' data in the presence of a custom
// passphrase. For this, a separate two-client test will be used.
//
// TODO(davidovic): Add two-client tests and update the above comment.
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
  // with the precise KeyParams given.
  bool WaitForEncryptedServerBookmarks(
      const std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark>&
          expected_bookmarks,
      const KeyParams& key_params) {
    auto cryptographer = CreateCryptographerWithKeyParams(key_params);
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

  browser_sync::ProfileSyncService* GetSyncService() {
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
    EXPECT_EQ(ProtoPassphraseTypeToEnum(nigori.passphrase_type()),
              PassphraseType::CUSTOM_PASSPHRASE);
    auto cryptographer = std::make_unique<Cryptographer>(&system_encryptor_);
    InitCustomPassphraseCryptographerFromNigori(nigori, cryptographer.get(),
                                                passphrase);
    return cryptographer;
  }

  // A cryptographer initialized with the given KeyParams has not "seen" the
  // server-side Nigori, and so any data decryptable by such a cryptographer
  // does not depend on external info.
  std::unique_ptr<Cryptographer> CreateCryptographerWithKeyParams(
      const KeyParams& key_params) {
    auto cryptographer = std::make_unique<Cryptographer>(&system_encryptor_);
    cryptographer->AddKey(key_params);
    return cryptographer;
  }

  void InjectEncryptedServerBookmark(const std::string& title,
                                     const GURL& url,
                                     const KeyParams& key_params) {
    std::unique_ptr<LoopbackServerEntity> server_entity =
        CreateBookmarkServerEntity(title, url);
    server_entity->SetSpecifics(GetEncryptedBookmarkEntitySpecifics(
        server_entity->GetSpecifics().bookmark(), key_params));
    GetFakeServer()->InjectEntity(std::move(server_entity));
  }

 private:
  SystemEncryptor system_encryptor_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientCustomPassphraseSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       CommitsEncryptedData) {
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(
      AddURL(/*profile=*/0, "Hello world", GURL("https://google.com/")));
  ASSERT_TRUE(
      AddURL(/*profile=*/0, "Bookmark #2", GURL("https://example.com/")));
  ASSERT_TRUE(WaitForNigori(PassphraseType::CUSTOM_PASSPHRASE));

  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      {{"Hello world", GURL("https://google.com/")},
       {"Bookmark #2", GURL("https://example.com/")}},
      /*passphrase=*/"hunter2"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       CommitsEncryptedDataUsingPbkdf2WhenScryptDisabled) {
  ScopedScryptFeatureToggler toggler(/*force_disabled=*/false,
                                     /*use_for_new_passphrases=*/false);
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AddURL(/*profile=*/0, "PBKDF2 encrypted",
                     GURL("https://google.com/pbkdf2-encrypted")));

  ASSERT_TRUE(WaitForNigori(PassphraseType::CUSTOM_PASSPHRASE));
  NigoriSpecifics nigori;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &nigori));
  EXPECT_EQ(nigori.custom_passphrase_key_derivation_method(),
            sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003);
  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      {{"PBKDF2 encrypted", GURL("https://google.com/pbkdf2-encrypted")}},
      /*passphrase=*/"hunter2"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       CommitsEncryptedDataUsingScryptWhenScryptEnabled) {
  ScopedScryptFeatureToggler toggler(/*force_disabled=*/false,
                                     /*use_for_new_passphrases=*/true);
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AddURL(/*profile=*/0, "scrypt encrypted",
                     GURL("https://google.com/scrypt-encrypted")));

  ASSERT_TRUE(WaitForNigori(PassphraseType::CUSTOM_PASSPHRASE));
  NigoriSpecifics nigori;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &nigori));
  EXPECT_EQ(nigori.custom_passphrase_key_derivation_method(),
            sync_pb::NigoriSpecifics::SCRYPT_8192_8_11);
  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      {{"scrypt encrypted", GURL("https://google.com/scrypt-encrypted")}},
      /*passphrase=*/"hunter2"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       CanDecryptPbkdf2KeyEncryptedData) {
  KeyParams key_params = {KeyDerivationParams::CreateForPbkdf2(), "hunter2"};
  InjectEncryptedServerBookmark("PBKDF2-encrypted bookmark",
                                GURL("http://example.com/doesnt-matter"),
                                key_params);
  SetNigoriInFakeServer(GetFakeServer(),
                        CreateCustomPassphraseNigori(key_params));
  SetDecryptionPassphraseForClient(/*index=*/0, "hunter2");

  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForPassphraseRequiredState(/*desired_state=*/false));

  EXPECT_TRUE(WaitForClientBookmarkWithTitle("PBKDF2-encrypted bookmark"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       CanDecryptScryptKeyEncryptedDataWhenScryptNotDisabled) {
  ScopedScryptFeatureToggler toggler(/*force_disabled=*/false,
                                     /*use_for_new_passphrases_=*/false);
  KeyParams key_params = {
      KeyDerivationParams::CreateForScrypt("someConstantSalt"), "hunter2"};
  InjectEncryptedServerBookmark("scypt-encrypted bookmark",
                                GURL("http://example.com/doesnt-matter"),
                                key_params);
  SetNigoriInFakeServer(GetFakeServer(),
                        CreateCustomPassphraseNigori(key_params));
  SetDecryptionPassphraseForClient(/*index=*/0, "hunter2");

  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForPassphraseRequiredState(/*desired_state=*/false));

  EXPECT_TRUE(WaitForClientBookmarkWithTitle("scypt-encrypted bookmark"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       CannotDecryptScryptKeyEncryptedDataWhenScryptDisabled) {
  KeyParams key_params = {
      KeyDerivationParams::CreateForScrypt("someConstantSalt"), "hunter2"};
  sync_pb::NigoriSpecifics nigori = CreateCustomPassphraseNigori(key_params);
  InjectEncryptedServerBookmark("scypt-encrypted bookmark",
                                GURL("http://example.com/doesnt-matter"),
                                key_params);
  // Can only set feature state now because creating a Nigori and injecting an
  // encrypted bookmark both require key derivation using scrypt.
  ScopedScryptFeatureToggler toggler(/*force_disabled=*/true,
                                     /*use_for_new_passphrases_=*/false);
  SetNigoriInFakeServer(GetFakeServer(), nigori);
  SetDecryptionPassphraseForClient(/*index=*/0, "hunter2");

  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(WaitForPassphraseRequiredState(/*desired_state=*/true));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       DoesNotLeakUnencryptedData) {
  ScopedScryptFeatureToggler toggler(/*force_disabled=*/false,
                                     /*use_for_new_passphrases=*/false);
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  DatatypeCommitCountingFakeServerObserver observer(GetFakeServer());
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AddURL(/*profile=*/0, "Should be encrypted",
                     GURL("https://google.com/encrypted")));

  ASSERT_TRUE(WaitForNigori(PassphraseType::CUSTOM_PASSPHRASE));
  // If WaitForEncryptedServerBookmarks() succeeds, that means that a
  // cryptographer initialized with only the key params was able to decrypt the
  // data, so the data must be encrypted using a passphrase-derived key (and not
  // e.g. a keystore key), because that cryptographer has never seen the
  // server-side Nigori. Furthermore, if a bookmark commit has happened only
  // once, we are certain that no bookmarks other than those we've verified to
  // be encrypted have been committed.
  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      {{"Should be encrypted", GURL("https://google.com/encrypted")}},
      {KeyDerivationParams::CreateForPbkdf2(), "hunter2"}));
  EXPECT_EQ(observer.GetCommitCountForDatatype(syncer::BOOKMARKS), 1);
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       ReencryptsDataWhenPassphraseIsSet) {
  ScopedScryptFeatureToggler toggler(/*force_disabled=*/false,
                                     /*use_for_new_passphrases=*/false);
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(WaitForNigori(PassphraseType::KEYSTORE_PASSPHRASE));
  ASSERT_TRUE(AddURL(/*profile=*/0, "Re-encryption is great",
                     GURL("https://google.com/re-encrypted")));
  std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark> expected = {
      {"Re-encryption is great", GURL("https://google.com/re-encrypted")}};
  ASSERT_TRUE(WaitForUnencryptedServerBookmarks(expected));

  GetSyncService()->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForNigori(PassphraseType::CUSTOM_PASSPHRASE));

  // If WaitForEncryptedServerBookmarks() succeeds, that means that a
  // cryptographer initialized with only the key params was able to decrypt the
  // data, so the data must be encrypted using a passphrase-derived key (and not
  // e.g. the previous keystore key which was stored in the Nigori keybag),
  // because that cryptographer has never seen the server-side Nigori.
  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      expected, {KeyDerivationParams::CreateForPbkdf2(), "hunter2"}));
}

}  // namespace
