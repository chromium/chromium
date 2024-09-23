// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/encryption_helper.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/service/sync_client.h"
#include "components/sync/service/sync_service_impl.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace encryption_helper {

namespace {

GURL GetFakeTrustedVaultRetrievalURL(
    const net::test_server::EmbeddedTestServer& test_server,
    const std::string& gaia_id,
    const std::vector<uint8_t>& encryption_key,
    int encryption_key_version) {
  // encryption_keys_retrieval.html would populate encryption key to
  // TrustedVaultService service upon loading. Key is provided as part of URL
  // and needs to be encoded with Base64, because it is binary.
  const std::string base64_encoded_key = base::Base64Encode(encryption_key);
  return test_server.GetURL(base::StringPrintf(
      "/sync/encryption_keys_retrieval.html?gaia=%s&key=%s&key_version=%d",
      gaia_id.c_str(), base64_encoded_key.c_str(), encryption_key_version));
}

GURL GetFakeTrustedVaultRecoverabilityURL(
    const net::test_server::EmbeddedTestServer& test_server,
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key) {
  // encryption_keys_recoverability.html would populate `public_key` to
  // TrustedVaultService upon loading. Key is provided as part of URL and needs
  // to be encoded with Base64, because it is binary.
  const std::string base64_encoded_public_key = base::Base64Encode(public_key);
  return test_server.GetURL(
      base::StringPrintf("/sync/encryption_keys_recoverability.html?%s#%s",
                         gaia_id.c_str(), base64_encoded_public_key.c_str()));
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

}  // namespace

void SetupFakeTrustedVaultPages(
    const std::string& gaia_id,
    const std::vector<uint8_t>& trusted_vault_key,
    int trusted_vault_key_version,
    const std::vector<uint8_t>& recovery_method_public_key,
    net::test_server::EmbeddedTestServer* test_server) {
  CHECK(test_server);
  // Note that this needs to be installed before the analogous below for
  // retrieval, because they share prefix.
  const GURL recoverability_url = GetFakeTrustedVaultRecoverabilityURL(
      *test_server, gaia_id, recovery_method_public_key);
  test_server->RegisterRequestHandler(base::BindRepeating(
      &HttpServerRedirect,
      /*from_prefix=*/
      GaiaUrls::GetInstance()
          ->signin_chrome_sync_keys_recoverability_degraded_url(),
      /*to=*/recoverability_url));

  const GURL retrieval_url = GetFakeTrustedVaultRetrievalURL(
      *test_server, gaia_id, trusted_vault_key, trusted_vault_key_version);
  test_server->RegisterRequestHandler(base::BindRepeating(
      &HttpServerRedirect,
      /*from_prefix=*/
      GaiaUrls::GetInstance()->signin_chrome_sync_keys_retrieval_url(),
      /*to=*/retrieval_url));
}

}  // namespace encryption_helper

ServerPassphraseTypeChecker::ServerPassphraseTypeChecker(
    syncer::PassphraseType expected_passphrase_type)
    : expected_passphrase_type_(expected_passphrase_type) {}

bool ServerPassphraseTypeChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for a Nigori node with the proper passphrase type to become "
         "available on the server.";

  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server()->GetPermanentSyncEntitiesByDataType(syncer::NIGORI);
  EXPECT_LE(nigori_entities.size(), 1U);
  return !nigori_entities.empty() &&
         syncer::ProtoPassphraseInt32ToEnum(
             nigori_entities[0].specifics().nigori().passphrase_type()) ==
             expected_passphrase_type_;
}

ServerCrossUserSharingPublicKeyChangedChecker::
    ServerCrossUserSharingPublicKeyChangedChecker(
        const std::string& previous_public_key)
    : previous_public_key_(previous_public_key) {}

bool ServerCrossUserSharingPublicKeyChangedChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for a Nigori node with a new cross-user sharing public key"
      << " available on the server";

  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server()->GetPermanentSyncEntitiesByDataType(syncer::NIGORI);
  EXPECT_LE(nigori_entities.size(), 1U);
  return !nigori_entities.empty() &&
         nigori_entities[0]
                 .specifics()
                 .nigori()
                 .cross_user_sharing_public_key()
                 .x25519_public_key() != previous_public_key_;
}

ServerNigoriKeyNameChecker::ServerNigoriKeyNameChecker(
    const std::string& expected_key_name)
    : expected_key_name_(expected_key_name) {}

bool ServerNigoriKeyNameChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server()->GetPermanentSyncEntitiesByDataType(syncer::NIGORI);
  DCHECK_EQ(nigori_entities.size(), 1U);

  const std::string given_key_name =
      nigori_entities[0].specifics().nigori().encryption_keybag().key_name();

  *os << "Waiting for a Nigori node with proper key bag encryption key name ("
      << expected_key_name_ << ") to become available on the server."
      << "The server key bag encryption key name is " << given_key_name << ".";
  return given_key_name == expected_key_name_;
}

PassphraseRequiredChecker::PassphraseRequiredChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}

bool PassphraseRequiredChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Checking whether passhrase is required";
  return service()->IsEngineInitialized() &&
         service()->GetUserSettings()->IsPassphraseRequired();
}

PassphraseAcceptedChecker::PassphraseAcceptedChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}

bool PassphraseAcceptedChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Checking whether passhrase is accepted";
  switch (service()->GetUserSettings()->GetPassphraseType().value_or(
      syncer::PassphraseType::kKeystorePassphrase)) {
    case syncer::PassphraseType::kKeystorePassphrase:
    case syncer::PassphraseType::kTrustedVaultPassphrase:
      return false;
    // With kImplicitPassphrase the user needs to enter the passphrase even
    // though it's not treated as an explicit passphrase.
    case syncer::PassphraseType::kImplicitPassphrase:
    case syncer::PassphraseType::kFrozenImplicitPassphrase:
    case syncer::PassphraseType::kCustomPassphrase:
      break;
  }
  return service()->IsEngineInitialized() &&
         !service()->GetUserSettings()->IsPassphraseRequired();
}

PassphraseTypeChecker::PassphraseTypeChecker(
    syncer::SyncServiceImpl* service,
    syncer::PassphraseType expected_passphrase_type)
    : SingleClientStatusChangeChecker(service),
      expected_passphrase_type_(expected_passphrase_type) {}

bool PassphraseTypeChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Checking expected passhrase type";
  return service()->GetUserSettings()->GetPassphraseType() ==
         expected_passphrase_type_;
}

TrustedVaultKeyRequiredStateChecker::TrustedVaultKeyRequiredStateChecker(
    syncer::SyncServiceImpl* service,
    bool desired_state)
    : SingleClientStatusChangeChecker(service), desired_state_(desired_state) {}

bool TrustedVaultKeyRequiredStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting until trusted vault keys are " +
             std::string(desired_state_ ? "required" : "not required");
  return service()
             ->GetUserSettings()
             ->IsTrustedVaultKeyRequiredForPreferredDataTypes() ==
         desired_state_;
}

TrustedVaultKeysChangedStateChecker::TrustedVaultKeysChangedStateChecker(
    syncer::SyncServiceImpl* service)
    : service_(service) {
  service->GetSyncClientForTest()->GetTrustedVaultClient()->AddObserver(this);
}

TrustedVaultKeysChangedStateChecker::~TrustedVaultKeysChangedStateChecker() {
  service_->GetSyncClientForTest()->GetTrustedVaultClient()->RemoveObserver(
      this);
}

bool TrustedVaultKeysChangedStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for trusted vault keys change";
  return keys_changed_;
}

void TrustedVaultKeysChangedStateChecker::OnTrustedVaultKeysChanged() {
  keys_changed_ = true;
  CheckExitCondition();
}

void TrustedVaultKeysChangedStateChecker::
    OnTrustedVaultRecoverabilityChanged() {}

TrustedVaultRecoverabilityDegradedStateChecker::
    TrustedVaultRecoverabilityDegradedStateChecker(
        syncer::SyncServiceImpl* service,
        bool degraded)
    : SingleClientStatusChangeChecker(service), degraded_(degraded) {}

bool TrustedVaultRecoverabilityDegradedStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting until trusted vault recoverability degraded state is "
      << degraded_;
  return service()->GetUserSettings()->IsTrustedVaultRecoverabilityDegraded() ==
         degraded_;
}
