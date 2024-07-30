// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/api/tasks/chrome_tasks_delegate.h"

#include <memory>
#include <string>

#include "ash/api/tasks/tasks_client.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ash/api/tasks/tasks_client_impl.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::api {

namespace {

// TODO(b/306433892): Add policy and document it here or explain why we did not
// create a policy.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("tasks_integration",
                                        R"(
        semantics {
          sender: "Chrome tasks delegate"
          description: "Provides ChromeOS users quick access to their task "
                       "lists and tasks without opening the app or website."
          trigger: "User opens a panel in Focus Mode."
          data: "The request is authenticated with an OAuth2 access token "
                "identifying the Google account."
          internal {
            contacts {
              email: "benbecker@google.com"
            }
            contacts {
              email: "chromeos-wms@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2023-11-10"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          chrome_policy {
            ContextualGoogleIntegrationsEnabled {
              ContextualGoogleIntegrationsEnabled: false
            }
          }
        }
    )");

// Passed to the active user's client in `ChromeTasksDelegate` so that it can
// generate a `google_apis::RequestSender` instance to use whenever it is making
// a call to the Google Tasks API. The `scopes` are used for authorizing the
// RequestSender instance and the `traffic_annotation_tag` documents the network
// request for system admins and regulators. The client requests this callback
// on creation so that it can use different callbacks to create dummy auth
// services in testing situations (See
// chrome/browser/ash/api/tasks/tasks_client_impl_unittest.cc).
std::unique_ptr<google_apis::RequestSender> CreateRequestSenderForClient(
    const std::vector<std::string>& scopes,
    const net::NetworkTrafficAnnotationTag& traffic_annotation_tag) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile->GetURLLoaderFactory();

  std::unique_ptr<google_apis::AuthService> auth_service =
      std::make_unique<google_apis::AuthService>(
          identity_manager,
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          url_loader_factory, scopes);
  return std::make_unique<google_apis::RequestSender>(
      std::move(auth_service), url_loader_factory,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           /* `USER_VISIBLE` is because the requested/returned data is visible
              to the user on System UI surfaces. */
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      /*custom_user_agent=*/std::string(), traffic_annotation_tag);
}

}  // namespace

ChromeTasksDelegate::ChromeTasksDelegate() = default;

ChromeTasksDelegate::~ChromeTasksDelegate() = default;

void ChromeTasksDelegate::UpdateClientForProfileSwitch(
    const AccountId& account_id) {
  // Cleanup before switching clients.
  if (active_account_id_.is_valid()) {
    TasksClientImpl* client = GetActiveAccountClient();
    CHECK(client);
    client->InvalidateCache();
  }

  // Do not create a client for guest profiles and don't create a new client for
  // an account that has already been registered.
  if (user_manager::UserManager::IsInitialized() &&
      !user_manager::UserManager::Get()->IsLoggedInAsGuest()) {
    auto& client = clients_[account_id];
    if (!client) {
      client = std::make_unique<TasksClientImpl>(
          ProfileHelper::Get()->GetProfileByAccountId(account_id),
          base::BindRepeating(&CreateRequestSenderForClient),
          kTrafficAnnotation);
    }
  }

  active_account_id_ = account_id;
}

void ChromeTasksDelegate::GetTaskLists(
    bool force_fetch,
    TasksClient::GetTaskListsCallback callback) {
  CHECK(active_account_id_.is_valid());
  TasksClientImpl* client = GetActiveAccountClient();
  CHECK(client);
  client->GetTaskLists(force_fetch, std::move(callback));
}

void ChromeTasksDelegate::GetTasks(const std::string& task_list_id,
                                   bool force_fetch,
                                   TasksClient::GetTasksCallback callback) {
  CHECK(active_account_id_.is_valid());
  TasksClientImpl* client = GetActiveAccountClient();
  CHECK(client);
  client->GetTasks(task_list_id, force_fetch, std::move(callback));
}

void ChromeTasksDelegate::AddTask(const std::string& task_list_id,
                                  const std::string& title,
                                  TasksClient::OnTaskSavedCallback callback) {
  CHECK(active_account_id_.is_valid());
  TasksClientImpl* client = GetActiveAccountClient();
  CHECK(client);
  client->AddTask(task_list_id, title, std::move(callback));
}

void ChromeTasksDelegate::UpdateTask(
    const std::string& task_list_id,
    const std::string& task_id,
    const std::string& title,
    bool completed,
    TasksClient::OnTaskSavedCallback callback) {
  CHECK(active_account_id_.is_valid());
  TasksClientImpl* client = GetActiveAccountClient();
  CHECK(client);
  client->UpdateTask(task_list_id, task_id, title, completed,
                     std::move(callback));
}

TasksClientImpl* ChromeTasksDelegate::GetActiveAccountClient() const {
  const auto iter = clients_.find(active_account_id_);
  return iter != clients_.end() ? iter->second.get() : nullptr;
}

}  // namespace ash::api
