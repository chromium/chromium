// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin_partition_manager.h"

#include "base/guid.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chromeos/chromeos_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "net/base/escape.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chromeos {
namespace login {

namespace {

// Generates a new unique StoragePartition name.
std::string GeneratePartitionName() {
  return base::GenerateGUID();
}

// Creates the URL for a guest site. Assumes that the StoragePartition is not
// persistent.
GURL GetGuestSiteURL(const std::string& partition_domain,
                     const std::string& partition_name) {
  return extensions::WebViewGuest::GetSiteForGuestPartitionConfig(
      partition_domain, partition_name, true /* in_memory */);
}

// Clears data from the passed storage partition. |partition_data_cleared|
// will be called when all cached data has been cleared.
void ClearStoragePartition(content::StoragePartition* storage_partition,
                           base::OnceClosure partition_data_cleared) {
  storage_partition->ClearData(
      content::StoragePartition::REMOVE_DATA_MASK_ALL,
      content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, GURL(),
      base::Time(), base::Time::Max(), std::move(partition_data_cleared));
}

net::URLRequestContextGetter* GetSystemURLRequestContextGetter() {
  return g_browser_process->system_request_context();
}

// Transfers HttpAuthCache content from |main_url_request_context_getter| into
// |signin_url_request_context_getter|.
void PrepareSigninURLRequestContextOnIOThread(
    net::URLRequestContextGetter* main_url_request_context_getter,
    net::URLRequestContextGetter* signin_url_request_context_getter) {
  // Transfer proxy auth data from the main URLRequestContext.
  net::HttpAuthCache* main_http_auth_cache =
      main_url_request_context_getter->GetURLRequestContext()
          ->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  net::HttpAuthCache* signin_http_auth_cache =
      signin_url_request_context_getter->GetURLRequestContext()
          ->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  signin_http_auth_cache->UpdateAllFrom(*main_http_auth_cache);
}

void InvokeStartSigninSessionDoneCallback(
    SigninPartitionManager::StartSigninSessionDoneCallback callback,
    const std::string& partition_name) {
  std::move(callback).Run(partition_name);
}

}  // namespace

SigninPartitionManager::SigninPartitionManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      clear_storage_partition_task_(
          base::BindRepeating(&ClearStoragePartition)),
      get_system_url_request_context_getter_task_(
          base::BindRepeating(&GetSystemURLRequestContextGetter)) {}

SigninPartitionManager::~SigninPartitionManager() {}

void SigninPartitionManager::StartSigninSession(
    const content::WebContents* embedder_web_contents,
    StartSigninSessionDoneCallback signin_session_started) {
  // If we already were in a sign-in session, close it first.
  // This clears stale data from the last-used StorageParittion.
  CloseCurrentSigninSession(base::DoNothing());

  // The storage partition domain identifies the site embedding the webview. It
  // won't change when the webview navigates.
  storage_partition_domain_ =
      embedder_web_contents->GetLastCommittedURL().host();
  current_storage_partition_name_ = GeneratePartitionName();

  GURL guest_site = GetGuestSiteURL(storage_partition_domain_,
                                    current_storage_partition_name_);

  current_storage_partition_ =
      content::BrowserContext::GetStoragePartitionForSite(browser_context_,
                                                          guest_site, true);

  base::OnceClosure invoke_callback = base::BindOnce(
      &InvokeStartSigninSessionDoneCallback, std::move(signin_session_started),
      current_storage_partition_name_);

  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &PrepareSigninURLRequestContextOnIOThread,
          base::RetainedRef(get_system_url_request_context_getter_task_.Run()),
          base::RetainedRef(
              current_storage_partition_->GetURLRequestContext())),
      std::move(invoke_callback));
}

void SigninPartitionManager::CloseCurrentSigninSession(
    base::OnceClosure partition_data_cleared) {
  if (!current_storage_partition_) {
    std::move(partition_data_cleared).Run();
    return;
  }
  clear_storage_partition_task_.Run(current_storage_partition_,
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

void SigninPartitionManager::SetGetSystemURLRequestContextGetterTaskForTesting(
    GetSystemURLRequestContextGetterTask
        get_system_url_request_context_getter_task) {
  get_system_url_request_context_getter_task_ =
      get_system_url_request_context_getter_task;
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
    : BrowserContextKeyedServiceFactory(
          "SigninPartitionManager",
          BrowserContextDependencyManager::GetInstance()) {}

SigninPartitionManager::Factory::~Factory() {}

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

KeyedService* SigninPartitionManager::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SigninPartitionManager(context);
}

content::BrowserContext*
SigninPartitionManager::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
}  // namespace login
}  // namespace chromeos
