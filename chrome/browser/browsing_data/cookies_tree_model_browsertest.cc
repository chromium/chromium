// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/browsing_data/core/features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

constexpr char kTestHostname[] = "a.test";

// Calls the accessStorage javascript function and awaits its completion for
// each frame in the active web contents for |browser|.
void EnsurePageAccessedStorage(content::WebContents* web_contents) {
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [](content::RenderFrameHost* frame) {
        EXPECT_TRUE(
            content::EvalJs(frame,
                            "(async () => { return await accessStorage();})()")
                .value.GetBool());
      });
}

std::vector<CookieTreeNode*> GetAllChildNodes(CookieTreeNode* node) {
  std::vector<CookieTreeNode*> nodes;
  for (const auto& child : node->children()) {
    auto child_nodes = GetAllChildNodes(child.get());
    nodes.insert(nodes.end(), child_nodes.begin(), child_nodes.end());
  }
  nodes.push_back(node);
  return nodes;
}

std::map<CookieTreeNode::DetailedInfo::NodeType, int> GetNodeTypeCounts(
    CookiesTreeModel* model) {
  auto nodes = GetAllChildNodes(model->GetRoot());

  std::map<CookieTreeNode::DetailedInfo::NodeType, int> node_counts_map;
  for (auto* node : nodes)
    node_counts_map[node->GetDetailedInfo().node_type]++;

  return node_counts_map;
}

}  // namespace

class CookiesTreeObserver : public CookiesTreeModel::Observer {
 public:
  void AwaitTreeModelEndBatch() {
    run_loop = std::make_unique<base::RunLoop>();
    run_loop->Run();
  }

  void TreeModelEndBatchDeprecated(CookiesTreeModel* model) override {
    run_loop->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop;
};

class CookiesTreeModelBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    if (base::FeatureList::IsEnabled(
            browsing_data::features::kDeprecateCookiesTreeModel)) {
      GTEST_SKIP() << "kDeprecateCookiesTreeModel is enabled skipping "
                      "CookiesTreeModel tests";
    }
    InitFeatures();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    test_server_.ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(test_server_.Start());
  }

  void AccessStorage() {
    ASSERT_TRUE(content::NavigateToURL(
        chrome_test_utils::GetActiveWebContents(this), storage_accessor_url()));
    base::RunLoop().RunUntilIdle();
    EnsurePageAccessedStorage(chrome_test_utils::GetActiveWebContents(this));
  }

  GURL storage_accessor_url() {
    auto host_port_pair =
        net::HostPortPair::FromURL(test_server_.GetURL(kTestHostname, "/"));
    base::StringPairs replacement_text = {
        {"REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()}};
    auto replaced_path = net::test_server::GetFilePathWithReplacements(
        "/browsing_data/storage_accessor.html", replacement_text);
    return test_server_.GetURL("a.test", replaced_path);
  }

  std::unique_ptr<CookiesTreeModel> CreateModelForStoragePartitionConfig(
      Profile* profile,
      const content::StoragePartitionConfig& storage_partition_config) {
    auto local_data_container = LocalDataContainer::CreateFromStoragePartition(
        profile->GetStoragePartition(storage_partition_config),
        CookiesTreeModel::GetCookieDeletionDisabledCallback(profile));
    auto tree_model = std::make_unique<CookiesTreeModel>(
        std::move(local_data_container), /*special_storage_policy=*/nullptr);
    CookiesTreeObserver observer;
    tree_model->AddCookiesTreeObserver(&observer);
    observer.AwaitTreeModelEndBatch();
    return tree_model;
  }

  virtual void InitFeatures() {
    feature_list()->InitWithFeatures(
        // WebSQL is disabled by default as of M119 (crbug/695592).
        // Enable feature in tests during deprecation trial and enterprise
        // policy support.
        {blink::features::kWebSQLAccess},
        {net::features::kThirdPartyStoragePartitioning});
  }

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

 protected:
  net::EmbeddedTestServer test_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CookiesTreeModelBrowserTest, NoQuotaStorage) {
  AccessStorage();

  auto tree_model = CookiesTreeModel::CreateForProfileDeprecated(
      chrome_test_utils::GetProfile(this));
  CookiesTreeObserver observer;
  tree_model->AddCookiesTreeObserver(&observer);
  observer.AwaitTreeModelEndBatch();

  bool is_migrate_storage_to_bdm_enabled = base::FeatureList::IsEnabled(
      browsing_data::features::kMigrateStorageToBDM);

  auto expected_size = is_migrate_storage_to_bdm_enabled ? 0 : 1;

  // Quota storage has been accessed, but should not be present in the tree.
  EXPECT_EQ(is_migrate_storage_to_bdm_enabled ? 5u : 17u,
            tree_model->GetRoot()->GetTotalNodeCount());
  auto node_counts = GetNodeTypeCounts(tree_model.get());
  EXPECT_EQ(is_migrate_storage_to_bdm_enabled ? 4u : 16u, node_counts.size());
  EXPECT_EQ(0, node_counts[CookieTreeNode::DetailedInfo::TYPE_QUOTA]);

  EXPECT_EQ(1, node_counts[CookieTreeNode::DetailedInfo::TYPE_ROOT]);
  EXPECT_EQ(1, node_counts[CookieTreeNode::DetailedInfo::TYPE_HOST]);
  EXPECT_EQ(2, node_counts[CookieTreeNode::DetailedInfo::TYPE_COOKIE]);
  EXPECT_EQ(1, node_counts[CookieTreeNode::DetailedInfo::TYPE_COOKIES]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_DATABASE]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_DATABASES]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGES]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_INDEXED_DBS]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEMS]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKERS]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGES]);
}

IN_PROC_BROWSER_TEST_F(CookiesTreeModelBrowserTest, BatchesFinishSync) {
  // Confirm that when all helpers fetch functions return synchronously, that
  // the model has received all expected batches.
  auto shared_objects = browsing_data::LocalSharedObjectsContainer(
      chrome_test_utils::GetProfile(this)->GetDefaultStoragePartition(),
      /*ignore_empty_localstorage=*/false, {}, base::NullCallback());
  auto local_data_container =
      LocalDataContainer::CreateFromLocalSharedObjectsContainer(shared_objects);

  // Ideally we could observe TreeModelEndBatch, however in the sync case, the
  // batch will finish during the models constructor, before we can attach an
  // observer.
  auto cookies_model = std::make_unique<CookiesTreeModel>(
      std::move(local_data_container), /*special_storage_policy=*/nullptr);

  // The model will clear all batch information when the batch is completed, so
  // all 0's here implies any previous batches have been completed, and the
  // model is not awaiting any helper to finish.
  EXPECT_EQ(cookies_model->batches_seen_, 0);
  EXPECT_EQ(cookies_model->batches_started_, 0);
  EXPECT_EQ(cookies_model->batches_expected_, 0);
  EXPECT_EQ(cookies_model->batches_seen_, 0);
}

class ScopedNonDefaultStoragePartitionContentBrowserClient
    : public ChromeContentBrowserClient {
 public:
  ScopedNonDefaultStoragePartitionContentBrowserClient(
      const char* target_host,
      const content::StoragePartitionConfig& storage_partition_config)
      : target_host_(target_host),
        storage_partition_config_(storage_partition_config),
        original_client_(content::SetBrowserClientForTesting(this)) {}

  ~ScopedNonDefaultStoragePartitionContentBrowserClient() override {
    CHECK(SetBrowserClientForTesting(original_client_) == this);
  }

  content::StoragePartitionConfig GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override {
    if (site.host() == target_host_) {
      return storage_partition_config_;
    }
    return content::StoragePartitionConfig::CreateDefault(browser_context);
  }

 private:
  const char* target_host_;
  content::StoragePartitionConfig storage_partition_config_;
  raw_ptr<content::ContentBrowserClient> original_client_;
};

IN_PROC_BROWSER_TEST_F(CookiesTreeModelBrowserTest,
                       NonDefaultStoragePartition) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  auto non_default_storage_partition_config =
      content::StoragePartitionConfig::Create(profile,
                                              /*partition_domain=*/"nondefault",
                                              /*partition_name=*/"",
                                              /*in_memory=*/false);
  ScopedNonDefaultStoragePartitionContentBrowserClient client(
      kTestHostname, non_default_storage_partition_config);

  // Check that the non-default StoragePartition starts out empty.
  auto tree_model = CreateModelForStoragePartitionConfig(
      profile, non_default_storage_partition_config);
  EXPECT_EQ(1u, tree_model->GetRoot()->GetTotalNodeCount());

  AccessStorage();

  // Check that the non-default StoragePartition now has content.
  tree_model = CreateModelForStoragePartitionConfig(
      profile, non_default_storage_partition_config);

  auto node_counts = GetNodeTypeCounts(tree_model.get());
  EXPECT_LT(0, node_counts[CookieTreeNode::DetailedInfo::TYPE_ROOT]);
  EXPECT_LT(0, node_counts[CookieTreeNode::DetailedInfo::TYPE_HOST]);
  EXPECT_LT(0, node_counts[CookieTreeNode::DetailedInfo::TYPE_COOKIE]);
  EXPECT_LT(0, node_counts[CookieTreeNode::DetailedInfo::TYPE_COOKIES]);
  if (base::FeatureList::IsEnabled(
          browsing_data::features::kMigrateStorageToBDM)) {
    EXPECT_EQ(0, node_counts[CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE]);
    EXPECT_EQ(0,
              node_counts[CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGES]);
  } else {
    EXPECT_LT(0, node_counts[CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE]);
    EXPECT_LT(0,
              node_counts[CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGES]);
  }
  EXPECT_EQ(0, node_counts[CookieTreeNode::DetailedInfo::TYPE_QUOTA]);

  // Check that the default StoragePartition is empty.
  tree_model = CreateModelForStoragePartitionConfig(
      profile, content::StoragePartitionConfig::CreateDefault(profile));

  EXPECT_EQ(1u, tree_model->GetRoot()->GetTotalNodeCount());
  node_counts = GetNodeTypeCounts(tree_model.get());
  EXPECT_EQ(1, node_counts[CookieTreeNode::DetailedInfo::TYPE_ROOT]);
}

class CookiesTreeModelBrowserTestQuotaOnly
    : public CookiesTreeModelBrowserTest {
 public:
  void InitFeatures() override {
    feature_list()->InitAndEnableFeature(
        net::features::kThirdPartyStoragePartitioning);
  }
};

IN_PROC_BROWSER_TEST_F(CookiesTreeModelBrowserTestQuotaOnly, QuotaStorageOnly) {
  AccessStorage();

  auto tree_model = CookiesTreeModel::CreateForProfileDeprecated(
      chrome_test_utils::GetProfile(this));
  CookiesTreeObserver observer;
  tree_model->AddCookiesTreeObserver(&observer);
  observer.AwaitTreeModelEndBatch();

  bool is_migrate_storage_to_bdm_enabled = base::FeatureList::IsEnabled(
      browsing_data::features::kMigrateStorageToBDM);
  // Quota storage has been accessed, only quota nodes should be present for
  // quota managed storage types.
  EXPECT_EQ(is_migrate_storage_to_bdm_enabled ? 5u : 8u,
            tree_model->GetRoot()->GetTotalNodeCount());

  auto node_counts = GetNodeTypeCounts(tree_model.get());
  auto expected_size = is_migrate_storage_to_bdm_enabled ? 0 : 1;
  EXPECT_EQ(is_migrate_storage_to_bdm_enabled ? 4u : 7u, node_counts.size());
  EXPECT_EQ(1, node_counts[CookieTreeNode::DetailedInfo::TYPE_ROOT]);
  EXPECT_EQ(1, node_counts[CookieTreeNode::DetailedInfo::TYPE_HOST]);
  EXPECT_EQ(2, node_counts[CookieTreeNode::DetailedInfo::TYPE_COOKIE]);
  EXPECT_EQ(1, node_counts[CookieTreeNode::DetailedInfo::TYPE_COOKIES]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGES]);
  EXPECT_EQ(expected_size,
            node_counts[CookieTreeNode::DetailedInfo::TYPE_QUOTA]);
}
