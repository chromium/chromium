// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COOKIES_TREE_MODEL_H_
#define CHROME_BROWSER_BROWSING_DATA_COOKIES_TREE_MODEL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/browsing_data/local_data_container.h"
#include "components/content_settings/core/common/content_settings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/models/tree_node_model.h"

class BrowsingDataCookieHelper;
class CookiesTreeModel;
class CookieTreeAppCacheNode;
class CookieTreeAppCachesNode;
class CookieTreeCacheStorageNode;
class CookieTreeCacheStoragesNode;
class CookieTreeCookieNode;
class CookieTreeCookiesNode;
class CookieTreeDatabaseNode;
class CookieTreeDatabasesNode;
class CookieTreeFileSystemNode;
class CookieTreeFileSystemsNode;
class CookieTreeFlashLSONode;
class CookieTreeHostNode;
class CookieTreeIndexedDBNode;
class CookieTreeIndexedDBsNode;
class CookieTreeLocalStorageNode;
class CookieTreeLocalStoragesNode;
class CookieTreeMediaLicenseNode;
class CookieTreeMediaLicensesNode;
class CookieTreeQuotaNode;
class CookieTreeServiceWorkerNode;
class CookieTreeServiceWorkersNode;
class CookieTreeSharedWorkerNode;
class CookieTreeSharedWorkersNode;
class CookieTreeSessionStorageNode;
class CookieTreeSessionStoragesNode;
class ExtensionSpecialStoragePolicy;

namespace content_settings {
class CookieSettings;
}

namespace extensions {
class ExtensionSet;
}

namespace net {
class CanonicalCookie;
}

// CookieTreeNode -------------------------------------------------------------
// The base node type in the Cookies, Databases, and Local Storage options
// view, from which all other types are derived. Specialized from TreeNode in
// that it has a notion of deleting objects stored in the profile, and being
// able to have its children do the same.
class CookieTreeNode : public ui::TreeNode<CookieTreeNode> {
 public:
  // Used to pull out information for the InfoView (the details display below
  // the tree control.)
  struct DetailedInfo {
    // NodeType corresponds to the various CookieTreeNode types.
    enum NodeType {
      TYPE_NONE,
      TYPE_ROOT,              // This is used for CookieTreeRootNode nodes.
      TYPE_HOST,              // This is used for CookieTreeHostNode nodes.
      TYPE_COOKIES,           // This is used for CookieTreeCookiesNode nodes.
      TYPE_COOKIE,            // This is used for CookieTreeCookieNode nodes.
      TYPE_DATABASES,         // This is used for CookieTreeDatabasesNode.
      TYPE_DATABASE,          // This is used for CookieTreeDatabaseNode.
      TYPE_LOCAL_STORAGES,    // This is used for CookieTreeLocalStoragesNode.
      TYPE_LOCAL_STORAGE,     // This is used for CookieTreeLocalStorageNode.
      TYPE_SESSION_STORAGES,  // This is used for CookieTreeSessionStoragesNode.
      TYPE_SESSION_STORAGE,   // This is used for CookieTreeSessionStorageNode.
      TYPE_APPCACHES,         // This is used for CookieTreeAppCachesNode.
      TYPE_APPCACHE,          // This is used for CookieTreeAppCacheNode.
      TYPE_INDEXED_DBS,       // This is used for CookieTreeIndexedDBsNode.
      TYPE_INDEXED_DB,        // This is used for CookieTreeIndexedDBNode.
      TYPE_FILE_SYSTEMS,      // This is used for CookieTreeFileSystemsNode.
      TYPE_FILE_SYSTEM,       // This is used for CookieTreeFileSystemNode.
      TYPE_QUOTA,             // This is used for CookieTreeQuotaNode.
      TYPE_SERVICE_WORKERS,   // This is used for CookieTreeServiceWorkersNode.
      TYPE_SERVICE_WORKER,    // This is used for CookieTreeServiceWorkerNode.
      TYPE_SHARED_WORKERS,    // This is used for CookieTreeSharedWorkersNode.
      TYPE_SHARED_WORKER,     // This is used for CookieTreeSharedWorkerNode.
      TYPE_CACHE_STORAGES,    // This is used for CookieTreeCacheStoragesNode.
      TYPE_CACHE_STORAGE,     // This is used for CookieTreeCacheStorageNode.
      TYPE_FLASH_LSO,         // This is used for CookieTreeFlashLSONode.
      TYPE_MEDIA_LICENSES,    // This is used for CookieTreeMediaLicensesNode.
      TYPE_MEDIA_LICENSE,     // This is used for CookieTreeMediaLicenseNode.
    };

    DetailedInfo();
    DetailedInfo(const DetailedInfo& other);
    ~DetailedInfo();

    DetailedInfo& Init(NodeType type);
    DetailedInfo& InitHost(const GURL& origin);
    DetailedInfo& InitCookie(const net::CanonicalCookie* cookie);
    DetailedInfo& InitDatabase(const content::StorageUsageInfo* usage_info);
    DetailedInfo& InitLocalStorage(
        const content::StorageUsageInfo* local_storage_info);
    DetailedInfo& InitSessionStorage(
        const content::StorageUsageInfo* session_storage_info);
    DetailedInfo& InitAppCache(const content::StorageUsageInfo* usage_info);
    DetailedInfo& InitIndexedDB(const content::StorageUsageInfo* usage_info);
    DetailedInfo& InitFileSystem(
        const BrowsingDataFileSystemHelper::FileSystemInfo* file_system_info);
    DetailedInfo& InitQuota(
        const BrowsingDataQuotaHelper::QuotaInfo* quota_info);
    DetailedInfo& InitServiceWorker(
        const content::StorageUsageInfo* usage_info);
    DetailedInfo& InitSharedWorker(
        const BrowsingDataSharedWorkerHelper::SharedWorkerInfo*
            shared_worker_info);
    DetailedInfo& InitCacheStorage(const content::StorageUsageInfo* usage_info);
    DetailedInfo& InitFlashLSO(const std::string& flash_lso_domain);
    DetailedInfo& InitMediaLicense(
        const BrowsingDataMediaLicenseHelper::MediaLicenseInfo*
            media_license_info);

    NodeType node_type;
    url::Origin origin;
    const net::CanonicalCookie* cookie = nullptr;
    // Used for AppCache, Database (WebSQL), IndexedDB, Service Worker, and
    // Cache Storage node types.
    const content::StorageUsageInfo* usage_info = nullptr;
    const BrowsingDataFileSystemHelper::FileSystemInfo* file_system_info =
        nullptr;
    const BrowsingDataQuotaHelper::QuotaInfo* quota_info = nullptr;
    const BrowsingDataSharedWorkerHelper::SharedWorkerInfo* shared_worker_info =
        nullptr;
    std::string flash_lso_domain;
    const BrowsingDataMediaLicenseHelper::MediaLicenseInfo* media_license_info =
        nullptr;
  };

  CookieTreeNode() {}
  explicit CookieTreeNode(const base::string16& title)
      : ui::TreeNode<CookieTreeNode>(title) {}
  ~CookieTreeNode() override {}

  // Recursively traverse the child nodes of this node and collect the storage
  // size data.
  virtual int64_t InclusiveSize() const;

  // Recursively traverse the child nodes and calculate the number of nodes of
  // type CookieTreeCookieNode.
  virtual int NumberOfCookies() const;

  // Delete backend storage for this node, and any children nodes. (E.g. delete
  // the cookie from CookieMonster, clear the database, and so forth.)
  virtual void DeleteStoredObjects();

  // Gets a pointer back to the associated model for the tree we are in.
  virtual CookiesTreeModel* GetModel() const;

  // Returns a struct with detailed information used to populate the details
  // part of the view.
  virtual DetailedInfo GetDetailedInfo() const = 0;

 protected:
  void AddChildSortedByTitle(std::unique_ptr<CookieTreeNode> new_child);

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieTreeNode);
};

// CookieTreeRootNode ---------------------------------------------------------
// The node at the root of the CookieTree that gets inserted into the view.
class CookieTreeRootNode : public CookieTreeNode {
 public:
  explicit CookieTreeRootNode(CookiesTreeModel* model);
  ~CookieTreeRootNode() override;

  CookieTreeHostNode* GetOrCreateHostNode(const GURL& url);

  // CookieTreeNode methods:
  CookiesTreeModel* GetModel() const override;
  DetailedInfo GetDetailedInfo() const override;

 private:
  CookiesTreeModel* model_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeRootNode);
};

// CookieTreeHostNode -------------------------------------------------------
class CookieTreeHostNode : public CookieTreeNode {
 public:
  // Returns the host node's title to use for a given URL.
  static base::string16 TitleForUrl(const GURL& url);

  explicit CookieTreeHostNode(const GURL& url);
  ~CookieTreeHostNode() override;

  // CookieTreeNode methods:
  DetailedInfo GetDetailedInfo() const override;
  int64_t InclusiveSize() const override;

  // CookieTreeHostNode methods:
  CookieTreeCookiesNode* GetOrCreateCookiesNode();
  CookieTreeDatabasesNode* GetOrCreateDatabasesNode();
  CookieTreeLocalStoragesNode* GetOrCreateLocalStoragesNode();
  CookieTreeSessionStoragesNode* GetOrCreateSessionStoragesNode();
  CookieTreeAppCachesNode* GetOrCreateAppCachesNode();
  CookieTreeIndexedDBsNode* GetOrCreateIndexedDBsNode();
  CookieTreeFileSystemsNode* GetOrCreateFileSystemsNode();
  CookieTreeServiceWorkersNode* GetOrCreateServiceWorkersNode();
  CookieTreeSharedWorkersNode* GetOrCreateSharedWorkersNode();
  CookieTreeCacheStoragesNode* GetOrCreateCacheStoragesNode();
  CookieTreeQuotaNode* UpdateOrCreateQuotaNode(
      std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info);
  CookieTreeFlashLSONode* GetOrCreateFlashLSONode(const std::string& domain);
  CookieTreeMediaLicensesNode* GetOrCreateMediaLicensesNode();

  std::string canonicalized_host() const { return canonicalized_host_; }

  // Creates an content exception for this origin of type
  // ContentSettingsType::COOKIES.
  void CreateContentException(content_settings::CookieSettings* cookie_settings,
                              ContentSetting setting) const;

  // True if a content exception can be created for this origin.
  bool CanCreateContentException() const;

  std::string GetHost() const;

  void UpdateHostUrl(const GURL& url);

 private:
  // Pointers to the cookies, databases, local and session storage and appcache
  // nodes.  When we build up the tree we need to quickly get a reference to
  // the COOKIES node to add children. Checking each child and interrogating
  // them to see if they are a COOKIES, APPCACHES, DATABASES etc node seems
  // less preferable than storing an extra pointer per origin.
  CookieTreeCookiesNode* cookies_child_ = nullptr;
  CookieTreeDatabasesNode* databases_child_ = nullptr;
  CookieTreeLocalStoragesNode* local_storages_child_ = nullptr;
  CookieTreeSessionStoragesNode* session_storages_child_ = nullptr;
  CookieTreeAppCachesNode* appcaches_child_ = nullptr;
  CookieTreeIndexedDBsNode* indexed_dbs_child_ = nullptr;
  CookieTreeFileSystemsNode* file_systems_child_ = nullptr;
  CookieTreeQuotaNode* quota_child_ = nullptr;
  CookieTreeServiceWorkersNode* service_workers_child_ = nullptr;
  CookieTreeSharedWorkersNode* shared_workers_child_ = nullptr;
  CookieTreeCacheStoragesNode* cache_storages_child_ = nullptr;
  CookieTreeFlashLSONode* flash_lso_child_ = nullptr;
  CookieTreeMediaLicensesNode* media_licenses_child_ = nullptr;

  // The URL for which this node was initially created.
  GURL url_;

  std::string canonicalized_host_;

  DISALLOW_COPY_AND_ASSIGN(CookieTreeHostNode);
};

// CookiesTreeModel -----------------------------------------------------------
class CookiesTreeModel : public ui::TreeNodeModel<CookieTreeNode> {
 public:
  CookiesTreeModel(std::unique_ptr<LocalDataContainer> data_container,
                   ExtensionSpecialStoragePolicy* special_storage_policy);
  ~CookiesTreeModel() override;

  // Given a CanonicalCookie, return the ID of the message which should be
  // displayed in various ports' "Send for:" UI.
  static int GetSendForMessageID(const net::CanonicalCookie& cookie);

  // Because non-cookie nodes are fetched in a background thread, they are not
  // present at the time the Model is created. The Model then notifies its
  // observers for every item added from databases, local storage, and
  // appcache. We extend the Observer interface to add notifications before and
  // after these batch inserts.
  class Observer : public ui::TreeModelObserver {
   public:
    virtual void TreeModelBeginBatch(CookiesTreeModel* model) {}
    virtual void TreeModelEndBatch(CookiesTreeModel* model) {}
  };

  // This class defines the scope for batch updates. It can be created as a
  // local variable and the destructor will terminate the batch update, if one
  // has been started.
  class ScopedBatchUpdateNotifier {
   public:
    ScopedBatchUpdateNotifier(CookiesTreeModel* model,
                              CookieTreeNode* node);
    ~ScopedBatchUpdateNotifier();

    void StartBatchUpdate();

   private:
    CookiesTreeModel* model_;
    CookieTreeNode* node_;
    bool batch_in_progress_ = false;
  };

  // ui::TreeModel methods:
  // Returns the set of icons for the nodes in the tree. You only need override
  // this if you don't want to use the default folder icons.
  void GetIcons(std::vector<gfx::ImageSkia>* icons) override;

  // Returns the index of the icon to use for |node|. Return -1 to use the
  // default icon. The index is relative to the list of icons returned from
  // GetIcons.
  int GetIconIndex(ui::TreeModelNode* node) override;

  // CookiesTreeModel methods:
  void DeleteAllStoredObjects();

  // Deletes a specific node in the tree, identified by |cookie_node|, and its
  // subtree.
  void DeleteCookieNode(CookieTreeNode* cookie_node);

  // Filter the origins to only display matched results.
  void UpdateSearchResults(const base::string16& filter);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Returns the set of extensions which protect the data item represented by
  // this node from deletion.
  // Returns nullptr if the node doesn't represent a protected data item or the
  // special storage policy is nullptr.
  const extensions::ExtensionSet* ExtensionsProtectingNode(
      const CookieTreeNode& cookie_node);
#endif

  // Manages CookiesTreeModel::Observers. This will also call
  // TreeNodeModel::AddObserver so that it gets all the proper notifications.
  // Note that the converse is not true: simply adding a TreeModelObserver will
  // not get CookiesTreeModel::Observer notifications.
  virtual void AddCookiesTreeObserver(Observer* observer);
  virtual void RemoveCookiesTreeObserver(Observer* observer);

  // Methods that update the model based on the data retrieved by the browsing
  // data helpers.
  void PopulateAppCacheInfo(LocalDataContainer* container);
  void PopulateCookieInfo(LocalDataContainer* container);
  void PopulateDatabaseInfo(LocalDataContainer* container);
  void PopulateLocalStorageInfo(LocalDataContainer* container);
  void PopulateSessionStorageInfo(LocalDataContainer* container);
  void PopulateIndexedDBInfo(LocalDataContainer* container);
  void PopulateFileSystemInfo(LocalDataContainer* container);
  void PopulateQuotaInfo(LocalDataContainer* container);
  void PopulateServiceWorkerUsageInfo(LocalDataContainer* container);
  void PopulateSharedWorkerInfo(LocalDataContainer* container);
  void PopulateCacheStorageUsageInfo(LocalDataContainer* container);
  void PopulateFlashLSOInfo(LocalDataContainer* container);
  void PopulateMediaLicenseInfo(LocalDataContainer* container);

  BrowsingDataCookieHelper* GetCookieHelper(const std::string& app_id);
  LocalDataContainer* data_container() {
    return data_container_.get();
  }

  // Set the number of |batches_expected| this class should expect to receive.
  // If |reset| is true, then this is a new set of batches, but if false, then
  // this is a revised number (batches originally counted should no longer be
  // expected).
  void SetBatchExpectation(int batches_expected, bool reset);

  // Create CookiesTreeModel by profile info.
  static std::unique_ptr<CookiesTreeModel> CreateForProfile(Profile* profile);

 private:
  enum CookieIconIndex { COOKIE = 0, DATABASE = 1 };

  // Reset the counters for batches.
  void ResetBatches();

  // Record that one batch has been delivered.
  void RecordBatchSeen();

  // Record that one batch has begun processing. If this is the first batch then
  // observers will be notified that batch processing has started.
  void NotifyObserverBeginBatch();

  // Record that one batch has finished processing. If this is the last batch
  // then observers will be notified that batch processing has ended.
  void NotifyObserverEndBatch();

  // Notifies observers if expected batch count has been delievered and all
  // batches have finished processing.
  void MaybeNotifyBatchesEnded();

  void PopulateAppCacheInfoWithFilter(LocalDataContainer* container,
                                      ScopedBatchUpdateNotifier* notifier,
                                      const base::string16& filter);
  void PopulateCookieInfoWithFilter(LocalDataContainer* container,
                                    ScopedBatchUpdateNotifier* notifier,
                                    const base::string16& filter);
  void PopulateDatabaseInfoWithFilter(LocalDataContainer* container,
                                      ScopedBatchUpdateNotifier* notifier,
                                      const base::string16& filter);
  void PopulateLocalStorageInfoWithFilter(LocalDataContainer* container,
                                          ScopedBatchUpdateNotifier* notifier,
                                          const base::string16& filter);
  void PopulateSessionStorageInfoWithFilter(LocalDataContainer* container,
                                            ScopedBatchUpdateNotifier* notifier,
                                            const base::string16& filter);
  void PopulateIndexedDBInfoWithFilter(LocalDataContainer* container,
                                       ScopedBatchUpdateNotifier* notifier,
                                       const base::string16& filter);
  void PopulateFileSystemInfoWithFilter(LocalDataContainer* container,
                                        ScopedBatchUpdateNotifier* notifier,
                                        const base::string16& filter);
  void PopulateQuotaInfoWithFilter(LocalDataContainer* container,
                                   ScopedBatchUpdateNotifier* notifier,
                                   const base::string16& filter);
  void PopulateServiceWorkerUsageInfoWithFilter(
      LocalDataContainer* container,
      ScopedBatchUpdateNotifier* notifier,
      const base::string16& filter);
  void PopulateSharedWorkerInfoWithFilter(LocalDataContainer* container,
                                          ScopedBatchUpdateNotifier* notifier,
                                          const base::string16& filter);
  void PopulateCacheStorageUsageInfoWithFilter(
      LocalDataContainer* container,
      ScopedBatchUpdateNotifier* notifier,
      const base::string16& filter);
  void PopulateFlashLSOInfoWithFilter(LocalDataContainer* container,
                                      ScopedBatchUpdateNotifier* notifier,
                                      const base::string16& filter);
  void PopulateMediaLicenseInfoWithFilter(LocalDataContainer* container,
                                          ScopedBatchUpdateNotifier* notifier,
                                          const base::string16& filter);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The extension special storage policy; see ExtensionsProtectingNode() above.
  scoped_refptr<ExtensionSpecialStoragePolicy> special_storage_policy_;
#endif

  // Map of app ids to LocalDataContainer objects to use when retrieving
  // locally stored data.
  std::unique_ptr<LocalDataContainer> data_container_;

  // The CookiesTreeModel maintains a separate list of observers that are
  // specifically of the type CookiesTreeModel::Observer.
  base::ObserverList<Observer>::Unchecked cookies_observer_list_;

  // Keeps track of how many batches the consumer of this class says it is going
  // to send.
  int batches_expected_ = 0;

  // Keeps track of how many batches we've seen.
  int batches_seen_ = 0;

  // Counts how many batches have started already. If this is non-zero and lower
  // than batches_ended_, then this model is still batching updates.
  int batches_started_ = 0;

  // Counts how many batches have finished.
  int batches_ended_ = 0;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COOKIES_TREE_MODEL_H_
