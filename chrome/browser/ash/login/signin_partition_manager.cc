// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin_partition_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace ash {
namespace login {
namespace {

// Generates a new unique StoragePartition name.
std::string GeneratePartitionName() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

// Clears data from the passed storage partition. `partition_data_cleared`
// will be called when all cached data has been cleared.
void ClearStoragePartition(content::StoragePartition* storage_partition,
                           base::OnceClosure partition_data_cleared) {
  storage_partition->ClearData(
      content::StoragePartition::REMOVE_DATA_MASK_ALL,
      content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      blink::StorageKey(), base::Time(), base::Time::Max(),
      std::move(partition_data_cleared));
}

network::mojom::NetworkContext* GetSystemNetworkContext() {
  return g_browser_process->system_network_context_manager()->GetContext();
}

// Copies the http auth cache proxy entries with key `cache_key` into
// `signin_storage_partition`'s NetworkContext.
void LoadHttpAuthCacheProxyEntries(
    content::StoragePartition* signin_storage_partition,
    base::OnceClosure completion_callback,
    const base::UnguessableToken& cache_key) {
  signin_storage_partition->GetNetworkContext()->LoadHttpAuthCacheProxyEntries(
      cache_key, std::move(completion_callback));
}

// Transfers http auth cache proxy entries from `main_network_context` into
// `signin_storage_partition`'s NetworkContext.
void TransferHttpAuthCacheProxyEntries(
    network::mojom::NetworkContext* main_network_context,
    content::StoragePartition* signin_storage_partition,
    base::OnceClosure completion_callback) {
  main_network_context->SaveHttpAuthCacheProxyEntries(
      base::BindOnce(&LoadHttpAuthCacheProxyEntries,
                     base::Unretained(signin_storage_partition),
                     std::move(completion_callback)));
}

}  // namespace

SigninPartitionManager::SigninPartitionManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      clear_storage_partition_task_(
          base::BindRepeating(&ClearStoragePartition)),
      get_system_network_context_task_(
          base::BindRepeating(&GetSystemNetworkContext)) {}

SigninPartitionManager::~SigninPartitionManager() = default;

void SigninPartitionManager::StartSigninSession(
    content::WebContents* embedder_web_contents,
    StartSigninSessionDoneCallback signin_session_started) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If we already were in a sign-in session, close it first.
  // This clears stale data from the last-used StoragePartition.
  CloseCurrentSigninSession(base::DoNothing());

  // The storage partition domain identifies the site embedding the webview. It
  // won't change when the webview navigates.
  storage_partition_domain_ =
      embedder_web_contents->GetLastCommittedURL().host();
  current_storage_partition_name_ = GeneratePartitionName();

  auto storage_partition_config = content::StoragePartitionConfig::Create(
      browser_context_, storage_partition_domain_,
      current_storage_partition_name_, true /*in_memory */);
  current_storage_partition_ =
      browser_context_->GetStoragePartition(storage_partition_config, true);
  if (on_create_new_storage_partition_) {
    on_create_new_storage_partition_.Run(current_storage_partition_.get());
  }

  TransferHttpAuthCacheProxyEntries(
      get_system_network_context_task_.Run(), current_storage_partition_,
      base::BindOnce(std::move(signin_session_started),
                     current_storage_partition_name_));
}

void SigninPartitionManager::CloseCurrentSigninSession(
    base::OnceClosure partition_data_cleared) {
  if (!current_storage_partition_) {
    std::move(partition_data_cleared).Run();
    return;
  }
  clear_storage_partition_task_.Run(current_storage_partition_.get(),
                                    std::move(partition_data_cleared));
  current_storage_partition_ = nullptr;
  current_storage_partition_name_.clear();
}

bool SigninPartitionManager::IsInSigninSession() const {
  return !current_storage_partition_name_.empty();
}

void SigninPartitionManager::SetClearStoragePartitionTaskForTesting(
    ClearStoragePartitionTask clear_storage_partition_task) {
  clear_storage_partition_task_ = clear_storage_partition_task;
}

void SigninPartitionManager::SetGetSystemNetworkContextForTesting(
    network::NetworkContextGetter get_system_network_context_task) {
  get_system_network_context_task_ = std::move(get_system_network_context_task);
}

void SigninPartitionManager::SetOnCreateNewStoragePartitionForTesting(
    OnCreateNewStoragePartition on_create_new_storage_partition) {
  on_create_new_storage_partition_ = on_create_new_storage_partition;
}

const std::string& SigninPartitionManager::GetCurrentStoragePartitionName()
    const {
  DCHECK(IsInSigninSession());
  return current_storage_partition_name_;
}

content::StoragePartition*
SigninPartitionManager::GetCurrentStoragePartition() {
  DCHECK(IsInSigninSession());
  return current_storage_partition_;
}

bool SigninPartitionManager::IsCurrentSigninStoragePartition(
    const content::StoragePartition* storage_partition) const {
  return IsInSigninSession() && storage_partition == current_storage_partition_;
}

SigninPartitionManager::Factory::Factory()
    : ProfileKeyedServiceFactory(
          "SigninPartitionManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

SigninPartitionManager::Factory::~Factory() = default;

// static
SigninPartitionManager* SigninPartitionManager::Factory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<SigninPartitionManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 true /* create */));
}

// static
SigninPartitionManager::Factory*
SigninPartitionManager::Factory::GetInstance() {
  return base::Singleton<SigninPartitionManager::Factory>::get();
}

std::unique_ptr<KeyedService>
SigninPartitionManager::Factory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SigninPartitionManager>(context);
}

}  // namespace login
}  // namespace ash
