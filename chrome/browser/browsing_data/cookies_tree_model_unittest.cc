// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/cookies_tree_model.h"

#include <memory>
#include <numeric>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mock_settings_observer.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/mock_browsing_data_quota_helper.h"
#include "components/browsing_data/content/mock_cookie_helper.h"
#include "components/browsing_data/content/mock_local_storage_helper.h"
#include "components/browsing_data/core/features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#endif

using content::BrowserThread;
using ::testing::_;

namespace {

enum TestNodeHostIndex {
  kFoo1 = 0,
  kFoo2 = 1,
  kFoo3,
  kHost1,
  kHost2,
  kQuotahost1,
  kQuotahost2
};

class CookiesTreeModelTest : public testing::Test {
 public:
  ~CookiesTreeModelTest() override {
    // Avoid memory leaks.
#if BUILDFLAG(ENABLE_EXTENSIONS)
    special_storage_policy_ = nullptr;
#endif
    // TODO(arthursonzogni): Consider removing this line, or at least explain
    // why it is needed.
    base::RunLoop().RunUntilIdle();
    profile_.reset();
    // TODO(arthursonzogni): Consider removing this line, or at least explain
    // why it is needed.
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    if (base::FeatureList::IsEnabled(
            browsing_data::features::kDeprecateCookiesTreeModel)) {
      GTEST_SKIP() << "kDeprecateCookiesTreeModel is enabled skipping "
                      "CookiesTreeModel tests";
    }

    profile_ = std::make_unique<TestingProfile>();
    auto* storage_partition = profile_->GetDefaultStoragePartition();
    mock_browsing_data_cookie_helper_ =
        base::MakeRefCounted<browsing_data::MockCookieHelper>(
            storage_partition);
    mock_browsing_data_local_storage_helper_ =
        base::MakeRefCounted<browsing_data::MockLocalStorageHelper>(
            storage_partition);
    mock_browsing_data_session_storage_helper_ =
        base::MakeRefCounted<browsing_data::MockLocalStorageHelper>(
            storage_partition);
    mock_browsing_data_quota_helper_ =
        base::MakeRefCounted<MockBrowsingDataQuotaHelper>();

    const char kExtensionScheme[] = "extensionscheme";
    auto cookie_settings =
        base::MakeRefCounted<content_settings::CookieSettings>(
            HostContentSettingsMapFactory::GetForProfile(profile_.get()),
            profile_->GetPrefs(),
            TrackingProtectionSettingsFactory::GetForProfile(profile_.get()),
            profile_->IsIncognitoProfile(), kExtensionScheme);
#if BUILDFLAG(ENABLE_EXTENSIONS)
    special_storage_policy_ =
        base::MakeRefCounted<ExtensionSpecialStoragePolicy>(
            cookie_settings.get());
#endif
  }

  void TearDown() override {
    mock_browsing_data_quota_helper_ = nullptr;
    mock_browsing_data_session_storage_helper_ = nullptr;
    mock_browsing_data_local_storage_helper_ = nullptr;
    mock_browsing_data_cookie_helper_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<CookiesTreeModel> CreateCookiesTreeModelWithInitialSample() {
    auto container = std::make_unique<LocalDataContainer>(
        mock_browsing_data_cookie_helper_,
        mock_browsing_data_local_storage_helper_,
        mock_browsing_data_session_storage_helper_,
        mock_browsing_data_quota_helper_);

    auto cookies_model = std::make_unique<CookiesTreeModel>(
        std::move(container), special_storage_policy());
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo1"), "A=1");
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo2"), "B=1");
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo3"), "C=1");
    mock_browsing_data_cookie_helper_->Notify();
    mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
    mock_browsing_data_local_storage_helper_->Notify();
    mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
    mock_browsing_data_session_storage_helper_->Notify();
    mock_browsing_data_quota_helper_->AddQuotaSamples();
    mock_browsing_data_quota_helper_->Notify();

    {
      SCOPED_TRACE(
          "Initial State 3 cookies, 2 local storages, 2 session storages, "
          "2 quotas");
      // 24 because there's the root, then
      // foo1 -> cookies -> a,
      // foo2 -> cookies -> b,
      // foo3 -> cookies -> c,
      // host1 -> localstorage -> http://host1:1/,
      //       -> sessionstorage -> http://host1:1/,
      // host2 -> localstorage -> http://host2:2/.
      //       -> sessionstorage -> http://host2:2/,
      // quotahost1 -> quotahost1,
      // quotahost2 -> quotahost2
      EXPECT_EQ(24u, cookies_model->GetRoot()->GetTotalNodeCount());
      EXPECT_EQ("A,B,C", GetDisplayedCookies(cookies_model.get()));
      EXPECT_EQ("http://host1:1/,http://host2:2/",
                GetDisplayedLocalStorages(cookies_model.get()));
      EXPECT_EQ("http://host1:1/,http://host2:2/",
                GetDisplayedSessionStorages(cookies_model.get()));
      EXPECT_EQ("quotahost1,quotahost2",
                GetDisplayedQuotas(cookies_model.get()));
    }
    return cookies_model;
  }

  // Checks that, when setting content settings for host nodes in the
  // cookie tree, the content settings are applied to the expected URL.
  void CheckContentSettingsUrlForHostNodes(
      const CookieTreeNode* node,
      CookieTreeNode::DetailedInfo::NodeType node_type,
      content_settings::CookieSettings* cookie_settings,
      const GURL& expected_url) {
    for (const auto& child : node->children()) {
      CheckContentSettingsUrlForHostNodes(child.get(),
                                          child->GetDetailedInfo().node_type,
                                          cookie_settings, expected_url);
    }

    ASSERT_EQ(node_type, node->GetDetailedInfo().node_type);

    if (node_type == CookieTreeNode::DetailedInfo::TYPE_HOST) {
      const CookieTreeHostNode* host =
          static_cast<const CookieTreeHostNode*>(node);

      if (expected_url.SchemeIsFile()) {
        EXPECT_FALSE(host->CanCreateContentException());
      } else {
        cookie_settings->ResetCookieSetting(expected_url);
        EXPECT_FALSE(cookie_settings->IsCookieSessionOnly(expected_url));

        host->CreateContentException(cookie_settings,
                                     CONTENT_SETTING_SESSION_ONLY);
        EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(expected_url));
      }
    }
  }

  std::string GetNodesOfChildren(
      const CookieTreeNode* node,
      CookieTreeNode::DetailedInfo::NodeType node_type) {
    if (!node->children().empty()) {
      std::string retval;
      for (const auto& child : node->children())
        retval += GetNodesOfChildren(child.get(), node_type);
      return retval;
    }

    if (node->GetDetailedInfo().node_type != node_type)
      return std::string();

    // TODO: GetURL().spec() is used instead of Serialize() for backwards
    // compatibility with tests. The tests should be updated once all
    // appropriate parts have been migrated to url::Origin.
    switch (node_type) {
      case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
        return node->GetDetailedInfo().cookie->Name() + ",";
      case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE:
      case CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE:
        return node->GetDetailedInfo()
                   .usage_info->storage_key.origin()
                   .GetURL()
                   .spec() +
               ",";
      case CookieTreeNode::DetailedInfo::TYPE_QUOTA:
        return node->GetDetailedInfo().quota_info->storage_key.origin().host() +
               ",";
      default:
        return std::string();
    }
  }

  // Get the nodes names displayed in the view (if we had one) in the order
  // they are displayed, as a comma seperated string.
  // Ex: EXPECT_STREQ("X,Y", GetDisplayedNodes(cookies_view, type).c_str());
  std::string GetDisplayedNodes(CookiesTreeModel* cookies_model,
                                CookieTreeNode::DetailedInfo::NodeType type) {
    CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(
        cookies_model->GetRoot());
    std::string retval = GetNodesOfChildren(root, type);
    if (!retval.empty() && retval.back() == ',')
      retval.erase(retval.length() - 1);
    return retval;
  }

  std::string GetDisplayedCookies(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_COOKIE);
  }

  std::string GetDisplayedLocalStorages(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE);
  }

  std::string GetDisplayedSessionStorages(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(
        cookies_model, CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE);
  }

  std::string GetDisplayedQuotas(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_QUOTA);
  }

  // Do not call on the root.
  void DeleteStoredObjects(CookieTreeNode* node) {
    node->DeleteStoredObjects();
    CookieTreeNode* parent_node = node->parent();
    DCHECK(parent_node);
    parent_node->GetModel()->Remove(parent_node, node);
  }

 protected:
  ExtensionSpecialStoragePolicy* special_storage_policy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    return special_storage_policy_.get();
#else
    return nullptr;
#endif
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  scoped_refptr<browsing_data::MockCookieHelper>
      mock_browsing_data_cookie_helper_;
  scoped_refptr<browsing_data::MockLocalStorageHelper>
      mock_browsing_data_local_storage_helper_;
  scoped_refptr<browsing_data::MockLocalStorageHelper>
      mock_browsing_data_session_storage_helper_;
  scoped_refptr<MockBrowsingDataQuotaHelper>
      mock_browsing_data_quota_helper_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<ExtensionSpecialStoragePolicy> special_storage_policy_;
#endif
};

TEST_F(CookiesTreeModelTest, RemoveAll) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  // Reset the selection of the first row.
  {
    SCOPED_TRACE("Before removing");
    EXPECT_EQ("A,B,C",
              GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2",
              GetDisplayedQuotas(cookies_model.get()));
  }

  mock_browsing_data_cookie_helper_->Reset();
  mock_browsing_data_local_storage_helper_->Reset();
  mock_browsing_data_session_storage_helper_->Reset();

  cookies_model->DeleteAllStoredObjects();

  // Make sure the nodes are also deleted from the model's cache.
  // http://crbug.com/43249
  cookies_model->UpdateSearchResults(std::u16string());

  {
    // 2 nodes - root and app
    SCOPED_TRACE("After removing");
    EXPECT_EQ(1u, cookies_model->GetRoot()->GetTotalNodeCount());
    EXPECT_EQ(0u, cookies_model->GetRoot()->children().size());
    EXPECT_EQ(std::string(), GetDisplayedCookies(cookies_model.get()));
    EXPECT_TRUE(mock_browsing_data_cookie_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_local_storage_helper_->AllDeleted());
    EXPECT_FALSE(mock_browsing_data_session_storage_helper_->AllDeleted());
  }
}

TEST_F(CookiesTreeModelTest, Remove) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  // Children start out arranged as follows:
  //
  // 0. `foo1`
  // 1. `foo2`
  // 2. `foo3`
  // 3. `host1`
  // 4. `host2`
  // 5. `quotahost1`
  // 6. `quotahost2`
  //
  // Here, we'll remove them one by one, starting from the end, and
  // check that the state makes sense. Initially there are 28 total nodes.

  // quotahost1 -> quotahost2 (2 objects)
  DeleteStoredObjects(cookies_model->GetRoot()
                          ->children()[TestNodeHostIndex::kQuotahost2]
                          .get());
  {
    SCOPED_TRACE("`quotahost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("quotahost1",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ(22u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // quotahost1 -> quotahost1 (2 objects)
  DeleteStoredObjects(cookies_model->GetRoot()
                          ->children()[TestNodeHostIndex::kQuotahost1]
                          .get());
  {
    SCOPED_TRACE("`quotahost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ(20u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // host2 -> localstorage -> http://host2:2/,
  //       -> sessionstorage -> http://host2:2/ (5 objects)
  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[TestNodeHostIndex::kHost2].get());
  {
    SCOPED_TRACE("`host2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://host1:1/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ(15u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // host1 -> localstorage -> http://host1:1/,
  //       -> sessionstorage -> http://host1:1/ (5 objects)
  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[TestNodeHostIndex::kHost1].get());
  {
    SCOPED_TRACE("`host1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ(10u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // foo3 -> cookies -> c (3 objects)
  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[TestNodeHostIndex::kFoo3].get());
  {
    SCOPED_TRACE("`foo3` removed.");
    EXPECT_STREQ("A,B", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ(7u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // foo2 -> cookies -> b (3 objects)
  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[TestNodeHostIndex::kFoo2].get());
  {
    SCOPED_TRACE("`foo2` removed.");
    EXPECT_STREQ("A", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ(4u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // foo1 -> cookies -> a (3 objects)
  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[TestNodeHostIndex::kFoo1].get());
  {
    SCOPED_TRACE("`foo1` removed.");
    EXPECT_STREQ("", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ(1u, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookiesNode) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  DeleteStoredObjects(cookies_model->GetRoot()
                          ->children()[TestNodeHostIndex::kFoo1]
                          ->children()[0]
                          .get());
  {
    SCOPED_TRACE("First cookies origin removed");
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    // 22 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted.
    EXPECT_EQ(22u, cookies_model->GetRoot()->GetTotalNodeCount());
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
  }

  DeleteStoredObjects(cookies_model->GetRoot()
                          ->children()[TestNodeHostIndex::kHost1]
                          ->children()[0]
                          .get());
  {
    SCOPED_TRACE("First local storage origin removed");
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ(20u, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookieNode) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  DeleteStoredObjects(cookies_model->GetRoot()
                          ->children()[TestNodeHostIndex::kFoo2]
                          ->children()[0]
                          .get());
  {
    SCOPED_TRACE("Second origin COOKIES node removed");
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    // 22 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted.
    EXPECT_EQ(22u, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(cookies_model->GetRoot()
                          ->children()[TestNodeHostIndex::kHost1]
                          ->children()[0]
                          .get());
  {
    SCOPED_TRACE("First local storage origin removed");
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ(20u, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNode) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_quota_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->Notify();
  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_session_storage_helper_->Notify();
  mock_browsing_data_quota_helper_->AddQuotaSamples();
  mock_browsing_data_quota_helper_->Notify();

  {
    SCOPED_TRACE(
        "Initial State 4 cookies, 2 local storages, "
        "2 session storages, 2 quotas.");
    // 25 because there's the root, then
    // foo1 -> cookies -> a,
    // foo2 -> cookies -> b,
    // foo3 -> cookies -> c,d
    // host1 -> localstorage -> http://host1:1/,
    //       -> sessionstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/,
    //       -> sessionstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/,
    // quotahost1 -> quotahost1,
    // quotahost2 -> quotahost2
    EXPECT_EQ(25u, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
  }
  DeleteStoredObjects(
      cookies_model.GetRoot()->children()[TestNodeHostIndex::kFoo3].get());
  {
    SCOPED_TRACE("Third cookie origin removed");
    EXPECT_STREQ("A,B", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ(21u, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNodeOf3) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_quota_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "E=1");
  mock_browsing_data_cookie_helper_->Notify();
  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_session_storage_helper_->Notify();
  mock_browsing_data_quota_helper_->AddQuotaSamples();
  mock_browsing_data_quota_helper_->Notify();

  {
    SCOPED_TRACE(
        "Initial State 5 cookies, 2 local storages, "
        "2 session storages, 2 quotas.");
    // 32 because there's the root, then
    // foo1 -> cookies -> a,
    // foo2 -> cookies -> b,
    // foo3 -> cookies -> c,d,e
    // host1 -> localstorage -> http://host1:1/,
    //       -> sessionstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/,
    //       -> sessionstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/,
    // quotahost1 -> quotahost1,
    // quotahost2 -> quotahost2.
    EXPECT_EQ(26u, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
  }
  DeleteStoredObjects(cookies_model.GetRoot()
                          ->children()[2]
                          ->children()[0]
                          ->children()[1]
                          .get());
  {
    SCOPED_TRACE("Middle cookie in third cookie origin removed");
    EXPECT_STREQ("A,B,C,E", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ(25u, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
  }
}

TEST_F(CookiesTreeModelTest, RemoveSecondOrigin) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_quota_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "E=1");
  mock_browsing_data_cookie_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 5 cookies");
    // 12 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c,d,e
    EXPECT_EQ(12u, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
  }
  DeleteStoredObjects(
      cookies_model.GetRoot()->children()[TestNodeHostIndex::kFoo2].get());
  {
    SCOPED_TRACE("Second origin removed");
    EXPECT_STREQ("A,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
    // Left with root -> foo1 -> cookies -> a, foo3 -> cookies -> c,d,e
    EXPECT_EQ(9u, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, OriginOrdering) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_quota_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://a.foo2.com"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2.com"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://b.foo1.com"), "C=1");
  // Leading dot on the foo4
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://foo4.com"), "D=1; domain=.foo4.com; path=/;");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://a.foo1.com"), "E=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1.com"), "F=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3.com"), "G=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo4.com"), "H=1");
  mock_browsing_data_cookie_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 8 cookies");
    EXPECT_EQ(23u, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("F,E,C,B,A,G,D,H",
        GetDisplayedCookies(&cookies_model).c_str());
  }
  // Delete "E"
  DeleteStoredObjects(
      cookies_model.GetRoot()->children()[TestNodeHostIndex::kFoo2].get());
  {
    EXPECT_STREQ("F,C,B,A,G,D,H", GetDisplayedCookies(&cookies_model).c_str());
  }
}

TEST_F(CookiesTreeModelTest, ContentSettings) {
  GURL host("http://xyz.com/");
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_quota_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->AddCookieSamples(host, "A=1");
  mock_browsing_data_cookie_helper_->Notify();

  TestingProfile profile;
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(&profile).get();
  MockSettingsObserver observer(content_settings);

  CookieTreeRootNode* root =
      static_cast<CookieTreeRootNode*>(cookies_model.GetRoot());
  CookieTreeHostNode* origin =
      root->GetOrCreateHostNode(host);

  EXPECT_EQ(1u, origin->children().size());
  EXPECT_TRUE(origin->CanCreateContentException());
  EXPECT_CALL(observer, OnContentSettingsChanged(
                            content_settings, ContentSettingsType::COOKIES,
                            false, ContentSettingsPattern::FromURL(host),
                            ContentSettingsPattern::Wildcard(), false))
      .Times(1);
  origin->CreateContentException(
      cookie_settings, CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(cookie_settings->IsFullCookieAccessAllowed(
      host, net::SiteForCookies::FromUrl(host), url::Origin::Create(host),
      net::CookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(host));
}

TEST_F(CookiesTreeModelTest, CookiesFilter) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_quota_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://123.com"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1.com"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2.com"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3.com"), "D=1");
  mock_browsing_data_cookie_helper_->Notify();
  EXPECT_EQ("A,B,C,D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(std::u16string(u"foo"));
  EXPECT_EQ("B,C,D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(std::u16string(u"2"));
  EXPECT_EQ("A,C", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(std::u16string(u"foo3"));
  EXPECT_EQ("D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(std::u16string());
  EXPECT_EQ("A,B,C,D", GetDisplayedCookies(&cookies_model));
}

// Tests that cookie source URLs are stored correctly in the cookies
// tree model.
TEST_F(CookiesTreeModelTest, CanonicalizeCookieSource) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_quota_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("file:///tmp/test.html"), "A=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com"), "B=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/"), "C=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/test"), "D=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com:1234/"), "E=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("https://example.com/"), "F=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://user:pwd@example.com/"), "G=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/test?foo"), "H=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/test#foo"), "I=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("https://example2.com/test#foo"), "J=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example3.com:1234/test#foo"), "K=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://user:pwd@example4.com/test?foo"), "L=1");
  mock_browsing_data_cookie_helper_->Notify();

  // Check that all the above example.com cookies go on the example.com
  // host node.
  cookies_model.UpdateSearchResults(std::u16string(u"example.com"));
  EXPECT_EQ("B,C,D,E,F,G,H,I", GetDisplayedCookies(&cookies_model));

  TestingProfile profile;
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(&profile).get();

  // Check that content settings for different URLs get applied to the
  // correct URL. That is, setting a cookie on https://example2.com
  // should create a host node for https://example2.com and thus content
  // settings set on that host node should apply to https://example2.com.

  cookies_model.UpdateSearchResults(std::u16string(u"file://"));
  EXPECT_EQ("", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("file:///test/tmp.html"));

  cookies_model.UpdateSearchResults(std::u16string(u"example2.com"));
  EXPECT_EQ("J", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("https://example2.com"));

  cookies_model.UpdateSearchResults(std::u16string(u"example3.com"));
  EXPECT_EQ("K", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("http://example3.com"));

  cookies_model.UpdateSearchResults(std::u16string(u"example4.com"));
  EXPECT_EQ("L", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("http://example4.com"));
}

TEST_F(CookiesTreeModelTest, CookieDeletionFilterIncognitoProfile) {
  auto* incognito_profile = profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(), true);
  ASSERT_TRUE(incognito_profile->IsOffTheRecord());
  auto callback =
      CookiesTreeModel::GetCookieDeletionDisabledCallback(incognito_profile);
  EXPECT_FALSE(callback);
}

TEST_F(CookiesTreeModelTest, CookieDeletionFilterNormalUser) {
  auto callback =
      CookiesTreeModel::GetCookieDeletionDisabledCallback(profile_.get());

  EXPECT_FALSE(callback);
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(CookiesTreeModelTest, CookieDeletionFilterChildUser) {
  profile_->SetIsSupervisedProfile();
  auto callback =
      CookiesTreeModel::GetCookieDeletionDisabledCallback(profile_.get());

  EXPECT_TRUE(callback);
  EXPECT_FALSE(callback.Run(GURL("https://google.com")));
  EXPECT_FALSE(callback.Run(GURL("https://example.com")));
  EXPECT_TRUE(callback.Run(GURL("http://youtube.com")));
  EXPECT_TRUE(callback.Run(GURL("https://youtube.com")));
}
#endif

TEST_F(CookiesTreeModelTest, InclusiveSize) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  // The root node doesn't have a concept of inclusive size, and so we must look
  // at the host nodes.
  auto& host_nodes = cookies_model->GetRoot()->children();
  uint64_t total =
      std::accumulate(host_nodes.cbegin(), host_nodes.cend(), int64_t{0},
                      [](int64_t total, const auto& child) {
                        return total + child->InclusiveSize();
                      });
  EXPECT_EQ(25u, total);
}

}  // namespace
