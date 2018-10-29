// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/cookies_tree_model.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <map>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_channel_id_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "extensions/common/extension_set.h"
#endif

namespace {

struct NodeTitleComparator {
  bool operator()(const std::unique_ptr<CookieTreeNode>& lhs,
                  const std::unique_ptr<CookieTreeNode>& rhs) {
    return lhs->GetTitle() < rhs->GetTitle();
  }
};

// Comparison functor, for use in CookieTreeRootNode.
struct HostNodeComparator {
  bool operator()(const std::unique_ptr<CookieTreeNode>& lhs,
                  const std::unique_ptr<CookieTreeHostNode>& rhs) {
    // This comparator is only meant to compare CookieTreeHostNode types. Make
    // sure we check this, as the static cast below is dangerous if we get the
    // wrong object type.
    CHECK_EQ(CookieTreeNode::DetailedInfo::TYPE_HOST,
             lhs->GetDetailedInfo().node_type);
    CHECK_EQ(CookieTreeNode::DetailedInfo::TYPE_HOST,
             rhs->GetDetailedInfo().node_type);

    const CookieTreeHostNode* ltn =
        static_cast<const CookieTreeHostNode*>(lhs.get());
    const CookieTreeHostNode* rtn = rhs.get();

    // We want to order by registry controlled domain, so we would get
    // google.com, ad.google.com, www.google.com,
    // microsoft.com, ad.microsoft.com. CanonicalizeHost transforms the origins
    // into a form like google.com.www so that string comparisons work.
    return ltn->canonicalized_host() < rtn->canonicalized_host();
  }
};

std::string CanonicalizeHost(const GURL& url) {
  // The canonicalized representation makes the registry controlled domain come
  // first, and then adds subdomains in reverse order, e.g.  1.mail.google.com
  // would become google.com.mail.1, and then a standard string comparison works
  // to order hosts by registry controlled domain first. Leading dots are
  // ignored, ".google.com" is the same as "google.com".
  if (url.SchemeIsFile()) {
    return std::string(url::kFileScheme) + url::kStandardSchemeSeparator;
  }

  std::string host = url.host();
  std::string retval =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (!retval.length())  // Is an IP address or other special origin.
    return host;

  std::string::size_type position = host.rfind(retval);

  // The host may be the registry controlled domain, in which case fail fast.
  if (position == 0 || position == std::string::npos)
    return host;

  // If host is www.google.com, retval will contain google.com at this point.
  // Start operating to the left of the registry controlled domain, e.g. in
  // the www.google.com example, start at index 3.
  --position;

  // If position == 0, that means it's a dot; this will be ignored to treat
  // ".google.com" the same as "google.com".
  while (position > 0) {
    retval += std::string(".");
    // Copy up to the next dot. host[position] is a dot so start after it.
    std::string::size_type next_dot = host.rfind(".", position - 1);
    if (next_dot == std::string::npos) {
      retval += host.substr(0, position);
      break;
    }
    retval += host.substr(next_dot + 1, position - (next_dot + 1));
    position = next_dot;
  }
  return retval;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
bool TypeIsProtected(CookieTreeNode::DetailedInfo::NodeType type) {
  switch (type) {
    // Fall through each below cases to return true.
    case CookieTreeNode::DetailedInfo::TYPE_DATABASE:
    case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE:
    case CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE:
    case CookieTreeNode::DetailedInfo::TYPE_APPCACHE:
    case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB:
    case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM:
    case CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER:
    case CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKER:
    case CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE:
      return true;

    // Fall through each below cases to return false.
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
    case CookieTreeNode::DetailedInfo::TYPE_QUOTA:
    case CookieTreeNode::DetailedInfo::TYPE_CHANNEL_ID:
    case CookieTreeNode::DetailedInfo::TYPE_FLASH_LSO:
    case CookieTreeNode::DetailedInfo::TYPE_MEDIA_LICENSE:
      return false;
    default:
      break;
  }
  return false;
}
#endif

// This function returns the local data container associated with a leaf tree
// node. The app node is assumed to be 3 levels above the leaf because of the
// following structure:
//   root -> origin -> storage type -> leaf node
LocalDataContainer* GetLocalDataContainerForNode(CookieTreeNode* node) {
  CookieTreeHostNode* host = static_cast<CookieTreeHostNode*>(
      node->parent()->parent());
  CHECK_EQ(host->GetDetailedInfo().node_type,
           CookieTreeNode::DetailedInfo::TYPE_HOST);
  return node->GetModel()->data_container();
}

}  // namespace

CookieTreeNode::DetailedInfo::DetailedInfo() : node_type(TYPE_NONE) {}

CookieTreeNode::DetailedInfo::DetailedInfo(const DetailedInfo& other) = default;

CookieTreeNode::DetailedInfo::~DetailedInfo() {}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::Init(
    NodeType type) {
  DCHECK_EQ(TYPE_NONE, node_type);
  node_type = type;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitHost() {
  Init(TYPE_HOST);
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitCookie(
    const net::CanonicalCookie* cookie) {
  Init(TYPE_COOKIE);
  this->cookie = cookie;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitDatabase(
    const BrowsingDataDatabaseHelper::DatabaseInfo* database_info) {
  Init(TYPE_DATABASE);
  this->database_info = database_info;
  origin = database_info->identifier.ToOrigin();
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitLocalStorage(
    const BrowsingDataLocalStorageHelper::LocalStorageInfo*
    local_storage_info) {
  Init(TYPE_LOCAL_STORAGE);
  this->local_storage_info = local_storage_info;
  origin = local_storage_info->origin_url;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitSessionStorage(
    const BrowsingDataLocalStorageHelper::LocalStorageInfo*
    session_storage_info) {
  Init(TYPE_SESSION_STORAGE);
  this->session_storage_info = session_storage_info;
  origin = session_storage_info->origin_url;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitAppCache(
    const GURL& origin,
    const content::AppCacheInfo* appcache_info) {
  Init(TYPE_APPCACHE);
  this->appcache_info = appcache_info;
  this->origin = origin;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitIndexedDB(
    const content::IndexedDBInfo* indexed_db_info) {
  Init(TYPE_INDEXED_DB);
  this->indexed_db_info = indexed_db_info;
  this->origin = indexed_db_info->origin;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitFileSystem(
    const BrowsingDataFileSystemHelper::FileSystemInfo* file_system_info) {
  Init(TYPE_FILE_SYSTEM);
  this->file_system_info = file_system_info;
  this->origin = file_system_info->origin;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitQuota(
    const BrowsingDataQuotaHelper::QuotaInfo* quota_info) {
  Init(TYPE_QUOTA);
  this->quota_info = quota_info;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitChannelID(
    const net::ChannelIDStore::ChannelID* channel_id) {
  Init(TYPE_CHANNEL_ID);
  this->channel_id = channel_id;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitServiceWorker(
    const content::ServiceWorkerUsageInfo* service_worker_info) {
  Init(TYPE_SERVICE_WORKER);
  this->service_worker_info = service_worker_info;
  this->origin = service_worker_info->origin;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitSharedWorker(
    const BrowsingDataSharedWorkerHelper::SharedWorkerInfo*
        shared_worker_info) {
  Init(TYPE_SHARED_WORKER);
  this->shared_worker_info = shared_worker_info;
  this->origin = shared_worker_info->worker.GetOrigin();
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitCacheStorage(
    const content::CacheStorageUsageInfo* cache_storage_info) {
  Init(TYPE_CACHE_STORAGE);
  this->cache_storage_info = cache_storage_info;
  this->origin = cache_storage_info->origin;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitFlashLSO(
    const std::string& flash_lso_domain) {
  Init(TYPE_FLASH_LSO);
  this->flash_lso_domain = flash_lso_domain;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitMediaLicense(
    const BrowsingDataMediaLicenseHelper::MediaLicenseInfo*
        media_license_info) {
  Init(TYPE_MEDIA_LICENSE);
  this->media_license_info = media_license_info;
  this->origin = media_license_info->origin;
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeNode, public:

void CookieTreeNode::DeleteStoredObjects() {
  for (const auto& child : children())
    child->DeleteStoredObjects();
}

CookiesTreeModel* CookieTreeNode::GetModel() const {
  if (parent())
    return parent()->GetModel();
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookieNode, public:

CookieTreeCookieNode::CookieTreeCookieNode(
    std::list<net::CanonicalCookie>::iterator cookie)
    : CookieTreeNode(base::UTF8ToUTF16(cookie->Name())),
      cookie_(cookie) {
}

CookieTreeCookieNode::~CookieTreeCookieNode() {}

void CookieTreeCookieNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);
  container->cookie_helper_->DeleteCookie(*cookie_);
  container->cookie_list_.erase(cookie_);
}

CookieTreeNode::DetailedInfo CookieTreeCookieNode::GetDetailedInfo() const {
  return DetailedInfo().InitCookie(&*cookie_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeAppCacheNode, public:

CookieTreeAppCacheNode::CookieTreeAppCacheNode(
    const url::Origin& origin,
    std::list<content::AppCacheInfo>::iterator appcache_info)
    : CookieTreeNode(base::UTF8ToUTF16(appcache_info->manifest_url.spec())),
      origin_(origin),
      appcache_info_(appcache_info) {}

CookieTreeAppCacheNode::~CookieTreeAppCacheNode() {
}

void CookieTreeAppCacheNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    DCHECK(container->appcache_helper_.get());
    container->appcache_helper_
        ->DeleteAppCacheGroup(appcache_info_->manifest_url);
    container->appcache_info_[origin_].erase(appcache_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeAppCacheNode::GetDetailedInfo() const {
  return DetailedInfo().InitAppCache(origin_.GetURL(), &*appcache_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeDatabaseNode, public:

CookieTreeDatabaseNode::CookieTreeDatabaseNode(
    std::list<BrowsingDataDatabaseHelper::DatabaseInfo>::iterator database_info)
    : CookieTreeNode(database_info->database_name.empty() ?
          l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASE_UNNAMED_NAME) :
          base::UTF8ToUTF16(database_info->database_name)),
      database_info_(database_info) {
}

CookieTreeDatabaseNode::~CookieTreeDatabaseNode() {}

void CookieTreeDatabaseNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->database_helper_->DeleteDatabase(
        database_info_->identifier.ToString(), database_info_->database_name);
    container->database_info_list_.erase(database_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeDatabaseNode::GetDetailedInfo() const {
  return DetailedInfo().InitDatabase(&*database_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeLocalStorageNode, public:

CookieTreeLocalStorageNode::CookieTreeLocalStorageNode(
    std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::iterator
        local_storage_info)
    : CookieTreeNode(base::UTF8ToUTF16(local_storage_info->origin_url.spec())),
      local_storage_info_(local_storage_info) {
}

CookieTreeLocalStorageNode::~CookieTreeLocalStorageNode() {}

void CookieTreeLocalStorageNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->local_storage_helper_->DeleteOrigin(
        local_storage_info_->origin_url, base::DoNothing());
    container->local_storage_info_list_.erase(local_storage_info_);
  }
}

CookieTreeNode::DetailedInfo
CookieTreeLocalStorageNode::GetDetailedInfo() const {
  return DetailedInfo().InitLocalStorage(
      &*local_storage_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSessionStorageNode, public:

CookieTreeSessionStorageNode::CookieTreeSessionStorageNode(
    std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::iterator
        session_storage_info)
    : CookieTreeNode(
          base::UTF8ToUTF16(session_storage_info->origin_url.spec())),
      session_storage_info_(session_storage_info) {
}

CookieTreeSessionStorageNode::~CookieTreeSessionStorageNode() {}

void CookieTreeSessionStorageNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    // TODO(rsesek): There's no easy way to get the namespace_id for a session
    // storage, nor is there an easy way to clear session storage just by
    // origin. This is probably okay since session storage is not persistent.
    // http://crbug.com/168996
    container->session_storage_info_list_.erase(session_storage_info_);
  }
}

CookieTreeNode::DetailedInfo
CookieTreeSessionStorageNode::GetDetailedInfo() const {
  return DetailedInfo().InitSessionStorage(&*session_storage_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeIndexedDBNode, public:

CookieTreeIndexedDBNode::CookieTreeIndexedDBNode(
    std::list<content::IndexedDBInfo>::iterator indexed_db_info)
    : CookieTreeNode(base::UTF8ToUTF16(indexed_db_info->origin.spec())),
      indexed_db_info_(indexed_db_info) {}

CookieTreeIndexedDBNode::~CookieTreeIndexedDBNode() {}

void CookieTreeIndexedDBNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->indexed_db_helper_->DeleteIndexedDB(indexed_db_info_->origin);
    container->indexed_db_info_list_.erase(indexed_db_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeIndexedDBNode::GetDetailedInfo() const {
  return DetailedInfo().InitIndexedDB(&*indexed_db_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeFileSystemNode, public:

CookieTreeFileSystemNode::CookieTreeFileSystemNode(
    std::list<BrowsingDataFileSystemHelper::FileSystemInfo>::iterator
        file_system_info)
    : CookieTreeNode(base::UTF8ToUTF16(
          file_system_info->origin.spec())),
      file_system_info_(file_system_info) {
}

CookieTreeFileSystemNode::~CookieTreeFileSystemNode() {}

void CookieTreeFileSystemNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->file_system_helper_->DeleteFileSystemOrigin(
        file_system_info_->origin);
    container->file_system_info_list_.erase(file_system_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeFileSystemNode::GetDetailedInfo() const {
  return DetailedInfo().InitFileSystem(&*file_system_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeQuotaNode, public:

CookieTreeQuotaNode::CookieTreeQuotaNode(
    std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info)
    : CookieTreeNode(base::UTF8ToUTF16(quota_info->host)),
      quota_info_(quota_info) {
}

CookieTreeQuotaNode::~CookieTreeQuotaNode() {}

void CookieTreeQuotaNode::DeleteStoredObjects() {
  // Calling this function may cause unexpected over-quota state of origin.
  // However, it'll caused no problem, just prevent usage growth of the origin.
  LocalDataContainer* container = GetModel()->data_container();

  if (container) {
    container->quota_helper_->RevokeHostQuota(quota_info_->host);
    container->quota_info_list_.erase(quota_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeQuotaNode::GetDetailedInfo() const {
  return DetailedInfo().InitQuota(&*quota_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeChannelIDNode, public:

CookieTreeChannelIDNode::CookieTreeChannelIDNode(
      net::ChannelIDStore::ChannelIDList::iterator channel_id)
    : CookieTreeNode(base::ASCIIToUTF16(channel_id->server_identifier())),
      channel_id_(channel_id) {
}

CookieTreeChannelIDNode::~CookieTreeChannelIDNode() {}

void CookieTreeChannelIDNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->channel_id_helper_->DeleteChannelID(
        channel_id_->server_identifier());
    container->channel_id_list_.erase(channel_id_);
  }
}

CookieTreeNode::DetailedInfo
CookieTreeChannelIDNode::GetDetailedInfo() const {
  return DetailedInfo().InitChannelID(&*channel_id_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeServiceWorkerNode, public:

CookieTreeServiceWorkerNode::CookieTreeServiceWorkerNode(
    std::list<content::ServiceWorkerUsageInfo>::iterator service_worker_info)
    : CookieTreeNode(base::UTF8ToUTF16(service_worker_info->origin.spec())),
      service_worker_info_(service_worker_info) {
}

CookieTreeServiceWorkerNode::~CookieTreeServiceWorkerNode() {
}

void CookieTreeServiceWorkerNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->service_worker_helper_->DeleteServiceWorkers(
        service_worker_info_->origin);
    container->service_worker_info_list_.erase(service_worker_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeServiceWorkerNode::GetDetailedInfo()
    const {
  return DetailedInfo().InitServiceWorker(&*service_worker_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSharedWorkerNode, public:

CookieTreeSharedWorkerNode::CookieTreeSharedWorkerNode(
    std::list<BrowsingDataSharedWorkerHelper::SharedWorkerInfo>::iterator
        shared_worker_info)
    : CookieTreeNode(base::UTF8ToUTF16(shared_worker_info->worker.spec())),
      shared_worker_info_(shared_worker_info) {}

CookieTreeSharedWorkerNode::~CookieTreeSharedWorkerNode() {}

void CookieTreeSharedWorkerNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->shared_worker_helper_->DeleteSharedWorker(
        shared_worker_info_->worker, shared_worker_info_->name,
        shared_worker_info_->constructor_origin);
    container->shared_worker_info_list_.erase(shared_worker_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeSharedWorkerNode::GetDetailedInfo()
    const {
  return DetailedInfo().InitSharedWorker(&*shared_worker_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCacheStorageNode, public:

CookieTreeCacheStorageNode::CookieTreeCacheStorageNode(
    std::list<content::CacheStorageUsageInfo>::iterator cache_storage_info)
    : CookieTreeNode(base::UTF8ToUTF16(cache_storage_info->origin.spec())),
      cache_storage_info_(cache_storage_info) {}

CookieTreeCacheStorageNode::~CookieTreeCacheStorageNode() {}

void CookieTreeCacheStorageNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->cache_storage_helper_->DeleteCacheStorage(
        cache_storage_info_->origin);
    container->cache_storage_info_list_.erase(cache_storage_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeCacheStorageNode::GetDetailedInfo()
    const {
  return DetailedInfo().InitCacheStorage(&*cache_storage_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeMediaLicenseNode, public:

CookieTreeMediaLicenseNode::CookieTreeMediaLicenseNode(
    const std::list<BrowsingDataMediaLicenseHelper::MediaLicenseInfo>::iterator
        media_license_info)
    : CookieTreeNode(base::UTF8ToUTF16(media_license_info->origin.spec())),
      media_license_info_(media_license_info) {}

CookieTreeMediaLicenseNode::~CookieTreeMediaLicenseNode() {}

void CookieTreeMediaLicenseNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->media_license_helper_->DeleteMediaLicenseOrigin(
        media_license_info_->origin);
    container->media_license_info_list_.erase(media_license_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeMediaLicenseNode::GetDetailedInfo()
    const {
  return DetailedInfo().InitMediaLicense(&*media_license_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeRootNode, public:

CookieTreeRootNode::CookieTreeRootNode(CookiesTreeModel* model)
    : model_(model) {
}

CookieTreeRootNode::~CookieTreeRootNode() {}

CookieTreeHostNode* CookieTreeRootNode::GetOrCreateHostNode(const GURL& url) {
  std::unique_ptr<CookieTreeHostNode> host_node =
      std::make_unique<CookieTreeHostNode>(url);

  // First see if there is an existing match.
  auto host_node_iterator = std::lower_bound(
      children().begin(), children().end(), host_node, HostNodeComparator());
  if (host_node_iterator != children().end() &&
      CookieTreeHostNode::TitleForUrl(url) ==
      (*host_node_iterator)->GetTitle())
    return static_cast<CookieTreeHostNode*>(host_node_iterator->get());
  // Node doesn't exist, insert the new one into the (ordered) children.
  DCHECK(model_);
  return static_cast<CookieTreeHostNode*>(model_->Add(
      this, std::move(host_node), (host_node_iterator - children().begin())));
}

CookiesTreeModel* CookieTreeRootNode::GetModel() const {
  return model_;
}

CookieTreeNode::DetailedInfo CookieTreeRootNode::GetDetailedInfo() const {
  return DetailedInfo().Init(DetailedInfo::TYPE_ROOT);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeHostNode, public:

// static
base::string16 CookieTreeHostNode::TitleForUrl(const GURL& url) {
  const std::string file_origin_node_name(
      std::string(url::kFileScheme) + url::kStandardSchemeSeparator);
  return base::UTF8ToUTF16(url.SchemeIsFile()
                               ? file_origin_node_name
                               : url::Origin::Create(url).host());
}

CookieTreeHostNode::CookieTreeHostNode(const GURL& url)
    : CookieTreeNode(TitleForUrl(url)),
      url_(url),
      canonicalized_host_(CanonicalizeHost(url)) {}

CookieTreeHostNode::~CookieTreeHostNode() {}

std::string CookieTreeHostNode::GetHost() const {
  const std::string file_origin_node_name(
      std::string(url::kFileScheme) + url::kStandardSchemeSeparator);
  return url_.SchemeIsFile() ? file_origin_node_name : url_.host();
}

CookieTreeNode::DetailedInfo CookieTreeHostNode::GetDetailedInfo() const {
  return DetailedInfo().InitHost();
}

CookieTreeCookiesNode* CookieTreeHostNode::GetOrCreateCookiesNode() {
  if (cookies_child_)
    return cookies_child_;
  cookies_child_ = new CookieTreeCookiesNode;
  AddChildSortedByTitle(base::WrapUnique(cookies_child_));
  return cookies_child_;
}

CookieTreeDatabasesNode* CookieTreeHostNode::GetOrCreateDatabasesNode() {
  if (databases_child_)
    return databases_child_;
  databases_child_ = new CookieTreeDatabasesNode;
  AddChildSortedByTitle(base::WrapUnique(databases_child_));
  return databases_child_;
}

CookieTreeLocalStoragesNode*
    CookieTreeHostNode::GetOrCreateLocalStoragesNode() {
  if (local_storages_child_)
    return local_storages_child_;
  local_storages_child_ = new CookieTreeLocalStoragesNode;
  AddChildSortedByTitle(base::WrapUnique(local_storages_child_));
  return local_storages_child_;
}

CookieTreeSessionStoragesNode*
    CookieTreeHostNode::GetOrCreateSessionStoragesNode() {
  if (session_storages_child_)
    return session_storages_child_;
  session_storages_child_ = new CookieTreeSessionStoragesNode;
  AddChildSortedByTitle(base::WrapUnique(session_storages_child_));
  return session_storages_child_;
}

CookieTreeAppCachesNode* CookieTreeHostNode::GetOrCreateAppCachesNode() {
  if (appcaches_child_)
    return appcaches_child_;
  appcaches_child_ = new CookieTreeAppCachesNode;
  AddChildSortedByTitle(base::WrapUnique(appcaches_child_));
  return appcaches_child_;
}

CookieTreeIndexedDBsNode* CookieTreeHostNode::GetOrCreateIndexedDBsNode() {
  if (indexed_dbs_child_)
    return indexed_dbs_child_;
  indexed_dbs_child_ = new CookieTreeIndexedDBsNode;
  AddChildSortedByTitle(base::WrapUnique(indexed_dbs_child_));
  return indexed_dbs_child_;
}

CookieTreeFileSystemsNode* CookieTreeHostNode::GetOrCreateFileSystemsNode() {
  if (file_systems_child_)
    return file_systems_child_;
  file_systems_child_ = new CookieTreeFileSystemsNode;
  AddChildSortedByTitle(base::WrapUnique(file_systems_child_));
  return file_systems_child_;
}

CookieTreeQuotaNode* CookieTreeHostNode::UpdateOrCreateQuotaNode(
    std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info) {
  if (quota_child_)
    return quota_child_;
  quota_child_ = new CookieTreeQuotaNode(quota_info);
  AddChildSortedByTitle(base::WrapUnique(quota_child_));
  return quota_child_;
}

CookieTreeChannelIDsNode*
CookieTreeHostNode::GetOrCreateChannelIDsNode() {
  if (channel_ids_child_)
    return channel_ids_child_;
  channel_ids_child_ = new CookieTreeChannelIDsNode;
  AddChildSortedByTitle(base::WrapUnique(channel_ids_child_));
  return channel_ids_child_;
}

CookieTreeServiceWorkersNode*
CookieTreeHostNode::GetOrCreateServiceWorkersNode() {
  if (service_workers_child_)
    return service_workers_child_;
  service_workers_child_ = new CookieTreeServiceWorkersNode;
  AddChildSortedByTitle(base::WrapUnique(service_workers_child_));
  return service_workers_child_;
}

CookieTreeSharedWorkersNode*
CookieTreeHostNode::GetOrCreateSharedWorkersNode() {
  if (shared_workers_child_)
    return shared_workers_child_;
  shared_workers_child_ = new CookieTreeSharedWorkersNode;
  AddChildSortedByTitle(base::WrapUnique(shared_workers_child_));
  return shared_workers_child_;
}

CookieTreeCacheStoragesNode*
CookieTreeHostNode::GetOrCreateCacheStoragesNode() {
  if (cache_storages_child_)
    return cache_storages_child_;
  cache_storages_child_ = new CookieTreeCacheStoragesNode;
  AddChildSortedByTitle(base::WrapUnique(cache_storages_child_));
  return cache_storages_child_;
}

CookieTreeFlashLSONode* CookieTreeHostNode::GetOrCreateFlashLSONode(
    const std::string& domain) {
  DCHECK_EQ(GetHost(), domain);
  if (flash_lso_child_)
    return flash_lso_child_;
  flash_lso_child_ = new CookieTreeFlashLSONode(domain);
  AddChildSortedByTitle(base::WrapUnique(flash_lso_child_));
  return flash_lso_child_;
}

CookieTreeMediaLicensesNode*
CookieTreeHostNode::GetOrCreateMediaLicensesNode() {
  if (media_licenses_child_)
    return media_licenses_child_;
  media_licenses_child_ = new CookieTreeMediaLicensesNode();
  AddChildSortedByTitle(base::WrapUnique(media_licenses_child_));
  return media_licenses_child_;
}

void CookieTreeHostNode::CreateContentException(
    content_settings::CookieSettings* cookie_settings,
    ContentSetting setting) const {
  DCHECK(setting == CONTENT_SETTING_ALLOW ||
         setting == CONTENT_SETTING_BLOCK ||
         setting == CONTENT_SETTING_SESSION_ONLY);
  if (CanCreateContentException()) {
    cookie_settings->ResetCookieSetting(url_);
    cookie_settings->SetCookieSetting(url_, setting);
  }
}

bool CookieTreeHostNode::CanCreateContentException() const {
  return !url_.SchemeIsFile();
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookiesNode, public:

CookieTreeCookiesNode::CookieTreeCookiesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_COOKIES)) {
}

CookieTreeCookiesNode::~CookieTreeCookiesNode() {
}

CookieTreeNode::DetailedInfo CookieTreeCookiesNode::GetDetailedInfo() const {
  return DetailedInfo().Init(DetailedInfo::TYPE_COOKIES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeAppCachesNode, public:

CookieTreeAppCachesNode::CookieTreeAppCachesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(
                         IDS_COOKIES_APPLICATION_CACHES)) {
}

CookieTreeAppCachesNode::~CookieTreeAppCachesNode() {}

CookieTreeNode::DetailedInfo CookieTreeAppCachesNode::GetDetailedInfo() const {
  return DetailedInfo().Init(DetailedInfo::TYPE_APPCACHES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeDatabasesNode, public:

CookieTreeDatabasesNode::CookieTreeDatabasesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASES)) {
}

CookieTreeDatabasesNode::~CookieTreeDatabasesNode() {}

CookieTreeNode::DetailedInfo CookieTreeDatabasesNode::GetDetailedInfo() const {
  return DetailedInfo().Init(DetailedInfo::TYPE_DATABASES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeLocalStoragesNode, public:

CookieTreeLocalStoragesNode::CookieTreeLocalStoragesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE)) {
}

CookieTreeLocalStoragesNode::~CookieTreeLocalStoragesNode() {}

CookieTreeNode::DetailedInfo
CookieTreeLocalStoragesNode::GetDetailedInfo() const {
  return DetailedInfo().Init(DetailedInfo::TYPE_LOCAL_STORAGES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSessionStoragesNode, public:

CookieTreeSessionStoragesNode::CookieTreeSessionStoragesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_SESSION_STORAGE)) {
}

CookieTreeSessionStoragesNode::~CookieTreeSessionStoragesNode() {}

CookieTreeNode::DetailedInfo
CookieTreeSessionStoragesNode::GetDetailedInfo() const {
  return DetailedInfo().Init(DetailedInfo::TYPE_SESSION_STORAGES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeIndexedDBsNode, public:

CookieTreeIndexedDBsNode::CookieTreeIndexedDBsNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_INDEXED_DBS)) {
}

CookieTreeIndexedDBsNode::~CookieTreeIndexedDBsNode() {}

CookieTreeNode::DetailedInfo
CookieTreeIndexedDBsNode::GetDetailedInfo() const {
  return DetailedInfo().Init(DetailedInfo::TYPE_INDEXED_DBS);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeFileSystemsNode, public:

CookieTreeFileSystemsNode::CookieTreeFileSystemsNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_FILE_SYSTEMS)) {
}

CookieTreeFileSystemsNode::~CookieTreeFileSystemsNode() {}

CookieTreeNode::DetailedInfo
CookieTreeFileSystemsNode::GetDetailedInfo() const {
  return DetailedInfo().Init(DetailedInfo::TYPE_FILE_SYSTEMS);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeChannelIDsNode, public:

CookieTreeChannelIDsNode::CookieTreeChannelIDsNode()
    : CookieTreeNode(
        l10n_util::GetStringUTF16(IDS_COOKIES_CHANNEL_IDS)) {
}

CookieTreeChannelIDsNode::~CookieTreeChannelIDsNode() {}

CookieTreeNode::DetailedInfo
CookieTreeChannelIDsNode::GetDetailedInfo() const {
  return DetailedInfo().Init(DetailedInfo::TYPE_CHANNEL_IDS);
}

void CookieTreeNode::AddChildSortedByTitle(
    std::unique_ptr<CookieTreeNode> new_child) {
  DCHECK(new_child);
  auto iter = std::lower_bound(children().begin(), children().end(), new_child,
                               NodeTitleComparator());
  GetModel()->Add(this, std::move(new_child), iter - children().begin());
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeServiceWorkersNode, public:

CookieTreeServiceWorkersNode::CookieTreeServiceWorkersNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_SERVICE_WORKERS)) {
}

CookieTreeServiceWorkersNode::~CookieTreeServiceWorkersNode() {
}

CookieTreeNode::DetailedInfo CookieTreeServiceWorkersNode::GetDetailedInfo()
    const {
  return DetailedInfo().Init(DetailedInfo::TYPE_SERVICE_WORKERS);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSharedWorkersNode, public:

CookieTreeSharedWorkersNode::CookieTreeSharedWorkersNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_SHARED_WORKERS)) {}

CookieTreeSharedWorkersNode::~CookieTreeSharedWorkersNode() {}

CookieTreeNode::DetailedInfo CookieTreeSharedWorkersNode::GetDetailedInfo()
    const {
  return DetailedInfo().Init(DetailedInfo::TYPE_SHARED_WORKERS);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCacheStoragesNode, public:

CookieTreeCacheStoragesNode::CookieTreeCacheStoragesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_CACHE_STORAGE)) {}

CookieTreeCacheStoragesNode::~CookieTreeCacheStoragesNode() {}

CookieTreeNode::DetailedInfo CookieTreeCacheStoragesNode::GetDetailedInfo()
    const {
  return DetailedInfo().Init(DetailedInfo::TYPE_CACHE_STORAGES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeFlashLSONode
CookieTreeFlashLSONode::CookieTreeFlashLSONode(
    const std::string& domain)
    : domain_(domain) {}
CookieTreeFlashLSONode::~CookieTreeFlashLSONode() {}

void CookieTreeFlashLSONode::DeleteStoredObjects() {
  // We are one level below the host node.
  CookieTreeHostNode* host = static_cast<CookieTreeHostNode*>(parent());
  CHECK_EQ(host->GetDetailedInfo().node_type,
           CookieTreeNode::DetailedInfo::TYPE_HOST);
  LocalDataContainer* container = GetModel()->data_container();
  container->flash_lso_helper_->DeleteFlashLSOsForSite(
      domain_, base::Closure());
  auto entry = std::find(container->flash_lso_domain_list_.begin(),
                         container->flash_lso_domain_list_.end(), domain_);
  container->flash_lso_domain_list_.erase(entry);
}

CookieTreeNode::DetailedInfo CookieTreeFlashLSONode::GetDetailedInfo() const {
  return DetailedInfo().InitFlashLSO(domain_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeMediaLicensesNode
CookieTreeMediaLicensesNode::CookieTreeMediaLicensesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_MEDIA_LICENSES)) {}

CookieTreeMediaLicensesNode::~CookieTreeMediaLicensesNode() {}

CookieTreeNode::DetailedInfo CookieTreeMediaLicensesNode::GetDetailedInfo()
    const {
  return DetailedInfo().Init(DetailedInfo::TYPE_MEDIA_LICENSES);
}

///////////////////////////////////////////////////////////////////////////////
// ScopedBatchUpdateNotifier
CookiesTreeModel::ScopedBatchUpdateNotifier::ScopedBatchUpdateNotifier(
    CookiesTreeModel* model,
    CookieTreeNode* node)
    : model_(model), node_(node) {
  model_->RecordBatchSeen();
}

CookiesTreeModel::ScopedBatchUpdateNotifier::~ScopedBatchUpdateNotifier() {
  if (batch_in_progress_) {
    model_->NotifyObserverTreeNodeChanged(node_);
    model_->NotifyObserverEndBatch();
  } else {
    // If no batch started, and this is the last batch, give the model a chance
    // to send out a final notification.
    model_->MaybeNotifyBatchesEnded();
  }
}

void CookiesTreeModel::ScopedBatchUpdateNotifier::StartBatchUpdate() {
  if (!batch_in_progress_) {
    model_->NotifyObserverBeginBatch();
    batch_in_progress_ = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeModel, public:
CookiesTreeModel::CookiesTreeModel(
    std::unique_ptr<LocalDataContainer> data_container,
    ExtensionSpecialStoragePolicy* special_storage_policy)
    : ui::TreeNodeModel<CookieTreeNode>(
          std::make_unique<CookieTreeRootNode>(this)),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      special_storage_policy_(special_storage_policy),
#endif
      data_container_(std::move(data_container)) {
  data_container_->Init(this);
}

CookiesTreeModel::~CookiesTreeModel() {
}

// static
int CookiesTreeModel::GetSendForMessageID(const net::CanonicalCookie& cookie) {
  if (cookie.IsSecure()) {
    if (cookie.SameSite() != net::CookieSameSite::NO_RESTRICTION)
      return IDS_COOKIES_COOKIE_SENDFOR_SECURE_SAME_SITE;
    return IDS_COOKIES_COOKIE_SENDFOR_SECURE;
  }
  if (cookie.SameSite() != net::CookieSameSite::NO_RESTRICTION)
    return IDS_COOKIES_COOKIE_SENDFOR_SAME_SITE;
  return IDS_COOKIES_COOKIE_SENDFOR_ANY;
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeModel, TreeModel methods (public):

// TreeModel methods:
// Returns the set of icons for the nodes in the tree. You only need override
// this if you don't want to use the default folder icons.
void CookiesTreeModel::GetIcons(std::vector<gfx::ImageSkia>* icons) {
  icons->push_back(*ui::ResourceBundle::GetSharedInstance()
                        .GetNativeImageNamed(IDR_DEFAULT_FAVICON)
                        .ToImageSkia());
  icons->push_back(*ui::ResourceBundle::GetSharedInstance()
                        .GetNativeImageNamed(IDR_COOKIES)
                        .ToImageSkia());
  icons->push_back(*ui::ResourceBundle::GetSharedInstance()
                        .GetNativeImageNamed(IDR_COOKIE_STORAGE_ICON)
                        .ToImageSkia());
}

// Returns the index of the icon to use for |node|. Return -1 to use the
// default icon. The index is relative to the list of icons returned from
// GetIcons.
int CookiesTreeModel::GetIconIndex(ui::TreeModelNode* node) {
  CookieTreeNode* ct_node = static_cast<CookieTreeNode*>(node);
  switch (ct_node->GetDetailedInfo().node_type) {
    case CookieTreeNode::DetailedInfo::TYPE_HOST:
      return ORIGIN;

    // Fall through each below cases to return COOKIE.
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
    case CookieTreeNode::DetailedInfo::TYPE_CHANNEL_ID:
      return COOKIE;

    // Fall through each below cases to return DATABASE.
    case CookieTreeNode::DetailedInfo::TYPE_DATABASE:
    case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE:
    case CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE:
    case CookieTreeNode::DetailedInfo::TYPE_APPCACHE:
    case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB:
    case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM:
    case CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER:
    case CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKER:
    case CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE:
    case CookieTreeNode::DetailedInfo::TYPE_MEDIA_LICENSE:
      return DATABASE;
    case CookieTreeNode::DetailedInfo::TYPE_QUOTA:
      return -1;
    default:
      break;
  }
  return -1;
}

void CookiesTreeModel::DeleteAllStoredObjects() {
  NotifyObserverBeginBatch();
  CookieTreeNode* root = GetRoot();
  root->DeleteStoredObjects();
  root->DeleteAll();
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::DeleteCookieNode(CookieTreeNode* cookie_node) {
  if (cookie_node == GetRoot())
    return;
  cookie_node->DeleteStoredObjects();
  CookieTreeNode* parent_node = cookie_node->parent();
  Remove(parent_node, cookie_node);
  if (parent_node->empty())
    DeleteCookieNode(parent_node);
}

void CookiesTreeModel::UpdateSearchResults(const base::string16& filter) {
  CookieTreeNode* root = GetRoot();
  SetBatchExpectation(1, true);
  ScopedBatchUpdateNotifier notifier(this, root);
  notifier.StartBatchUpdate();
  root->DeleteAll();

  PopulateCookieInfoWithFilter(data_container(), &notifier, filter);
  PopulateDatabaseInfoWithFilter(data_container(), &notifier, filter);
  PopulateLocalStorageInfoWithFilter(data_container(), &notifier, filter);
  PopulateSessionStorageInfoWithFilter(data_container(), &notifier, filter);
  PopulateAppCacheInfoWithFilter(data_container(), &notifier, filter);
  PopulateIndexedDBInfoWithFilter(data_container(), &notifier, filter);
  PopulateFileSystemInfoWithFilter(data_container(), &notifier, filter);
  PopulateQuotaInfoWithFilter(data_container(), &notifier, filter);
  PopulateChannelIDInfoWithFilter(data_container(), &notifier, filter);
  PopulateServiceWorkerUsageInfoWithFilter(data_container(), &notifier, filter);
  PopulateSharedWorkerInfoWithFilter(data_container(), &notifier, filter);
  PopulateCacheStorageUsageInfoWithFilter(data_container(), &notifier, filter);
  PopulateFlashLSOInfoWithFilter(data_container(), &notifier, filter);
  PopulateMediaLicenseInfoWithFilter(data_container(), &notifier, filter);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
const extensions::ExtensionSet* CookiesTreeModel::ExtensionsProtectingNode(
    const CookieTreeNode& cookie_node) {
  if (!special_storage_policy_.get())
    return nullptr;

  CookieTreeNode::DetailedInfo info = cookie_node.GetDetailedInfo();

  if (!TypeIsProtected(info.node_type))
    return nullptr;

  DCHECK(!info.origin.is_empty());
  return special_storage_policy_->ExtensionsProtectingOrigin(info.origin);
}
#endif

void CookiesTreeModel::AddCookiesTreeObserver(Observer* observer) {
  cookies_observer_list_.AddObserver(observer);
  // Call super so that TreeNodeModel can notify, too.
  ui::TreeNodeModel<CookieTreeNode>::AddObserver(observer);
}

void CookiesTreeModel::RemoveCookiesTreeObserver(Observer* observer) {
  cookies_observer_list_.RemoveObserver(observer);
  // Call super so that TreeNodeModel doesn't have dead pointers.
  ui::TreeNodeModel<CookieTreeNode>::RemoveObserver(observer);
}

void CookiesTreeModel::PopulateAppCacheInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateAppCacheInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateCookieInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateCookieInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateDatabaseInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateDatabaseInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateLocalStorageInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateLocalStorageInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateSessionStorageInfo(
      LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateSessionStorageInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateIndexedDBInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateIndexedDBInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateFileSystemInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateFileSystemInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateQuotaInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateQuotaInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateChannelIDInfo(
      LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateChannelIDInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateServiceWorkerUsageInfo(
    LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateServiceWorkerUsageInfoWithFilter(
      container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateSharedWorkerInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateSharedWorkerInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateCacheStorageUsageInfo(
    LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateCacheStorageUsageInfoWithFilter(container, &notifier,
                                          base::string16());
}

void CookiesTreeModel::PopulateFlashLSOInfo(
      LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateFlashLSOInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateMediaLicenseInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateMediaLicenseInfoWithFilter(container, &notifier, base::string16());
}

void CookiesTreeModel::PopulateAppCacheInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  using content::AppCacheInfo;
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->appcache_info_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto& origin : container->appcache_info_) {
    base::string16 host_node_name = base::UTF8ToUTF16(origin.first.host());
    if (filter.empty() ||
        (host_node_name.find(filter) != base::string16::npos)) {
      CookieTreeHostNode* host_node =
          root->GetOrCreateHostNode(origin.first.GetURL());
      CookieTreeAppCachesNode* appcaches_node =
          host_node->GetOrCreateAppCachesNode();

      for (auto info = origin.second.begin(); info != origin.second.end();
           ++info) {
        appcaches_node->AddAppCacheNode(
            std::make_unique<CookieTreeAppCacheNode>(origin.first, info));
      }
    }
  }
}

void CookiesTreeModel::PopulateCookieInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  notifier->StartBatchUpdate();
  for (auto it = container->cookie_list_.begin();
       it != container->cookie_list_.end(); ++it) {
    std::string domain = it->Domain();
    if (domain.length() > 1 && domain[0] == '.')
      domain = domain.substr(1);

    // Cookies ignore schemes, so group all HTTP and HTTPS cookies together.
    GURL source(std::string(url::kHttpScheme) + url::kStandardSchemeSeparator +
                domain + "/");

    if (filter.empty() || (CookieTreeHostNode::TitleForUrl(source)
                               .find(filter) != base::string16::npos)) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(source);
      CookieTreeCookiesNode* cookies_node =
          host_node->GetOrCreateCookiesNode();
      cookies_node->AddCookieNode(std::make_unique<CookieTreeCookieNode>(it));
    }
  }
}

void CookiesTreeModel::PopulateDatabaseInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->database_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto database_info = container->database_info_list_.begin();
       database_info != container->database_info_list_.end(); ++database_info) {
    GURL origin(database_info->identifier.ToOrigin());

    if (filter.empty() || (CookieTreeHostNode::TitleForUrl(origin)
                               .find(filter) != base::string16::npos)) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(origin);
      CookieTreeDatabasesNode* databases_node =
          host_node->GetOrCreateDatabasesNode();
      databases_node->AddDatabaseNode(
          std::make_unique<CookieTreeDatabaseNode>(database_info));
    }
  }
}

void CookiesTreeModel::PopulateLocalStorageInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->local_storage_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto local_storage_info = container->local_storage_info_list_.begin();
       local_storage_info != container->local_storage_info_list_.end();
       ++local_storage_info) {
    const GURL& origin(local_storage_info->origin_url);

    if (filter.empty() || (CookieTreeHostNode::TitleForUrl(origin)
                               .find(filter) != std::string::npos)) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(origin);
      CookieTreeLocalStoragesNode* local_storages_node =
          host_node->GetOrCreateLocalStoragesNode();
      local_storages_node->AddLocalStorageNode(
          std::make_unique<CookieTreeLocalStorageNode>(local_storage_info));
    }
  }
}

void CookiesTreeModel::PopulateSessionStorageInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->session_storage_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto session_storage_info =
           container->session_storage_info_list_.begin();
       session_storage_info != container->session_storage_info_list_.end();
       ++session_storage_info) {
    const GURL& origin = session_storage_info->origin_url;

    if (filter.empty() || (CookieTreeHostNode::TitleForUrl(origin)
                               .find(filter) != base::string16::npos)) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(origin);
      CookieTreeSessionStoragesNode* session_storages_node =
          host_node->GetOrCreateSessionStoragesNode();
      session_storages_node->AddSessionStorageNode(
          std::make_unique<CookieTreeSessionStorageNode>(session_storage_info));
    }
  }
}

void CookiesTreeModel::PopulateIndexedDBInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->indexed_db_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto indexed_db_info = container->indexed_db_info_list_.begin();
       indexed_db_info != container->indexed_db_info_list_.end();
       ++indexed_db_info) {
    const GURL& origin = indexed_db_info->origin;

    if (filter.empty() || (CookieTreeHostNode::TitleForUrl(origin)
                               .find(filter) != base::string16::npos)) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(origin);
      CookieTreeIndexedDBsNode* indexed_dbs_node =
          host_node->GetOrCreateIndexedDBsNode();
      indexed_dbs_node->AddIndexedDBNode(
          std::make_unique<CookieTreeIndexedDBNode>(indexed_db_info));
    }
  }
}

void CookiesTreeModel::PopulateChannelIDInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->channel_id_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto channel_id_info = container->channel_id_list_.begin();
       channel_id_info != container->channel_id_list_.end();
       ++channel_id_info) {
    GURL origin(channel_id_info->server_identifier());
    if (!origin.is_valid()) {
      // Channel ID.  Make a valid URL to satisfy the
      // CookieTreeRootNode::GetOrCreateHostNode interface.
      origin = GURL(std::string(url::kHttpsScheme) +
          url::kStandardSchemeSeparator +
          channel_id_info->server_identifier() + "/");
    }
    base::string16 title = CookieTreeHostNode::TitleForUrl(origin);
    if (filter.empty() || title.find(filter) != base::string16::npos) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(origin);
      CookieTreeChannelIDsNode* channel_ids_node =
          host_node->GetOrCreateChannelIDsNode();
      channel_ids_node->AddChannelIDNode(
          std::make_unique<CookieTreeChannelIDNode>(channel_id_info));
    }
  }
}

void CookiesTreeModel::PopulateServiceWorkerUsageInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->service_worker_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto service_worker_info = container->service_worker_info_list_.begin();
       service_worker_info != container->service_worker_info_list_.end();
       ++service_worker_info) {
    const GURL& origin = service_worker_info->origin;

    if (filter.empty() || (CookieTreeHostNode::TitleForUrl(origin)
                               .find(filter) != base::string16::npos)) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(origin);
      CookieTreeServiceWorkersNode* service_workers_node =
          host_node->GetOrCreateServiceWorkersNode();
      service_workers_node->AddServiceWorkerNode(
          std::make_unique<CookieTreeServiceWorkerNode>(service_worker_info));
    }
  }
}

void CookiesTreeModel::PopulateSharedWorkerInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->shared_worker_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto shared_worker_info = container->shared_worker_info_list_.begin();
       shared_worker_info != container->shared_worker_info_list_.end();
       ++shared_worker_info) {
    const GURL& worker = shared_worker_info->worker;

    if (filter.empty() || (CookieTreeHostNode::TitleForUrl(worker).find(
                               filter) != base::string16::npos)) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(worker);
      CookieTreeSharedWorkersNode* shared_workers_node =
          host_node->GetOrCreateSharedWorkersNode();
      shared_workers_node->AddSharedWorkerNode(
          std::make_unique<CookieTreeSharedWorkerNode>(shared_worker_info));
    }
  }
}

void CookiesTreeModel::PopulateCacheStorageUsageInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->cache_storage_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto cache_storage_info = container->cache_storage_info_list_.begin();
       cache_storage_info != container->cache_storage_info_list_.end();
       ++cache_storage_info) {
    const GURL& origin = cache_storage_info->origin;

    if (filter.empty() || (CookieTreeHostNode::TitleForUrl(origin)
                               .find(filter) != base::string16::npos)) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(origin);
      CookieTreeCacheStoragesNode* cache_storages_node =
          host_node->GetOrCreateCacheStoragesNode();
      cache_storages_node->AddCacheStorageNode(
          std::make_unique<CookieTreeCacheStorageNode>(cache_storage_info));
    }
  }
}

void CookiesTreeModel::PopulateFileSystemInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->file_system_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto file_system_info = container->file_system_info_list_.begin();
       file_system_info != container->file_system_info_list_.end();
       ++file_system_info) {
    GURL origin(file_system_info->origin);

    if (filter.empty() || (CookieTreeHostNode::TitleForUrl(origin)
                               .find(filter) != base::string16::npos)) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(origin);
      CookieTreeFileSystemsNode* file_systems_node =
          host_node->GetOrCreateFileSystemsNode();
      file_systems_node->AddFileSystemNode(
          std::make_unique<CookieTreeFileSystemNode>(file_system_info));
    }
  }
}

void CookiesTreeModel::PopulateQuotaInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->quota_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto quota_info = container->quota_info_list_.begin();
       quota_info != container->quota_info_list_.end(); ++quota_info) {
    if (filter.empty() || (base::UTF8ToUTF16(quota_info->host).find(filter) !=
                           base::string16::npos)) {
      CookieTreeHostNode* host_node =
          root->GetOrCreateHostNode(GURL("http://" + quota_info->host));
      host_node->UpdateOrCreateQuotaNode(quota_info);
    }
  }
}

void CookiesTreeModel::PopulateFlashLSOInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->flash_lso_domain_list_.empty())
    return;

  std::string filter_utf8 = base::UTF16ToUTF8(filter);
  notifier->StartBatchUpdate();
  for (auto it = container->flash_lso_domain_list_.begin();
       it != container->flash_lso_domain_list_.end(); ++it) {
    if (filter_utf8.empty() || it->find(filter_utf8) != std::string::npos) {
      // Create a fake origin for GetOrCreateHostNode().
      GURL origin("http://" + *it);
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(origin);
      host_node->GetOrCreateFlashLSONode(*it);
    }
  }
}

void CookiesTreeModel::PopulateMediaLicenseInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const base::string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->media_license_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto media_license_info = container->media_license_info_list_.begin();
       media_license_info != container->media_license_info_list_.end();
       ++media_license_info) {
    GURL origin(media_license_info->origin);

    if (filter.empty() || (CookieTreeHostNode::TitleForUrl(origin).find(
                               filter) != base::string16::npos)) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(origin);
      CookieTreeMediaLicensesNode* media_licenses_node =
          host_node->GetOrCreateMediaLicensesNode();
      media_licenses_node->AddMediaLicenseNode(
          std::make_unique<CookieTreeMediaLicenseNode>(media_license_info));
    }
  }
}

void CookiesTreeModel::SetBatchExpectation(int batches_expected, bool reset) {
  batches_expected_ = batches_expected;
  if (reset) {
    batches_seen_ = 0;
    batches_started_ = 0;
    batches_ended_ = 0;
  } else {
    MaybeNotifyBatchesEnded();
  }
}

void CookiesTreeModel::RecordBatchSeen() {
  batches_seen_++;
}

void CookiesTreeModel::NotifyObserverBeginBatch() {
  // Only notify the model once if we're batching in a nested manner.
  if (batches_started_++ == 0) {
    for (Observer& observer : cookies_observer_list_)
      observer.TreeModelBeginBatch(this);
  }
}

void CookiesTreeModel::NotifyObserverEndBatch() {
  batches_ended_++;
  MaybeNotifyBatchesEnded();
}

void CookiesTreeModel::MaybeNotifyBatchesEnded() {
  // Only notify the observers if this is the outermost call to EndBatch() if
  // called in a nested manner.
  if (batches_ended_ == batches_started_ &&
      batches_seen_ == batches_expected_) {
    for (Observer& observer : cookies_observer_list_)
      observer.TreeModelEndBatch(this);
    SetBatchExpectation(0, true);
  }
}
