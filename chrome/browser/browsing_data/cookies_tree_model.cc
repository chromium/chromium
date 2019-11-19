// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/cookies_tree_model.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cache_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_quota_helper.h"
#include "chrome/browser/browsing_data/browsing_data_service_worker_helper.h"
#include "chrome/browser/browsing_data/browsing_data_shared_worker_helper.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
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

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitHost(
    const GURL& origin) {
  Init(TYPE_HOST);
  this->origin = url::Origin::Create(origin);
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitCookie(
    const net::CanonicalCookie* cookie) {
  Init(TYPE_COOKIE);
  this->cookie = cookie;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitDatabase(
    const content::StorageUsageInfo* usage_info) {
  Init(TYPE_DATABASE);
  this->usage_info = usage_info;
  origin = usage_info->origin;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitLocalStorage(
    const content::StorageUsageInfo* usage_info) {
  Init(TYPE_LOCAL_STORAGE);
  this->usage_info = usage_info;
  origin = usage_info->origin;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitSessionStorage(
    const content::StorageUsageInfo* usage_info) {
  Init(TYPE_SESSION_STORAGE);
  this->usage_info = usage_info;
  origin = usage_info->origin;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitAppCache(
    const content::StorageUsageInfo* usage_info) {
  Init(TYPE_APPCACHE);
  this->usage_info = usage_info;
  origin = usage_info->origin;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitIndexedDB(
    const content::StorageUsageInfo* usage_info) {
  Init(TYPE_INDEXED_DB);
  this->usage_info = usage_info;
  this->origin = usage_info->origin;
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

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitServiceWorker(
    const content::StorageUsageInfo* usage_info) {
  Init(TYPE_SERVICE_WORKER);
  this->usage_info = usage_info;
  this->origin = usage_info->origin;
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitSharedWorker(
    const BrowsingDataSharedWorkerHelper::SharedWorkerInfo*
        shared_worker_info) {
  Init(TYPE_SHARED_WORKER);
  this->shared_worker_info = shared_worker_info;
  this->origin = url::Origin::Create(shared_worker_info->worker.GetOrigin());
  return *this;
}

CookieTreeNode::DetailedInfo& CookieTreeNode::DetailedInfo::InitCacheStorage(
    const content::StorageUsageInfo* usage_info) {
  Init(TYPE_CACHE_STORAGE);
  this->usage_info = usage_info;
  this->origin = usage_info->origin;
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
  this->origin = url::Origin::Create(media_license_info->origin);
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

int64_t CookieTreeNode::InclusiveSize() const {
  return 0;
}

int CookieTreeNode::NumberOfCookies() const {
  return std::accumulate(children().cbegin(), children().cend(), 0,
                         [](int total, const auto& child) {
                           return total + child->NumberOfCookies();
                         });
}

void CookieTreeNode::AddChildSortedByTitle(
    std::unique_ptr<CookieTreeNode> new_child) {
  DCHECK(new_child);
  auto iter = std::lower_bound(children().begin(), children().end(), new_child,
                               NodeTitleComparator());
  GetModel()->Add(this, std::move(new_child),
                  size_t{iter - children().begin()});
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookieNode

class CookieTreeCookieNode : public CookieTreeNode {
 public:
  friend class CookieTreeCookiesNode;

  // The cookie should remain valid at least as long as the
  // CookieTreeCookieNode is valid.
  explicit CookieTreeCookieNode(
      std::list<net::CanonicalCookie>::iterator cookie)
      : CookieTreeNode(base::UTF8ToUTF16(cookie->Name())), cookie_(cookie) {}

  ~CookieTreeCookieNode() override {}

  // CookieTreeNode methods:
  void DeleteStoredObjects() override {
    LocalDataContainer* container = GetLocalDataContainerForNode(this);
    container->cookie_helper_->DeleteCookie(*cookie_);
    container->cookie_list_.erase(cookie_);
  }
  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitCookie(&*cookie_);
  }

  int NumberOfCookies() const override { return 1; }

 private:
  // |cookie_| is expected to remain valid as long as the CookieTreeCookieNode
  // is valid.
  std::list<net::CanonicalCookie>::iterator cookie_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeCookieNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeAppCacheNode

class CookieTreeAppCacheNode : public CookieTreeNode {
 public:
  friend class CookieTreeAppCachesNode;

  // |appcache_info| should remain valid at least as long as the
  // CookieTreeAppCacheNode is valid.
  explicit CookieTreeAppCacheNode(
      std::list<content::StorageUsageInfo>::iterator usage_info)
      : CookieTreeNode(base::UTF8ToUTF16(usage_info->origin.Serialize())),
        usage_info_(usage_info) {}
  ~CookieTreeAppCacheNode() override {}

  void DeleteStoredObjects() override {
    LocalDataContainer* container = GetLocalDataContainerForNode(this);

    if (container) {
      container->appcache_helper_->DeleteAppCaches(usage_info_->origin);
      container->appcache_info_list_.erase(usage_info_);
    }
  }
  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitAppCache(&*usage_info_);
  }

  int64_t InclusiveSize() const override {
    return usage_info_->total_size_bytes;
  }

 private:
  // |usage_info_| is expected to remain valid as long as this node is valid.
  std::list<content::StorageUsageInfo>::iterator usage_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeAppCacheNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeDatabaseNode

class CookieTreeDatabaseNode : public CookieTreeNode {
 public:
  friend class CookieTreeDatabasesNode;

  // |usage_info| should remain valid at least as long as the
  // CookieTreeDatabaseNode is valid.
  explicit CookieTreeDatabaseNode(
      std::list<content::StorageUsageInfo>::iterator usage_info)
      : CookieTreeNode(base::UTF8ToUTF16(usage_info->origin.Serialize())),
        usage_info_(usage_info) {}

  ~CookieTreeDatabaseNode() override {}

  void DeleteStoredObjects() override {
    LocalDataContainer* container = GetLocalDataContainerForNode(this);

    if (container) {
      container->database_helper_->DeleteDatabase(usage_info_->origin);
      container->database_info_list_.erase(usage_info_);
    }
  }

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitDatabase(&*usage_info_);
  }

  int64_t InclusiveSize() const override {
    return usage_info_->total_size_bytes;
  }

 private:
  // |database_info_| is expected to remain valid as long as the
  // CookieTreeDatabaseNode is valid.
  std::list<content::StorageUsageInfo>::iterator usage_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeDatabaseNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeLocalStorageNode

class CookieTreeLocalStorageNode : public CookieTreeNode {
 public:
  // |usage_info| should remain valid at least as long as the
  // CookieTreeLocalStorageNode is valid.
  explicit CookieTreeLocalStorageNode(
      std::list<content::StorageUsageInfo>::iterator local_storage_info)
      : CookieTreeNode(
            base::UTF8ToUTF16(local_storage_info->origin.Serialize())),
        local_storage_info_(local_storage_info) {}

  ~CookieTreeLocalStorageNode() override {}

  // CookieTreeNode methods:
  void DeleteStoredObjects() override {
    LocalDataContainer* container = GetLocalDataContainerForNode(this);

    if (container) {
      container->local_storage_helper_->DeleteOrigin(
          local_storage_info_->origin, base::DoNothing());
      container->local_storage_info_list_.erase(local_storage_info_);
    }
  }
  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitLocalStorage(&*local_storage_info_);
  }

  int64_t InclusiveSize() const override {
    return local_storage_info_->total_size_bytes;
  }

 private:
  // |local_storage_info_| is expected to remain valid as long as the
  // CookieTreeLocalStorageNode is valid.
  std::list<content::StorageUsageInfo>::iterator local_storage_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeLocalStorageNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSessionStorageNode

class CookieTreeSessionStorageNode : public CookieTreeNode {
 public:
  // |session_storage_info| should remain valid at least as long as the
  // CookieTreeSessionStorageNode is valid.
  explicit CookieTreeSessionStorageNode(
      std::list<content::StorageUsageInfo>::iterator session_storage_info)
      : CookieTreeNode(
            base::UTF8ToUTF16(session_storage_info->origin.Serialize())),
        session_storage_info_(session_storage_info) {}

  ~CookieTreeSessionStorageNode() override {}

  // CookieTreeNode methods:
  void DeleteStoredObjects() override {
    LocalDataContainer* container = GetLocalDataContainerForNode(this);

    if (container) {
      // TODO(rsesek): There's no easy way to get the namespace_id for a session
      // storage, nor is there an easy way to clear session storage just by
      // origin. This is probably okay since session storage is not persistent.
      // http://crbug.com/168996
      container->session_storage_info_list_.erase(session_storage_info_);
    }
  }
  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitSessionStorage(&*session_storage_info_);
  }

 private:
  // |session_storage_info_| is expected to remain valid as long as the
  // CookieTreeSessionStorageNode is valid.
  std::list<content::StorageUsageInfo>::iterator session_storage_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeSessionStorageNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeIndexedDBNode

class CookieTreeIndexedDBNode : public CookieTreeNode {
 public:
  // |usage_info| should remain valid at least as long as the
  // CookieTreeIndexedDBNode is valid.
  explicit CookieTreeIndexedDBNode(
      std::list<content::StorageUsageInfo>::iterator usage_info)
      : CookieTreeNode(base::UTF8ToUTF16(usage_info->origin.Serialize())),
        usage_info_(usage_info) {}

  ~CookieTreeIndexedDBNode() override {}

  // CookieTreeNode methods:
  void DeleteStoredObjects() override {
    LocalDataContainer* container = GetLocalDataContainerForNode(this);

    if (container) {
      container->indexed_db_helper_->DeleteIndexedDB(
          usage_info_->origin.GetURL());
      container->indexed_db_info_list_.erase(usage_info_);
    }
  }

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitIndexedDB(&*usage_info_);
  }

  int64_t InclusiveSize() const override {
    return usage_info_->total_size_bytes;
  }

 private:
  // |usage_info_| is expected to remain valid as long as the
  // CookieTreeIndexedDBNode is valid.
  std::list<content::StorageUsageInfo>::iterator usage_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeIndexedDBNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeFileSystemNode

class CookieTreeFileSystemNode : public CookieTreeNode {
 public:
  friend class CookieTreeFileSystemsNode;

  // |file_system_info| should remain valid at least as long as the
  // CookieTreeFileSystemNode is valid.
  explicit CookieTreeFileSystemNode(
      std::list<BrowsingDataFileSystemHelper::FileSystemInfo>::iterator
          file_system_info)
      : CookieTreeNode(base::UTF8ToUTF16(file_system_info->origin.Serialize())),
        file_system_info_(file_system_info) {}
  ~CookieTreeFileSystemNode() override {}

  void DeleteStoredObjects() override {
    LocalDataContainer* container = GetLocalDataContainerForNode(this);

    if (container) {
      container->file_system_helper_->DeleteFileSystemOrigin(
          file_system_info_->origin);
      container->file_system_info_list_.erase(file_system_info_);
    }
  }

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitFileSystem(&*file_system_info_);
  }

  int64_t InclusiveSize() const override {
    int64_t size = 0;
    for (auto const& usage : file_system_info_->usage_map) {
      size += usage.second;
    }
    return size;
  }

 private:
  // file_system_info_ expected to remain valid as long as the
  // CookieTreeFileSystemNode is valid.
  std::list<BrowsingDataFileSystemHelper::FileSystemInfo>::iterator
      file_system_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeFileSystemNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeQuotaNode

class CookieTreeQuotaNode : public CookieTreeNode {
 public:
  // |quota_info| should remain valid at least as long as the
  // CookieTreeQuotaNode is valid.
  explicit CookieTreeQuotaNode(
      std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info)
      : CookieTreeNode(base::UTF8ToUTF16(quota_info->host)),
        quota_info_(quota_info) {}

  ~CookieTreeQuotaNode() override {}

  void DeleteStoredObjects() override {
    // Calling this function may cause unexpected over-quota state of origin.
    // However, it'll caused no problem, just prevent usage growth of the
    // origin.
    LocalDataContainer* container = GetModel()->data_container();

    if (container) {
      container->quota_helper_->RevokeHostQuota(quota_info_->host);
      container->quota_info_list_.erase(quota_info_);
    }
  }
  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitQuota(&*quota_info_);
  }

 private:
  // |quota_info_| is expected to remain valid as long as the
  // CookieTreeQuotaNode is valid.
  std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeQuotaNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeServiceWorkerNode

class CookieTreeServiceWorkerNode : public CookieTreeNode {
 public:
  // |usage_info| should remain valid at least as long as the
  // CookieTreeServiceWorkerNode is valid.
  explicit CookieTreeServiceWorkerNode(
      std::list<content::StorageUsageInfo>::iterator usage_info)
      : CookieTreeNode(base::UTF8ToUTF16(usage_info->origin.Serialize())),
        usage_info_(usage_info) {}

  ~CookieTreeServiceWorkerNode() override {}

  // CookieTreeNode methods:
  void DeleteStoredObjects() override {
    LocalDataContainer* container = GetLocalDataContainerForNode(this);

    if (container) {
      container->service_worker_helper_->DeleteServiceWorkers(
          usage_info_->origin.GetURL());
      container->service_worker_info_list_.erase(usage_info_);
    }
  }

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitServiceWorker(&*usage_info_);
  }

  int64_t InclusiveSize() const override {
    return usage_info_->total_size_bytes;
  }

 private:
  // |usage_info_| is expected to remain valid as long as the
  // CookieTreeServiceWorkerNode is valid.
  std::list<content::StorageUsageInfo>::iterator usage_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeServiceWorkerNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSharedWorkerNode

class CookieTreeSharedWorkerNode : public CookieTreeNode {
 public:
  // |shared_worker_info| should remain valid at least as long as the
  // CookieTreeSharedWorkerNode is valid.
  explicit CookieTreeSharedWorkerNode(
      std::list<BrowsingDataSharedWorkerHelper::SharedWorkerInfo>::iterator
          shared_worker_info)
      : CookieTreeNode(base::UTF8ToUTF16(shared_worker_info->worker.spec())),
        shared_worker_info_(shared_worker_info) {}

  ~CookieTreeSharedWorkerNode() override {}

  // CookieTreeNode methods:
  void DeleteStoredObjects() override {
    LocalDataContainer* container = GetLocalDataContainerForNode(this);

    if (container) {
      container->shared_worker_helper_->DeleteSharedWorker(
          shared_worker_info_->worker, shared_worker_info_->name,
          shared_worker_info_->constructor_origin);
      container->shared_worker_info_list_.erase(shared_worker_info_);
    }
  }

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitSharedWorker(&*shared_worker_info_);
  }

 private:
  // |shared_worker_info_| is expected to remain valid as long as the
  // CookieTreeSharedWorkerNode is valid.
  std::list<BrowsingDataSharedWorkerHelper::SharedWorkerInfo>::iterator
      shared_worker_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeSharedWorkerNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCacheStorageNode

class CookieTreeCacheStorageNode : public CookieTreeNode {
 public:
  // |usage_info| should remain valid at least as long as the
  // CookieTreeCacheStorageNode is valid.
  explicit CookieTreeCacheStorageNode(
      std::list<content::StorageUsageInfo>::iterator usage_info)
      : CookieTreeNode(base::UTF8ToUTF16(usage_info->origin.Serialize())),
        usage_info_(usage_info) {}

  ~CookieTreeCacheStorageNode() override {}

  // CookieTreeNode methods:
  void DeleteStoredObjects() override {
    LocalDataContainer* container = GetLocalDataContainerForNode(this);

    if (container) {
      container->cache_storage_helper_->DeleteCacheStorage(
          usage_info_->origin.GetURL());
      container->cache_storage_info_list_.erase(usage_info_);
    }
  }

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitCacheStorage(&*usage_info_);
  }

  int64_t InclusiveSize() const override {
    return usage_info_->total_size_bytes;
  }

 private:
  // |usage_info_| is expected to remain valid as long as the
  // CookieTreeCacheStorageNode is valid.
  std::list<content::StorageUsageInfo>::iterator usage_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeCacheStorageNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeMediaLicenseNode

class CookieTreeMediaLicenseNode : public CookieTreeNode {
 public:
  friend class CookieTreeMediaLicensesNode;

  // |media_license_info| is expected to remain valid as long as the
  // CookieTreeMediaLicenseNode is valid.
  explicit CookieTreeMediaLicenseNode(
      const std::list<BrowsingDataMediaLicenseHelper::MediaLicenseInfo>::
          iterator media_license_info)
      : CookieTreeNode(base::UTF8ToUTF16(media_license_info->origin.spec())),
        media_license_info_(media_license_info) {}

  ~CookieTreeMediaLicenseNode() override {}

  void DeleteStoredObjects() override {
    LocalDataContainer* container = GetLocalDataContainerForNode(this);

    if (container) {
      container->media_license_helper_->DeleteMediaLicenseOrigin(
          media_license_info_->origin);
      container->media_license_info_list_.erase(media_license_info_);
    }
  }

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitMediaLicense(&*media_license_info_);
  }

  int64_t InclusiveSize() const override { return media_license_info_->size; }

 private:
  // |media_license_info_| is expected to remain valid as long as the
  // CookieTreeMediaLicenseNode is valid.
  std::list<BrowsingDataMediaLicenseHelper::MediaLicenseInfo>::iterator
      media_license_info_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeMediaLicenseNode);
};

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
          (*host_node_iterator)->GetTitle()) {
    CookieTreeHostNode* found_node =
        static_cast<CookieTreeHostNode*>(host_node_iterator->get());
    // Cookies node will create fake url with http scheme, so update the url if
    // there is a more valid url.
    if (found_node->GetDetailedInfo().origin.GetURL().SchemeIs(
            url::kHttpScheme) &&
        url.SchemeIs(url::kHttpsScheme))
      found_node->UpdateHostUrl(url);
    return found_node;
  }
  // Node doesn't exist, insert the new one into the (ordered) children.
  DCHECK(model_);
  return static_cast<CookieTreeHostNode*>(
      model_->Add(this, std::move(host_node),
                  size_t{host_node_iterator - children().begin()}));
}

CookiesTreeModel* CookieTreeRootNode::GetModel() const {
  return model_;
}

CookieTreeNode::DetailedInfo CookieTreeRootNode::GetDetailedInfo() const {
  return DetailedInfo().Init(DetailedInfo::TYPE_ROOT);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookiesNode

class CookieTreeCookiesNode : public CookieTreeNode {
 public:
  CookieTreeCookiesNode()
      : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_COOKIES)) {}

  ~CookieTreeCookiesNode() override {}

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().Init(DetailedInfo::TYPE_COOKIES);
  }

  void AddCookieNode(std::unique_ptr<CookieTreeCookieNode> child) {
    AddChildSortedByTitle(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeCookiesNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCollectionNode

class CookieTreeCollectionNode : public CookieTreeNode {
 public:
  explicit CookieTreeCollectionNode(const base::string16& title)
      : CookieTreeNode(title) {}

  ~CookieTreeCollectionNode() override {}

  int64_t InclusiveSize() const final {
    return std::accumulate(children().cbegin(), children().cend(), int64_t{0},
                           [](int64_t total, const auto& child) {
                             return total + child->InclusiveSize();
                           });
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeCollectionNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeAppCachesNode

class CookieTreeAppCachesNode : public CookieTreeCollectionNode {
 public:
  CookieTreeAppCachesNode()
      : CookieTreeCollectionNode(
            l10n_util::GetStringUTF16(IDS_COOKIES_APPLICATION_CACHES)) {}

  ~CookieTreeAppCachesNode() override {}

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().Init(DetailedInfo::TYPE_APPCACHES);
  }

  void AddAppCacheNode(std::unique_ptr<CookieTreeAppCacheNode> child) {
    AddChildSortedByTitle(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeAppCachesNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeDatabasesNode

class CookieTreeDatabasesNode : public CookieTreeCollectionNode {
 public:
  CookieTreeDatabasesNode()
      : CookieTreeCollectionNode(
            l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASES)) {}

  ~CookieTreeDatabasesNode() override {}

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().Init(DetailedInfo::TYPE_DATABASES);
  }

  void AddDatabaseNode(std::unique_ptr<CookieTreeDatabaseNode> child) {
    AddChildSortedByTitle(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeDatabasesNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeLocalStoragesNode

class CookieTreeLocalStoragesNode : public CookieTreeCollectionNode {
 public:
  CookieTreeLocalStoragesNode()
      : CookieTreeCollectionNode(
            l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE)) {}

  ~CookieTreeLocalStoragesNode() override {}

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().Init(DetailedInfo::TYPE_LOCAL_STORAGES);
  }

  void AddLocalStorageNode(std::unique_ptr<CookieTreeLocalStorageNode> child) {
    AddChildSortedByTitle(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeLocalStoragesNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSessionStoragesNode

class CookieTreeSessionStoragesNode : public CookieTreeNode {
 public:
  CookieTreeSessionStoragesNode()
      : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_SESSION_STORAGE)) {
  }

  ~CookieTreeSessionStoragesNode() override {}

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().Init(DetailedInfo::TYPE_SESSION_STORAGES);
  }

  void AddSessionStorageNode(
      std::unique_ptr<CookieTreeSessionStorageNode> child) {
    AddChildSortedByTitle(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeSessionStoragesNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeIndexedDBsNode

class CookieTreeIndexedDBsNode : public CookieTreeCollectionNode {
 public:
  CookieTreeIndexedDBsNode()
      : CookieTreeCollectionNode(
            l10n_util::GetStringUTF16(IDS_COOKIES_INDEXED_DBS)) {}

  ~CookieTreeIndexedDBsNode() override {}

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().Init(DetailedInfo::TYPE_INDEXED_DBS);
  }

  void AddIndexedDBNode(std::unique_ptr<CookieTreeIndexedDBNode> child) {
    AddChildSortedByTitle(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeIndexedDBsNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeFileSystemsNode

class CookieTreeFileSystemsNode : public CookieTreeCollectionNode {
 public:
  CookieTreeFileSystemsNode()
      : CookieTreeCollectionNode(
            l10n_util::GetStringUTF16(IDS_COOKIES_FILE_SYSTEMS)) {}

  ~CookieTreeFileSystemsNode() override {}

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().Init(DetailedInfo::TYPE_FILE_SYSTEMS);
  }

  void AddFileSystemNode(std::unique_ptr<CookieTreeFileSystemNode> child) {
    AddChildSortedByTitle(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeFileSystemsNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeServiceWorkersNode

class CookieTreeServiceWorkersNode : public CookieTreeCollectionNode {
 public:
  CookieTreeServiceWorkersNode()
      : CookieTreeCollectionNode(
            l10n_util::GetStringUTF16(IDS_COOKIES_SERVICE_WORKERS)) {}

  ~CookieTreeServiceWorkersNode() override {}

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().Init(DetailedInfo::TYPE_SERVICE_WORKERS);
  }

  void AddServiceWorkerNode(
      std::unique_ptr<CookieTreeServiceWorkerNode> child) {
    AddChildSortedByTitle(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeServiceWorkersNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSharedWorkersNode

class CookieTreeSharedWorkersNode : public CookieTreeNode {
 public:
  CookieTreeSharedWorkersNode()
      : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_SHARED_WORKERS)) {}

  ~CookieTreeSharedWorkersNode() override {}

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().Init(DetailedInfo::TYPE_SHARED_WORKERS);
  }

  void AddSharedWorkerNode(std::unique_ptr<CookieTreeSharedWorkerNode> child) {
    AddChildSortedByTitle(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeSharedWorkersNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCacheStoragesNode

class CookieTreeCacheStoragesNode : public CookieTreeCollectionNode {
 public:
  CookieTreeCacheStoragesNode()
      : CookieTreeCollectionNode(
            l10n_util::GetStringUTF16(IDS_COOKIES_CACHE_STORAGE)) {}

  ~CookieTreeCacheStoragesNode() override {}

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().Init(DetailedInfo::TYPE_CACHE_STORAGES);
  }

  void AddCacheStorageNode(std::unique_ptr<CookieTreeCacheStorageNode> child) {
    AddChildSortedByTitle(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeCacheStoragesNode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeFlashLSONode

class CookieTreeFlashLSONode : public CookieTreeNode {
 public:
  explicit CookieTreeFlashLSONode(const std::string& domain)
      : domain_(domain) {}
  ~CookieTreeFlashLSONode() override {}

  // CookieTreeNode methods:
  void DeleteStoredObjects() override {
    // We are one level below the host node.
    CookieTreeHostNode* host = static_cast<CookieTreeHostNode*>(parent());
    CHECK_EQ(host->GetDetailedInfo().node_type,
             CookieTreeNode::DetailedInfo::TYPE_HOST);
    LocalDataContainer* container = GetModel()->data_container();
    container->flash_lso_helper_->DeleteFlashLSOsForSite(domain_,
                                                         base::OnceClosure());
    auto entry = std::find(container->flash_lso_domain_list_.begin(),
                           container->flash_lso_domain_list_.end(), domain_);
    container->flash_lso_domain_list_.erase(entry);
  }
  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().InitFlashLSO(domain_);
  }

 private:
  std::string domain_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeFlashLSONode);
};

///////////////////////////////////////////////////////////////////////////////
// CookieTreeMediaLicensesNode

class CookieTreeMediaLicensesNode : public CookieTreeCollectionNode {
 public:
  CookieTreeMediaLicensesNode()
      : CookieTreeCollectionNode(
            l10n_util::GetStringUTF16(IDS_COOKIES_MEDIA_LICENSES)) {}

  ~CookieTreeMediaLicensesNode() override {}

  DetailedInfo GetDetailedInfo() const override {
    return DetailedInfo().Init(DetailedInfo::TYPE_MEDIA_LICENSES);
  }

  void AddMediaLicenseNode(std::unique_ptr<CookieTreeMediaLicenseNode> child) {
    AddChildSortedByTitle(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeMediaLicensesNode);
};

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

void CookieTreeHostNode::UpdateHostUrl(const GURL& url) {
  this->url_ = url;
}

CookieTreeNode::DetailedInfo CookieTreeHostNode::GetDetailedInfo() const {
  return DetailedInfo().InitHost(url_);
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

int64_t CookieTreeHostNode::InclusiveSize() const {
  return std::accumulate(children().cbegin(), children().cend(), int64_t{0},
                         [](int64_t total, const auto& child) {
                           return total + child->InclusiveSize();
                         });
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
    if (!cookie.IsEffectivelySameSiteNone())
      return IDS_COOKIES_COOKIE_SENDFOR_SECURE_SAME_SITE;
    return IDS_COOKIES_COOKIE_SENDFOR_SECURE;
  }
  if (!cookie.IsEffectivelySameSiteNone())
    return IDS_COOKIES_COOKIE_SENDFOR_SAME_SITE;
  return IDS_COOKIES_COOKIE_SENDFOR_ANY;
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeModel, TreeModel methods (public):

// TreeModel methods:
// Returns the set of icons for the nodes in the tree. You only need override
// this if you don't want to use the default folder icons.
void CookiesTreeModel::GetIcons(std::vector<gfx::ImageSkia>* icons) {
  icons->push_back(
      gfx::CreateVectorIcon(kCookieIcon, 18, gfx::kChromeIconGrey));
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
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
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
    case CookieTreeNode::DetailedInfo::TYPE_HOST:
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
  if (parent_node->children().empty())
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

  DCHECK(!info.origin.opaque());
  return special_storage_policy_->ExtensionsProtectingOrigin(
      info.origin.GetURL());
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
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->appcache_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (auto it = container->appcache_info_list_.begin();
       it != container->appcache_info_list_.end(); ++it) {
    const GURL url = it->origin.GetURL();
    if (filter.empty() || (CookieTreeHostNode::TitleForUrl(url).find(filter) !=
                           base::string16::npos)) {
      CookieTreeHostNode* host_node = root->GetOrCreateHostNode(url);
      CookieTreeAppCachesNode* appcaches_node =
          host_node->GetOrCreateAppCachesNode();
      appcaches_node->AddAppCacheNode(
          std::make_unique<CookieTreeAppCacheNode>(it));
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
    if (filter.empty() ||
        (CookieTreeHostNode::TitleForUrl(database_info->origin.GetURL())
             .find(filter) != base::string16::npos)) {
      CookieTreeHostNode* host_node =
          root->GetOrCreateHostNode(database_info->origin.GetURL());
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
    const GURL& origin(local_storage_info->origin.GetURL());

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
    const GURL& origin = session_storage_info->origin.GetURL();

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
    const url::Origin& origin = indexed_db_info->origin;

    if (filter.empty() ||
        (CookieTreeHostNode::TitleForUrl(origin.GetURL()).find(filter) !=
         base::string16::npos)) {
      CookieTreeHostNode* host_node =
          root->GetOrCreateHostNode(origin.GetURL());
      CookieTreeIndexedDBsNode* indexed_dbs_node =
          host_node->GetOrCreateIndexedDBsNode();
      indexed_dbs_node->AddIndexedDBNode(
          std::make_unique<CookieTreeIndexedDBNode>(indexed_db_info));
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
    const url::Origin& origin = service_worker_info->origin;

    if (filter.empty() ||
        (CookieTreeHostNode::TitleForUrl(origin.GetURL()).find(filter) !=
         base::string16::npos)) {
      CookieTreeHostNode* host_node =
          root->GetOrCreateHostNode(origin.GetURL());
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
    const url::Origin& origin = cache_storage_info->origin;

    if (filter.empty() ||
        (CookieTreeHostNode::TitleForUrl(origin.GetURL()).find(filter) !=
         base::string16::npos)) {
      CookieTreeHostNode* host_node =
          root->GetOrCreateHostNode(origin.GetURL());
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
    GURL origin = file_system_info->origin.GetURL();

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
// static
std::unique_ptr<CookiesTreeModel> CookiesTreeModel::CreateForProfile(
    Profile* profile) {
  auto* storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(profile);
  auto* file_system_context = storage_partition->GetFileSystemContext();

  auto container = std::make_unique<LocalDataContainer>(
      new BrowsingDataCookieHelper(storage_partition),
      new BrowsingDataDatabaseHelper(profile),
      new BrowsingDataLocalStorageHelper(profile),
      /*session_storage_helper=*/nullptr,
      new BrowsingDataAppCacheHelper(storage_partition->GetAppCacheService()),
      new BrowsingDataIndexedDBHelper(storage_partition->GetIndexedDBContext()),
      BrowsingDataFileSystemHelper::Create(file_system_context),
      BrowsingDataQuotaHelper::Create(profile),
      new BrowsingDataServiceWorkerHelper(
          storage_partition->GetServiceWorkerContext()),
      new BrowsingDataSharedWorkerHelper(storage_partition,
                                         profile->GetResourceContext()),
      new BrowsingDataCacheStorageHelper(
          storage_partition->GetCacheStorageContext()),
#if defined(OS_ANDROID)
      // Android doesn't have flash LSO hence it cannot be created for
      // android build.
      nullptr,
#else
      BrowsingDataFlashLSOHelper::Create(profile),
#endif
      BrowsingDataMediaLicenseHelper::Create(file_system_context));
  return std::make_unique<CookiesTreeModel>(
      std::move(container), profile->GetExtensionSpecialStoragePolicy());
}
