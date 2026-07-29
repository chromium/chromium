// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_store_client.h"

#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_request_helper.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_switches.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/time_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

constexpr char kPassboxEndpointUrlBase[] = "https://passbox-pa.googleapis.com/";

// TODO(crbug.com/537160937): Consider if any other policy should be
// considered for this feature.
constexpr net::NetworkTrafficAnnotationTag kPassboxTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("remote_actor_passbox_client", R"(
      semantics {
        sender: "Remote Actor Passbox Client"
        description:
          "Uploads and deletes credentials to/from Passbox (a Google service "
          "that securely stores credentials) for sharing with remote actors."
        trigger:
          "Triggered when a user consents to share a password with an "
          "AI agent, or when the shared credential needs to be revoked."
        data:
          "All requests will contain the OAuth2 token of the primary Chrome "
          "profile. On top of that, the requests will contain:\n"
          " - Obfuscated GAIA ID of the user\n"
          " - Web origin of the target site\n"
          " - Password client tag hash identifying the credential\n"
          " - For uploads: username and password, base64 encoded JSON"
        user_data {
          type: ACCESS_TOKEN
          type: SENSITIVE_URL
          type: USERNAME
          type: CREDENTIALS
        }
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "anki-team@google.com"
          }
        }
        last_reviewed: "2026-07-21"
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature does not have a Chrome setting. It is enabled when the "
          "user consents to password sharing on Gemini Web and explicitly "
          "selects a password to share."
        chrome_policy {
          GeminiSparkSettings {
            GeminiSparkSettings: 1
          }
        }
      })");

std::string GetEndpointUrlBase() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kPassboxEndpoint)) {
    return command_line->GetSwitchValueASCII(switches::kPassboxEndpoint);
  }
  return kPassboxEndpointUrlBase;
}

}  // namespace

RemoteActorCredentialStoreClient::RemoteActorCredentialStoreClient(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RemoteActorCredentialStoreClient::~RemoteActorCredentialStoreClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RemoteActorCredentialStoreClient::UpdateCredential(
    const std::string& obfuscated_gaia_id,
    const std::string& web_origin,
    const std::string& password_client_tag_hash,
    const std::u16string& username,
    const std::u16string& password,
    base::TimeDelta ttl,
    UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::DictValue payload;
  payload.Set("signon_realm", web_origin);
  payload.Set("username", base::UTF16ToUTF8(username));
  payload.Set("password", base::UTF16ToUTF8(password));

  std::string json_payload;
  if (!base::JSONWriter::Write(payload, &json_payload)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  std::string base64_payload = base::Base64Encode(json_payload);

  std::string escaped_origin =
      base::EscapeQueryParamValue(web_origin, /*use_plus=*/false);
  std::string resource_name = base::StringPrintf(
      "internalservices/AGENTIC_CREDENTIAL_MANAGER/owneridnamespaces/"
      "GOOGLE_USER_ID/ownerids/%s/externalservices/%s/credentials/%s",
      obfuscated_gaia_id.c_str(), escaped_origin.c_str(),
      password_client_tag_hash.c_str());

  GURL url(base::StrCat(
      {GetEndpointUrlBase(), "v1/", resource_name, "?allow_missing=true"}));

  base::DictValue credential_data_dict;
  credential_data_dict.Set("data", base64_payload);

  base::Time deletion_time = base::Time::Now() + ttl;
  base::DictValue state_dict;
  state_dict.Set("scheduledDeletionTime",
                 google_apis::util::FormatTimeAsString(deletion_time));

  base::DictValue credential_dict;
  credential_dict.Set("name", resource_name);
  credential_dict.Set("credentialData", std::move(credential_data_dict));
  credential_dict.Set("state", std::move(state_dict));

  std::string post_data;
  base::JSONWriter::Write(credential_dict, &post_data);

  StartRequest(std::make_unique<RemoteActorRequest>(
      identity_manager_, url, "PATCH", post_data,
      signin::OAuthConsumerId::kRemoteActorLoginCredentialsService,
      kPassboxTrafficAnnotation, url_loader_factory_,
      base::BindOnce(&RemoteActorCredentialStoreClient::OnRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void RemoteActorCredentialStoreClient::DeleteCredential(
    const std::string& obfuscated_gaia_id,
    const std::string& web_origin,
    const std::string& password_client_tag_hash,
    DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string escaped_origin =
      base::EscapeQueryParamValue(web_origin, /*use_plus=*/false);
  std::string resource_name = base::StringPrintf(
      "internalservices/AGENTIC_CREDENTIAL_MANAGER/owneridnamespaces/"
      "GOOGLE_USER_ID/ownerids/%s/externalservices/%s/credentials/%s",
      obfuscated_gaia_id.c_str(), escaped_origin.c_str(),
      password_client_tag_hash.c_str());

  GURL url(base::StrCat(
      {GetEndpointUrlBase(), "v1/", resource_name, "?allow_missing=true"}));

  StartRequest(std::make_unique<RemoteActorRequest>(
      identity_manager_, url, "DELETE", "",
      signin::OAuthConsumerId::kRemoteActorLoginCredentialsService,
      kPassboxTrafficAnnotation, url_loader_factory_,
      base::BindOnce(&RemoteActorCredentialStoreClient::OnRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void RemoteActorCredentialStoreClient::StartRequest(
    std::unique_ptr<RemoteActorRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoteActorRequest* request_ptr = request.get();
  pending_requests_.push_back(std::move(request));
  request_ptr->Start();
}

void RemoteActorCredentialStoreClient::OnRequestCompleted(
    base::OnceCallback<void(bool)> callback,
    RemoteActorRequest* request,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&RemoteActorCredentialStoreClient::DeleteRequest,
                     weak_ptr_factory_.GetWeakPtr(), request));
  std::move(callback).Run(success);
}

void RemoteActorCredentialStoreClient::DeleteRequest(
    RemoteActorRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::erase_if(
      pending_requests_,
      [request](const std::unique_ptr<RemoteActorRequest>& pending_request) {
        return pending_request.get() == request;
      });
}

}  // namespace password_manager
