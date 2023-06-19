// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COOKIES_TREE_MODEL_H_
#define CHROME_BROWSER_BROWSING_DATA_COOKIES_TREE_MODEL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/browsing_data/local_data_container.h"
#include "components/content_settings/core/common/content_settings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/models/tree_node_model.h"

class CookiesTreeModel;
class CookieTreeCacheStorageNode;
class CookieTreeCacheStoragesNode;
class CookieTreeCookieNode;
class CookieTreeCookiesNode;
class CookieTreeDatabaseNode;
class CookieTreeDatabasesNode;
class CookieTreeFileSystemNode;
class CookieTreeFileSystemsNode;
class CookieTreeHostNode;
class CookieTreeIndexedDBNode;
class CookieTreeIndexedDBsNode;
class CookieTreeLocalStorageNode;
class CookieTreeLocalStoragesNode;
class CookieTreeQuotaNode;
class CookieTreeServiceWorkerNode;
class CookieTreeServiceWorkersNode;
class CookieTreeSharedWorkerNode;
class CookieTreeSharedWorkersNode;
class CookieTreeSessionStorageNode;
class CookieTreeSessionStoragesNode;
class ExtensionSpecialStoragePolicy;
class Profile;

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
    };

    DetailedInfo();
    DetailedInfo(const DetailedInfo& other);
    ~DetailedInfo();

    DetailedInfo& Init(NodeType type);
    DetailedInfo& InitHost(const GURL& host);
    DetailedInfo& InitCookie(const net::CanonicalCookie* canonical_cookie);
    DetailedInfo& InitDatabase(
        const content::StorageUsageInfo* storage_usage_info);
    DetailedInfo& InitLocalStorage(
        const content::StorageUsageInfo* storage_usage_info);
    DetailedInfo& InitSessionStorage(
        const content::StorageUsageInfo* storage_usage_info);
    DetailedInfo& InitIndexedDB(
        const content::StorageUsageInfo* storage_usage_info);
    DetailedInfo& InitFileSystem(
        const browsing_data::FileSystemHelper::FileSystemInfo* file_system);
    DetailedInfo& InitQuota(const BrowsingDataQuotaHelper::QuotaInfo* quota);
    DetailedInfo& InitServiceWorker(
        const content::StorageUsageInfo* storage_usage_info);
    DetailedInfo& InitSharedWorker(
        const browsing_data::SharedWorkerHelper::SharedWorkerInfo*
            shared_worker);
    DetailedInfo& InitCacheStorage(
        const content::StorageUsageInfo* storage_usage_info);

    NodeType node_type;
    url::Origin origin;
    raw_ptr<const net::CanonicalCookie> cookie = nullptr;
    // Used for Database (WebSQL), IndexedDB, Service Worker, and
    // Cache Storage node types.
    raw_ptr<const content::StorageUsageInfo> usage_info = nullptr;
    raw_ptr<const browsing_data::FileSystemHelper::FileSystemInfo>
        file_system_info = nullptr;
    raw_ptr<const BrowsingDataQuotaHelper::QuotaInfo> quota_info = nullptr;
    raw_ptr<const browsing_data::SharedWorkerHelper::SharedWorkerInfo>
        shared_worker_info = nullptr;
  };

  CookieTreeNode() {}
  explicit CookieTreeNode(const std::u16string& title)
      : ui::TreeNode<CookieTreeNode>(title) {}

  CookieTreeNode(const CookieTreeNode&) = delete;
  CookieTreeNode& operator=(const CookieTreeNode&) = delete;

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
};

// CookieTreeRootNode ---------------------------------------------------------
// The node at the root of the CookieTree that gets inserted into the view.
class CookieTreeRootNode : public CookieTreeNode {
 public:
  explicit CookieTreeRootNode(CookiesTreeModel* model);

  CookieTreeRootNode(const CookieTreeRootNode&) = delete;
  CookieTreeRootNode& operator=(const CookieTreeRootNode&) = delete;

  ~CookieTreeRootNode() override;

  CookieTreeHostNode* GetOrCreateHostNode(const GURL& url);

  // CookieTreeNode methods:
  CookiesTreeModel* GetModel() const override;
  DetailedInfo GetDetailedInfo() const override;

 private:
  raw_ptr<CookiesTreeModel> model_;
};

// CookieTreeHostNode -------------------------------------------------------
class CookieTreeHostNode : public CookieTreeNode {
 public:
  // Returns the host node's title to use for a given URL.
  static std::u16string TitleForUrl(const GURL& url);

  explicit CookieTreeHostNode(const GURL& url);

  CookieTreeHostNode(const CookieTreeHostNode&) = delete;
  CookieTreeHostNode& operator=(const CookieTreeHostNode&) = delete;

  ~CookieTreeHostNode() override;

  // CookieTreeNode methods:
  DetailedInfo GetDetailedInfo() const override;

  // CookieTreeHostNode methods:
  CookieTreeCookiesNode* GetOrCreateCookiesNode();
  CookieTreeDatabasesNode* GetOrCreateDatabasesNode();
  CookieTreeLocalStoragesNode* GetOrCreateLocalStoragesNode();
  CookieTreeSessionStoragesNode* GetOrCreateSessionStoragesNode();
  CookieTreeIndexedDBsNode* GetOrCreateIndexedDBsNode();
  CookieTreeFileSystemsNode* GetOrCreateFileSystemsNode();
  CookieTreeServiceWorkersNode* GetOrCreateServiceWorkersNode();
  CookieTreeSharedWorkersNode* GetOrCreateSharedWorkersNode();
  CookieTreeCacheStoragesNode* GetOrCreateCacheStoragesNode();
  CookieTreeQuotaNode* UpdateOrCreateQuotaNode(
      std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info);

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
  // Pointers to the cookies, databases, local and session storage nodes.
  // When we build up the tree we need to quickly get a reference to
  // the COOKIES node to add children. Checking each child and interrogating
  // them to see if they are a COOKIES, DATABASES, etc node seems
  // less preferable than storing an extra pointer per origin.
  raw_ptr<CookieTreeCookiesNode, DanglingUntriaged> cookies_child_ = nullptr;
  raw_ptr<CookieTreeDatabasesNode, DanglingUntriaged> databases_child_ =
      nullptr;
  raw_ptr<CookieTreeLocalStoragesNode, DanglingUntriaged>
      local_storages_child_ = nullptr;
  raw_ptr<CookieTreeSessionStoragesNode, DanglingUntriaged>
      session_storages_child_ = nullptr;
  raw_ptr<CookieTreeIndexedDBsNode, DanglingUntriaged> indexed_dbs_child_ =
      nullptr;
  raw_ptr<CookieTreeFileSystemsNode> file_systems_child_ = nullptr;
  raw_ptr<CookieTreeQuotaNode, DanglingUntriaged> quota_child_ = nullptr;
  raw_ptr<CookieTreeServiceWorkersNode> service_workers_child_ = nullptr;
  raw_ptr<CookieTreeSharedWorkersNode> shared_workers_child_ = nullptr;
  raw_ptr<CookieTreeCacheStoragesNode> cache_storages_child_ = nullptr;

  // The URL for which this node was initially created.
  GURL url_;

  std::string canonicalized_host_;
};

// CookiesTreeModel -----------------------------------------------------------
class CookiesTreeModel : public ui::TreeNodeModel<CookieTreeNode> {
 public:
  // Create CookiesTreeModel by profile info.
  // DEPRECATED(crbug.com/1271155): The cookies tree model is slowly being
  // deprecated, during this process the semantics of the model are nuanced
  // w.r.t partitioned storage, and should not be used in new locations.
  static std::unique_ptr<CookiesTreeModel> CreateForProfileDeprecated(
      Profile* profile);

  CookiesTreeModel(std::unique_ptr<LocalDataContainer> data_container,
                   ExtensionSpecialStoragePolicy* special_storage_policy);

  ~CookiesTreeModel() override;

  // Given a CanonicalCookie, return the ID of the message which should be
  // displayed in various ports' "Send for:" UI.
  static int GetSendForMessageID(const net::CanonicalCookie& cookie);

  // Because non-cookie nodes are fetched in a background thread, they are not
  // present at the time the Model is created. The Model then notifies its
  // observers for every item added from databases and local storage.
  // We extend the Observer interface to add notifications before and
  // after these batch inserts.
  // DEPRECATED(crbug.com/1271155): The cookies tree model is slowly being
  // deprecated, during this process the semantics of the model are nuanced
  // w.r.t sync vs async operations, and should not be used in new locations.
  // Batch operations which fetch are always sync if all helpers are canned
  // (in-memory) versions *and* the CannedLocalStorageHelper is configured not
  // to check for empty local storages. Batch fetch operations are always async
  // otherwise.
  class Observer : public ui::TreeModelObserver {
   public:
    virtual void TreeModelBeginBatchDeprecated(CookiesTreeModel* model) {}
    virtual void TreeModelEndBatchDeprecated(CookiesTreeModel* model) {}
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
    raw_ptr<CookiesTreeModel> model_;
    raw_ptr<CookieTreeNode> node_;
    bool batch_in_progress_ = false;
  };

  // ui::TreeModel:
  void GetIcons(std::vector<ui::ImageModel>* icons) override;
  absl::optional<size_t> GetIconIndex(ui::TreeModelNode* node) override;

  void DeleteAllStoredObjects();

  // Deletes a specific node in the tree, identified by |cookie_node|, and its
  // subtree.
  void DeleteCookieNode(CookieTreeNode* cookie_node);

  // Filter the origins to only display matched results.
  void UpdateSearchResults(const std::u16string& filter);

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

  LocalDataContainer* data_container() {
    return data_container_.get();
  }

  // Set the number of |batches_expected| this class should expect to receive.
  // If |reset| is true, then this is a new set of batches, but if false, then
  // this is a revised number (batches originally counted should no longer be
  // expected).
  void SetBatchExpectation(int batches_expected, bool reset);

  static browsing_data::CookieHelper::IsDeletionDisabledCallback
  GetCookieDeletionDisabledCallback(Profile* profile);

 private:
  FRIEND_TEST_ALL_PREFIXES(CookiesTreeModelBrowserTest, BatchesFinishSync);

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

  void PopulateCookieInfoWithFilter(LocalDataContainer* container,
                                    ScopedBatchUpdateNotifier* notifier,
                                    const std::u16string& filter);
  void PopulateDatabaseInfoWithFilter(LocalDataContainer* container,
                                      ScopedBatchUpdateNotifier* notifier,
                                      const std::u16string& filter);
  void PopulateLocalStorageInfoWithFilter(LocalDataContainer* container,
                                          ScopedBatchUpdateNotifier* notifier,
                                          const std::u16string& filter);
  void PopulateSessionStorageInfoWithFilter(LocalDataContainer* container,
                                            ScopedBatchUpdateNotifier* notifier,
                                            const std::u16string& filter);
  void PopulateIndexedDBInfoWithFilter(LocalDataContainer* container,
                                       ScopedBatchUpdateNotifier* notifier,
                                       const std::u16string& filter);
  void PopulateFileSystemInfoWithFilter(LocalDataContainer* container,
                                        ScopedBatchUpdateNotifier* notifier,
                                        const std::u16string& filter);
  void PopulateQuotaInfoWithFilter(LocalDataContainer* container,
                                   ScopedBatchUpdateNotifier* notifier,
                                   const std::u16string& filter);
  void PopulateServiceWorkerUsageInfoWithFilter(
      LocalDataContainer* container,
      ScopedBatchUpdateNotifier* notifier,
      const std::u16string& filter);
  void PopulateSharedWorkerInfoWithFilter(LocalDataContainer* container,
                                          ScopedBatchUpdateNotifier* notifier,
                                          const std::u16string& filter);
  void PopulateCacheStorageUsageInfoWithFilter(
      LocalDataContainer* container,
      ScopedBatchUpdateNotifier* notifier,
      const std::u16string& filter);

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
