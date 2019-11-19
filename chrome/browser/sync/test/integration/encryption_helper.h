// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_ENCRYPTION_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_ENCRYPTION_HELPER_H_

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/syncable/directory_cryptographer.h"
#include "components/sync/test/fake_server/fake_server.h"

namespace encryption_helper {

// Given a |fake_server|, fetches its Nigori node and writes it to the
// proto pointed to by |nigori|. Returns false if the server does not contain
// exactly one Nigori node.
bool GetServerNigori(fake_server::FakeServer* fake_server,
                     sync_pb::NigoriSpecifics* nigori);

// Given a |fake_server|, sets the Nigori instance stored in it to |nigori|.
void SetNigoriInFakeServer(fake_server::FakeServer* fake_server,
                           const sync_pb::NigoriSpecifics& nigori);

// Given a |nigori| with CUSTOM_PASSPHRASE passphrase type, initializes the
// given |cryptographer| with the key described in it. Since the key inside the
// Nigori is encrypted (by design), the provided |passphrase| will be used to
// decrypt it. This function will fail the test (using ASSERT) if the Nigori is
// not a custom passphrase one, or if the key cannot be decrypted.
std::unique_ptr<syncer::Cryptographer>
InitCustomPassphraseCryptographerFromNigori(
    const sync_pb::NigoriSpecifics& nigori,
    const std::string& passphrase);

// Returns an EntitySpecifics containing encrypted data corresponding to the
// provided BookmarkSpecifics and encrypted using the given |key_params|.
sync_pb::EntitySpecifics GetEncryptedBookmarkEntitySpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const syncer::KeyParams& key_params);

// Creates a NigoriSpecifics that describes encryption using a custom passphrase
// with the given key parameters.
sync_pb::NigoriSpecifics CreateCustomPassphraseNigori(
    const syncer::KeyParams& params);

}  // namespace encryption_helper

// Checker used to block until a Nigori with a given passphrase type is
// available on the server.
class ServerNigoriChecker : public SingleClientStatusChangeChecker {
 public:
  ServerNigoriChecker(syncer::ProfileSyncService* service,
                      fake_server::FakeServer* fake_server,
                      syncer::PassphraseType expected_passphrase_type);

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  fake_server::FakeServer* const fake_server_;
  const syncer::PassphraseType expected_passphrase_type_;
};

// Checker used to block until a Nigori with a given keybag encryption key name
// is available on the server.
class ServerNigoriKeyNameChecker : public SingleClientStatusChangeChecker {
 public:
  ServerNigoriKeyNameChecker(const std::string& expected_key_name,
                             syncer::ProfileSyncService* service,
                             fake_server::FakeServer* fake_server);

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  fake_server::FakeServer* const fake_server_;
  const std::string expected_key_name_;
};

// Checker used to block until Sync requires or stops requiring a passphrase.
class PassphraseRequiredStateChecker : public SingleClientStatusChangeChecker {
 public:
  PassphraseRequiredStateChecker(syncer::ProfileSyncService* service,
                                 bool desired_state);

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const bool desired_state_;
};

// Checker used to block until Sync requires or stops requiring trusted vault
// keys.
class TrustedVaultKeyRequiredStateChecker
    : public SingleClientStatusChangeChecker {
 public:
  TrustedVaultKeyRequiredStateChecker(syncer::ProfileSyncService* service,
                                      bool desired_state);

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const bool desired_state_;
};

// Helper for setting scrypt-related feature flags.
// NOTE: DO NOT INSTANTIATE THIS CLASS IN THE TEST BODY FOR INTEGRATION TESTS!
// That causes data races, see crbug.com/915219. Instead, instantiate it in the
// test fixture class.
class ScopedScryptFeatureToggler {
 public:
  ScopedScryptFeatureToggler(bool force_disabled, bool use_for_new_passphrases);

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_ENCRYPTION_HELPER_H_
