// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/sync.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/sync/chromeos/explicit_passphrase_mojo_utils.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori_test_utils.h"
#include "components/sync/protocol/os_preference_specifics.pb.h"
#include "components/sync/test/fake_server/fake_server_nigori_helper.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

void InjectEncryptedServerOsPreference(
    const sync_pb::OsPreferenceSpecifics& unencrypted_specifics,
    const syncer::KeyParamsForTesting& key_params,
    fake_server::FakeServer* fake_server) {
  sync_pb::EntitySpecifics wrapped_unencrypted_specifics;
  *wrapped_unencrypted_specifics.mutable_os_preference() =
      unencrypted_specifics;

  sync_pb::EntitySpecifics encrypted_specifics;
  auto cryptographer = syncer::CryptographerImpl::FromSingleKeyForTesting(
      key_params.password, key_params.derivation_params);
  bool encrypt_result = cryptographer->Encrypt(
      wrapped_unencrypted_specifics, encrypted_specifics.mutable_encrypted());
  *encrypted_specifics.mutable_os_preference() =
      sync_pb::OsPreferenceSpecifics();
  DCHECK(encrypt_result);

  fake_server->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"encrypted",
          /*client_tag=*/unencrypted_specifics.preference().name(),
          encrypted_specifics, /*creation_time=*/0, /*last_modified_time=*/0));
}

std::string PrefValueToProtoString(const base::Value& value) {
  std::string result;
  bool success = base::JSONWriter::Write(value, &result);
  DCHECK(success);
  return result;
}

class PassphraseRequiredNotifiedToCrosapiObserverChecker
    : public StatusChangeChecker,
      public crosapi::mojom::SyncExplicitPassphraseClientObserver {
 public:
  explicit PassphraseRequiredNotifiedToCrosapiObserverChecker(
      mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient>*
          remote_explicit_passphrase_client) {
    DCHECK(remote_explicit_passphrase_client);
    remote_explicit_passphrase_client->get()->AddObserver(
        receiver_.BindNewPipeAndPassRemote());
  }

  PassphraseRequiredNotifiedToCrosapiObserverChecker(
      const PassphraseRequiredNotifiedToCrosapiObserverChecker&) = delete;
  PassphraseRequiredNotifiedToCrosapiObserverChecker& operator=(
      const PassphraseRequiredNotifiedToCrosapiObserverChecker&) = delete;
  ~PassphraseRequiredNotifiedToCrosapiObserverChecker() override = default;

  // crosapi::mojom::SyncExplicitPassphraseClientObserver implementation.
  void OnPassphraseAvailable() override {}

  void OnPassphraseRequired() override {
    passphrase_required_notified_ = true;
    CheckExitCondition();
  }

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for OnPassphraseRequired() call for crosapi observer";
    return passphrase_required_notified_;
  }

 private:
  bool passphrase_required_notified_ = false;
  mojo::Receiver<crosapi::mojom::SyncExplicitPassphraseClientObserver>
      receiver_{this};
};

class AshCustomPassphraseSharingSyncTest : public SyncTest {
 public:
  AshCustomPassphraseSharingSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures({chromeos::features::kLacrosSupport,
                                    chromeos::features::kLacrosPrimary},
                                   {});
  }

  AshCustomPassphraseSharingSyncTest(
      const AshCustomPassphraseSharingSyncTest&) = delete;
  AshCustomPassphraseSharingSyncTest& operator=(
      const AshCustomPassphraseSharingSyncTest&) = delete;

  ~AshCustomPassphraseSharingSyncTest() override = default;

  // SyncTest overrides.
  base::FilePath GetProfileBaseName(int index) override {
    // Need to reuse test user profile for this test - Crosapi explicitly
    // assumes there is only one regular profile.
    // TODO(crbug.com/1102768): eventually this should be the case for all Ash
    // tests.
    DCHECK_EQ(index, 0);
    return base::FilePath(chrome::kTestUserProfileDir);
  }

  void SetupCrosapi() {
    crosapi::CrosapiAsh* crosapi_ash =
        crosapi::CrosapiManager::Get()->crosapi_ash();
    DCHECK(crosapi_ash);

    crosapi_ash->BindSyncService(
        sync_mojo_service_remote_.BindNewPipeAndPassReceiver());
    sync_mojo_service_remote_.get()->BindExplicitPassphraseClient(
        explicit_passphrase_client_remote_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient>*
  explicit_passphrase_client_remote() {
    return &explicit_passphrase_client_remote_;
  }

  void MimicDecryptionKeyProvidedByLacros(
      const syncer::KeyParamsForTesting& key_params) {
    auto nigori = syncer::Nigori::CreateByDerivation(
        key_params.derivation_params, key_params.password);

    auto account_key = crosapi::mojom::AccountKey::New();
    account_key->id = GetSyncService(0)->GetAccountInfo().gaia;
    account_key->account_type = crosapi::mojom::AccountType::kGaia;

    explicit_passphrase_client_remote_.get()->SetDecryptionNigoriKey(
        std::move(account_key),
        /*decryption_key=*/syncer::NigoriToMojo(*nigori));
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  mojo::Remote<crosapi::mojom::SyncService> sync_mojo_service_remote_;
  mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient>
      explicit_passphrase_client_remote_;
};

IN_PROC_BROWSER_TEST_F(AshCustomPassphraseSharingSyncTest,
                       ShouldNotifyObserversAndAcceptPassphraseFromLacros) {
  ASSERT_TRUE(SetupSync());
  SetupCrosapi();

  PassphraseRequiredNotifiedToCrosapiObserverChecker
      passphrase_required_notified_to_crosapi_observer_checker(
          explicit_passphrase_client_remote());

  // Mimic custom passphrase being set by other client.
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::ScryptPassphraseKeyParamsForTesting("hunter2");
  fake_server::SetNigoriInFakeServer(
      syncer::BuildCustomPassphraseNigoriSpecifics(kKeyParams),
      GetFakeServer());

  // Inject encrypted os preference with non-default value.
  sync_pb::OsPreferenceSpecifics os_preference_specifics;
  const base::Value kNewPrefValue = base::Value(true);
  os_preference_specifics.mutable_preference()->set_name(
      prefs::kResolveTimezoneByGeolocationMigratedToMethod);
  os_preference_specifics.mutable_preference()->set_value(
      PrefValueToProtoString(kNewPrefValue));
  InjectEncryptedServerOsPreference(os_preference_specifics, kKeyParams,
                                    GetFakeServer());

  // Data isn't decryptable yet, client should enter passphrase required state,
  // notify observers (Lacros) via crosapi and have default preference value.
  EXPECT_TRUE(passphrase_required_notified_to_crosapi_observer_checker.Wait());
  ASSERT_TRUE(
      PassphraseRequiredStateChecker(GetSyncService(0), /*desired_state=*/true)
          .Wait());
  ASSERT_NE(*preferences_helper::GetPrefs(0)->Get(
                prefs::kResolveTimezoneByGeolocationMigratedToMethod),
            kNewPrefValue);

  // Mimic passphrase being provided by Lacros, verify that passphrase is no
  // longer required and the data is decryptable.
  MimicDecryptionKeyProvidedByLacros(kKeyParams);
  EXPECT_TRUE(
      PassphraseRequiredStateChecker(GetSyncService(0), /*desired_state=*/false)
          .Wait());
  EXPECT_TRUE(BooleanPrefValueChecker(
                  preferences_helper::GetPrefs(0),
                  prefs::kResolveTimezoneByGeolocationMigratedToMethod,
                  kNewPrefValue.GetBool())
                  .Wait());
}

}  // namespace
