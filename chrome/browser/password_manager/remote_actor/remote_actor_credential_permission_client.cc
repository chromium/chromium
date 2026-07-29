// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_permission_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_request_helper.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_switches.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

namespace {

constexpr char kAgenticPermissionServiceUrlBase[] =
    "https://agenticpermission.pa.googleapis.com/";

// TODO(crbug.com/537160937): Consider if any other policy should be
// considered for this feature.
constexpr net::NetworkTrafficAnnotationTag kRemotePermissionTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("remote_actor_permission_service",
                                        R"(
      semantics {
        sender: "Remote Actor Permission Service"
        description:
          "Communicates with Agentic permission service to grant permissions "
          "for password sharing with agents."
        trigger:
          "Triggered when a user consents to share a password with an agent."
        data:
          "All requests will contain OAuth2 token of the primary Chrome "
          "profile. On top of that, the requests will contain:\n"
          " - Agent OAuth client ID\n"
          " - Web origin of the target site\n"
          " - Boolean indicating if all affiliated passwords permission is "
          "granted\n"
        user_data {
          type: ACCESS_TOKEN
          type: SENSITIVE_URL
        }
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "anki-team@google.com"
          }
        }
        last_reviewed: "2026-07-17"
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

std::string CreateGrantPasswordRequestBody(
    const RemoteActorCredentialPermissionClient::PasswordPermission&
        permission) {
  base::DictValue agent_dict;
  agent_dict.Set("type", "AGENT_TYPE_PERSONAL_ASSISTANT");
  agent_dict.Set("agentOauthClientId", permission.agent_oauth_client_id);

  // TODO(crbug.com/532483845): Use passwordClientTagHash to grant permission
  // for a specific credential instead of all affiliated passwords.
  base::DictValue saved_password_access_dict;
  saved_password_access_dict.Set("webOrigin", permission.web_origin);
  saved_password_access_dict.Set("allAffiliatedPasswords", true);

  base::ListValue saved_password_access_list;
  saved_password_access_list.Append(std::move(saved_password_access_dict));

  base::DictValue saved_passwords_dict;
  saved_passwords_dict.Set("savedPasswordAccess",
                           std::move(saved_password_access_list));

  base::DictValue request_dict;
  request_dict.Set("agent", std::move(agent_dict));
  request_dict.Set("savedPasswords", std::move(saved_passwords_dict));

  std::string post_data;
  base::JSONWriter::Write(request_dict, &post_data);
  return post_data;
}

std::string GetEndpointUrlBase() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAgentPermissionServiceEndpoint)) {
    return command_line->GetSwitchValueASCII(
        switches::kAgentPermissionServiceEndpoint);
  }
  return kAgenticPermissionServiceUrlBase;
}

}  // namespace

RemoteActorCredentialPermissionClient::RemoteActorCredentialPermissionClient(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RemoteActorCredentialPermissionClient::
    ~RemoteActorCredentialPermissionClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RemoteActorCredentialPermissionClient::GrantPasswordPermission(
    const PasswordPermission& permission,
    GrantPasswordPermissionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url::Origin origin = url::Origin::Create(GURL(permission.web_origin));
  if (permission.agent_oauth_client_id.empty() || origin.opaque()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  GURL url(base::StrCat(
      {GetEndpointUrlBase(), "v1/permissions:update?allow_missing=true"}));
  std::string post_data = CreateGrantPasswordRequestBody(permission);

  StartRequest(std::make_unique<RemoteActorRequest>(
      identity_manager_, url, "POST", post_data,
      signin::OAuthConsumerId::kActorLoginPermissionService,
      kRemotePermissionTrafficAnnotation, url_loader_factory_,
      base::BindOnce(&RemoteActorCredentialPermissionClient::OnRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void RemoteActorCredentialPermissionClient::StartRequest(
    std::unique_ptr<RemoteActorRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoteActorRequest* request_ptr = request.get();
  pending_requests_.push_back(std::move(request));
  request_ptr->Start();
}

void RemoteActorCredentialPermissionClient::OnRequestCompleted(
    base::OnceCallback<void(bool)> callback,
    RemoteActorRequest* request,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&RemoteActorCredentialPermissionClient::DeleteRequest,
                     weak_ptr_factory_.GetWeakPtr(), request));
  std::move(callback).Run(success);
}

void RemoteActorCredentialPermissionClient::DeleteRequest(
    RemoteActorRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::erase_if(
      pending_requests_,
      [request](const std::unique_ptr<RemoteActorRequest>& pending_request) {
        return pending_request.get() == request;
      });
}

}  // namespace password_manager
