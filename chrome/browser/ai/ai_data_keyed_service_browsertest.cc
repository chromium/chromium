// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_data_keyed_service.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_switches.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/ai/ai_data_keyed_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/history_embeddings/mock_answerer.h"
#include "components/history_embeddings/mock_intent_classifier.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/passage_embeddings/passage_embeddings_test_util.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::TestFuture;
using ::optimization_guide::DocumentIdentifierUserData;
using ::optimization_guide::proto::ClickAction;
using ::testing::ReturnRef;
using AiData = AiDataKeyedService::AiData;
using AiDataSpecifier = AiDataKeyedService::AiDataSpecifier;

class AiDataKeyedServiceBrowserTest : public InProcessBrowserTest {
 public:
  AiDataKeyedServiceBrowserTest() = default;

  AiDataKeyedServiceBrowserTest(const AiDataKeyedServiceBrowserTest&) = delete;
  AiDataKeyedServiceBrowserTest& operator=(
      const AiDataKeyedServiceBrowserTest&) = delete;

  ~AiDataKeyedServiceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());

    HistoryEmbeddingsServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindLambdaForTesting([this](content::BrowserContext* context) {
          return HistoryEmbeddingsServiceFactory::
              BuildServiceInstanceForBrowserContextForTesting(
                  context,
                  passage_embeddings_test_env_.embedder_metadata_provider(),
                  passage_embeddings_test_env_.embedder(),
                  std::make_unique<history_embeddings::MockAnswerer>(),
                  std::make_unique<history_embeddings::MockIntentClassifier>());
        }));
  }

  AiDataKeyedService& ai_data_service() {
    return *AiDataKeyedServiceFactory::GetAiDataKeyedService(
        browser()->profile());
  }

  actor::ActorKeyedService& actor_service() {
    return *actor::ActorKeyedService::Get(browser()->profile());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void LoadPage(const GURL& url) {
    content::NavigateToURLBlockUntilNavigationsComplete(web_contents(), url, 1);
    content::WaitForCopyableViewInWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  void LoadSimplePage() { LoadPage(https_server_->GetURL("/simple.html")); }

  AiData QueryAiData() {
    base::test::TestFuture<AiData> ai_data;
    ai_data_service().GetAiData(1, web_contents(), "", ai_data.GetCallback(),
                                1);
    return ai_data.Get();
  }

  AiData QueryAiDataWithSpecifier(AiDataSpecifier specifier) {
    base::test::TestFuture<AiData> ai_data;
    ai_data_service().GetAiDataWithSpecifier(
        web_contents(), std::move(specifier), ai_data.GetCallback());
    return ai_data.Get();
  }

  AiData LoadSimplePageAndData() {
    LoadSimplePage();
    return QueryAiData();
  }

  AiData LoadSimplePageAndDataWithSpecifier(AiDataSpecifier specifier) {
    LoadSimplePage();
    return QueryAiDataWithSpecifier(std::move(specifier));
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  autofill::test::AutofillBrowserTestEnvironment autofill_test_environment_;
  passage_embeddings::TestEnvironment passage_embeddings_test_env_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_ =
      std::make_unique<net::EmbeddedTestServer>(
          net::EmbeddedTestServer::TYPE_HTTPS);
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest,
                       AllowlistedExtensionListData) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "hpkopmikdojpadgmioifjjodbmnjjjca", "bgbpcgpcobgjpnpiginpidndjpggappi",
      "eefninhhiifgcimjkmkongegpoaikmhm", "fjhpgileahdpnmfmaggobehbipojhlce",
      "abdciamfdmknaeggbnmafmbdfdmhfgfa", "fiamdfnbelfkjlacoaeiclobkdmckaoa"};

  for (const auto& extension_id : expected_allowlisted_extensions) {
    EXPECT_TRUE(
        AiDataKeyedService::IsExtensionAllowlistedForData(extension_id));
  }
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest,
                       AllowlistedExtensionListActions) {
  std::vector<std::string> expected_allowlisted_extensions = {};

  for (const auto& extension_id : expected_allowlisted_extensions) {
    EXPECT_TRUE(
        AiDataKeyedService::IsExtensionAllowlistedForActions(extension_id));
  }

  std::vector<std::string> expected_not_allowlisted_extensions = {
      "hpkopmikdojpadgmioifjjodbmnjjjca", "bgbpcgpcobgjpnpiginpidndjpggappi",
      "eefninhhiifgcimjkmkongegpoaikmhm", "fjhpgileahdpnmfmaggobehbipojhlce",
      "abdciamfdmknaeggbnmafmbdfdmhfgfa", "fiamdfnbelfkjlacoaeiclobkdmckaoa"};
  for (const auto& extension_id : expected_not_allowlisted_extensions) {
    EXPECT_FALSE(
        AiDataKeyedService::IsExtensionAllowlistedForActions(extension_id));
  }
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, GetsData) {
  EXPECT_TRUE(LoadSimplePageAndData().has_value());
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, InnerText) {
  AiData ai_data = LoadSimplePageAndData();
  ASSERT_TRUE(ai_data.has_value());
  EXPECT_EQ(ai_data->page_context().inner_text(), "Non empty simple page");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, InnerTextOffset) {
  AiData ai_data = LoadSimplePageAndData();
  ASSERT_TRUE(ai_data.has_value());
  EXPECT_EQ(ai_data->page_context().inner_text_offset(), 0u);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Title) {
  AiData ai_data = LoadSimplePageAndData();
  ASSERT_TRUE(ai_data.has_value());
  EXPECT_EQ(ai_data->page_context().title(), "OK");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Url) {
  AiData ai_data = LoadSimplePageAndData();
  ASSERT_TRUE(ai_data.has_value());
  EXPECT_NE(ai_data->page_context().url().find("simple"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest,
                       EmptyHistoryResultWithEmptyQueryString) {
  AiDataSpecifier specifier;
  auto* history_query_specifiers =
      specifier.mutable_browser_data_collection_specifier()
          ->mutable_history_query_specifiers();
  history_query_specifiers->add_history_queries()->set_query("");
  AiData ai_data = LoadSimplePageAndDataWithSpecifier(std::move(specifier));
  ASSERT_TRUE(ai_data.has_value());
  EXPECT_TRUE(ai_data->history_query_result().empty());
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, AxTreeUpdate) {
  AiData ai_data = LoadSimplePageAndData();
  ASSERT_TRUE(ai_data.has_value());
  // If there are nodes and the titles is correct, then the AX tree is filled
  // out.
  EXPECT_GT(ai_data->page_context().ax_tree_data().nodes().size(), 0);
  EXPECT_EQ(ai_data->page_context().ax_tree_data().tree_data().title(), "OK");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, TabData) {
  chrome::AddTabAt(browser(), GURL("foo.com"), -1, false);
  chrome::AddTabAt(browser(), GURL("bar.com"), -1, false);

  auto* tab_group1 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({0}));
  auto vis_data1 = *tab_group1->visual_data();
  vis_data1.SetTitle(u"ok");
  browser()->GetTabStripModel()->ChangeTabGroupVisuals(tab_group1->id(),
                                                       vis_data1);

  auto* tab_group2 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({1, 2}));
  auto vis_data2 = *tab_group1->visual_data();
  vis_data2.SetTitle(u"ok");
  browser()->GetTabStripModel()->ChangeTabGroupVisuals(tab_group2->id(),
                                                       vis_data2);

  AiData ai_data = LoadSimplePageAndData();
  ASSERT_TRUE(ai_data.has_value());
  EXPECT_EQ(ai_data->active_tab_id(), 0);
  EXPECT_EQ(ai_data->tabs().size(), 3);
  EXPECT_EQ(ai_data->pre_existing_tab_groups().size(), 2);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, TabInnerText) {
  chrome::AddTabAt(browser(), GURL("foo.com"), -1, false);
  chrome::AddTabAt(browser(), GURL("bar.com"), -1, false);

  auto* tab_group1 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({0}));
  auto vis_data1 = *tab_group1->visual_data();
  vis_data1.SetTitle(u"ok");
  browser()->GetTabStripModel()->ChangeTabGroupVisuals(tab_group1->id(),
                                                       vis_data1);

  auto* tab_group2 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({1, 2}));
  auto vis_data2 = *tab_group1->visual_data();
  vis_data2.SetTitle(u"ok");
  browser()->GetTabStripModel()->ChangeTabGroupVisuals(tab_group2->id(),
                                                       vis_data2);

  AiData ai_data = LoadSimplePageAndData();
  ASSERT_TRUE(ai_data.has_value());
  EXPECT_EQ(ai_data->active_tab_id(), 0);
  for (const auto& tab_in_proto : ai_data->tabs()) {
    if (tab_in_proto.tab_id() == 0) {
      EXPECT_EQ(tab_in_proto.title(), "OK");
      EXPECT_NE(tab_in_proto.url().find("simple"), std::string::npos);
      EXPECT_EQ(tab_in_proto.page_context().inner_text(),
                "Non empty simple page");
    }
  }
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, TabInnerTextLimit) {
  LoadSimplePageAndData();
  chrome::AddTabAt(browser(), GURL("bar.com"), -1, true);
  AiData ai_data = LoadSimplePageAndData();
  ASSERT_TRUE(ai_data.has_value());
  EXPECT_EQ(ai_data->active_tab_id(), 1);
  for (auto& tab : ai_data->tabs()) {
    if (tab.tab_id() == 0) {
      EXPECT_EQ(tab.page_context().inner_text(), "Non empty simple page");
    }
    if (tab.tab_id() == 1) {
      EXPECT_EQ(tab.page_context().inner_text(), "");
    }
  }
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Screenshot) {
  AiData ai_data = LoadSimplePageAndData();
  ASSERT_TRUE(ai_data.has_value());
  content::RequestFrame(web_contents());
  EXPECT_NE(ai_data->page_context().tab_screenshot(), "");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, SiteEngagementScores) {
  AiData ai_data = LoadSimplePageAndData();
  ASSERT_TRUE(ai_data.has_value());
  EXPECT_EQ(ai_data->site_engagement().entries().size(), 1);
  EXPECT_NE(ai_data->site_engagement().entries()[0].url(), "");
  EXPECT_GE(ai_data->site_engagement().entries()[0].score(), 0);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, AIPageContent) {
  LoadPage(
      https_server()->GetURL("/optimization_guide/actionable_elements.html"));
  AiData ai_data = QueryAiData();
  ASSERT_TRUE(ai_data.has_value());

  {
    const auto& page_content = ai_data->page_context().annotated_page_content();
    EXPECT_EQ(page_content.version(),
              optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
    const auto& content_attributes =
        page_content.root_node().content_attributes();
    EXPECT_EQ(content_attributes.attribute_type(),
              optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
    EXPECT_FALSE(content_attributes.has_interaction_info());
    EXPECT_EQ(page_content.root_node().children_nodes().size(), 0);
  }

  {
    const auto& page_content = ai_data->action_annotated_page_content();
    EXPECT_EQ(page_content.version(),
              optimization_guide::proto::
                  ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);
    const auto& content_attributes =
        page_content.root_node().content_attributes();
    EXPECT_EQ(content_attributes.attribute_type(),
              optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
    EXPECT_TRUE(content_attributes.has_interaction_info());
    EXPECT_FALSE(content_attributes.interaction_info().is_clickable());

    const auto& html = page_content.root_node().children_nodes().at(0);
    const auto& body = html.children_nodes().at(0);

    ASSERT_EQ(body.children_nodes().size(), 1);
    const auto& child = body.children_nodes().at(0);
    EXPECT_TRUE(child.content_attributes().has_interaction_info());
    EXPECT_TRUE(child.content_attributes().interaction_info().is_clickable());
  }
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, SpecifierOn) {
  AiDataSpecifier specifier;
  auto* browser_specifier =
      specifier.mutable_browser_data_collection_specifier();
  auto* foreground_tab_specifier =
      browser_specifier->mutable_foreground_tab_page_context_specifier();
  foreground_tab_specifier->set_inner_text(true);
  foreground_tab_specifier->set_tab_screenshot(true);
  foreground_tab_specifier->set_ax_tree(true);
  foreground_tab_specifier->set_pdf_data(true);
  auto* general_tabs_specifier =
      browser_specifier->mutable_tabs_context_specifier()
          ->mutable_general_tab_specifier();
  general_tabs_specifier->mutable_page_context_specifier()->set_inner_text(
      true);
  general_tabs_specifier->set_tab_limit(2);
  browser_specifier->set_site_engagement(true);
  browser_specifier->set_tab_groups(true);

  AiData ai_data = LoadSimplePageAndDataWithSpecifier(std::move(specifier));
  ASSERT_TRUE(ai_data.has_value());
  EXPECT_NE(ai_data->page_context().tab_screenshot(), "");
  const auto& page_content = ai_data->page_context().annotated_page_content();
  const auto& content_attributes =
      page_content.root_node().content_attributes();
  EXPECT_EQ(content_attributes.attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
  EXPECT_EQ(ai_data->site_engagement().entries().size(), 1);
  EXPECT_NE(ai_data->site_engagement().entries()[0].url(), "");
  EXPECT_GE(ai_data->site_engagement().entries()[0].score(), 0);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, SpecifierOff) {
  AiDataSpecifier specifier;
  AiData ai_data = LoadSimplePageAndDataWithSpecifier(std::move(specifier));
  ASSERT_TRUE(ai_data.has_value());
  EXPECT_EQ(ai_data->page_context().tab_screenshot(), "");
  EXPECT_EQ(ai_data->page_context().inner_text(), "");
  EXPECT_EQ(ai_data->site_engagement().entries().size(), 0);
  EXPECT_TRUE(ai_data->history_query_result().empty());
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest,
                       GetFormDataByFieldGlobalIdForModelPrototyping) {
  // Simulate loading `expected_form`.
  LoadSimplePage();
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          web_contents()->GetPrimaryMainFrame());
  const autofill::FormData expected_form = autofill::test::GetFormData(
      {.fields = {{.label = u"Field 1"}, {.label = u"Field 2"}}});
  ASSERT_TRUE(driver);
  autofill::TestAutofillManagerSingleEventWaiter wait_for_forms_seen(
      driver->GetAutofillManager(),
      &autofill::AutofillManager::Observer::OnAfterFormsSeen,
      testing::ElementsAre(expected_form.global_id()), testing::IsEmpty());
  driver->GetAutofillManager().OnFormsSeen(/*updated_forms=*/{expected_form},
                                           /*removed_forms=*/{});
  ASSERT_TRUE(std::move(wait_for_forms_seen).Wait());

  // Query the API for `expected_form`'s first field.
  AiDataKeyedService::AiDataSpecifier specifier;
  auto* global_id = specifier.mutable_browser_data_collection_specifier()
                        ->mutable_foreground_tab_page_context_specifier()
                        ->mutable_field_global_id();
  global_id->set_frame_token(
      expected_form.fields()[0].global_id().frame_token->ToString());
  global_id->set_renderer_id(
      expected_form.fields()[0].global_id().renderer_id.value());
  AiData ai_data = QueryAiDataWithSpecifier(std::move(specifier));

  // Expect that the result matches `expected_form`.
  ASSERT_TRUE(ai_data.has_value());
  ASSERT_TRUE(ai_data->has_form_data());
  const optimization_guide::proto::FormData& actual_form = ai_data->form_data();
  ASSERT_EQ(actual_form.fields_size(), 2);
  EXPECT_EQ(actual_form.fields(0).field_label(),
            base::UTF16ToUTF8(expected_form.fields()[0].label()));
  EXPECT_EQ(actual_form.fields(1).field_label(),
            base::UTF16ToUTF8(expected_form.fields()[1].label()));
}
#endif

class AiDataKeyedServiceBrowserTestWithBlocklistedExtensions
    : public AiDataKeyedServiceBrowserTest {
 public:
  ~AiDataKeyedServiceBrowserTestWithBlocklistedExtensions() override = default;
  AiDataKeyedServiceBrowserTestWithBlocklistedExtensions() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting(),
        {{"blocked_extension_ids", "hpkopmikdojpadgmioifjjodbmnjjjca"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTestWithBlocklistedExtensions,
                       BlockedExtensionList) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "bgbpcgpcobgjpnpiginpidndjpggappi", "eefninhhiifgcimjkmkongegpoaikmhm",
      "fjhpgileahdpnmfmaggobehbipojhlce", "abdciamfdmknaeggbnmafmbdfdmhfgfa",
      "fiamdfnbelfkjlacoaeiclobkdmckaoa"};

  EXPECT_FALSE(AiDataKeyedService::IsExtensionAllowlistedForData(
      "hpkopmikdojpadgmioifjjodbmnjjjca"));
  for (const auto& extension : expected_allowlisted_extensions) {
    EXPECT_TRUE(AiDataKeyedService::IsExtensionAllowlistedForData(extension));
  }
}

class AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions
    : public AiDataKeyedServiceBrowserTest {
 public:
  ~AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions() override =
      default;
  AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting(),
        {{"allowlisted_extension_ids", "1234"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions,
    RemotelyAllowlistedExtensionList) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "1234",
      "hpkopmikdojpadgmioifjjodbmnjjjca",
      "bgbpcgpcobgjpnpiginpidndjpggappi",
      "eefninhhiifgcimjkmkongegpoaikmhm",
      "fjhpgileahdpnmfmaggobehbipojhlce",
      "abdciamfdmknaeggbnmafmbdfdmhfgfa",
      "fiamdfnbelfkjlacoaeiclobkdmckaoa"};

  for (const auto& extension : expected_allowlisted_extensions) {
    EXPECT_TRUE(AiDataKeyedService::IsExtensionAllowlistedForData(extension));
  }
}

class AiDataKeyedServiceBrowserTestWithAllowAndBlock
    : public AiDataKeyedServiceBrowserTest {
 public:
  ~AiDataKeyedServiceBrowserTestWithAllowAndBlock() override = default;
  AiDataKeyedServiceBrowserTestWithAllowAndBlock() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting(),
        {{"allowlisted_extension_ids", "1234"},
         {"blocked_extension_ids", "1234"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTestWithAllowAndBlock,
                       AllowAndBlock) {
  EXPECT_FALSE(AiDataKeyedService::IsExtensionAllowlistedForData("1234"));
}

}  // namespace
