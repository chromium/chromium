// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_data_keyed_service.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ai/ai_data_keyed_service_factory.h"
#include "chrome/browser/autofill_ai/chrome_autofill_ai_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::ReturnRef;

class MockAutofillAiModelExecutor
    : public autofill_ai::AutofillAiModelExecutor {
 public:
  MOCK_METHOD(
      void,
      GetPredictions,
      (autofill::FormData form_data,
       (base::flat_map<autofill::FieldGlobalId, bool> field_eligibility_map),
       (base::flat_map<autofill::FieldGlobalId, bool> sensitivity_map),
       optimization_guide::proto::AXTreeUpdate ax_tree_update,
       PredictionsReceivedCallback callback),
      (override));
  MOCK_METHOD(
      const std::optional<optimization_guide::proto::FormsPredictionsRequest>&,
      GetLatestRequest,
      (),
      (const override));
  MOCK_METHOD(
      const std::optional<optimization_guide::proto::FormsPredictionsResponse>&,
      GetLatestResponse,
      (),
      (const override));
};

class AiDataKeyedServiceBrowserTest : public InProcessBrowserTest {
 public:
  AiDataKeyedServiceBrowserTest() = default;

  AiDataKeyedServiceBrowserTest(const AiDataKeyedServiceBrowserTest&) = delete;
  AiDataKeyedServiceBrowserTest& operator=(
      const AiDataKeyedServiceBrowserTest&) = delete;

  ~AiDataKeyedServiceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetChromeTestDataDir());

    ASSERT_TRUE(https_server_->Start());

    url_ = https_server_->GetURL("/simple.html");
  }

  void SetAiData(base::OnceClosure quit_closure,
                 AiDataKeyedService::AiData ai_data) {
    ai_data_ = std::move(ai_data);
    std::move(quit_closure).Run();
  }

  GURL url() { return url_; }

  const AiDataKeyedService::AiData& ai_data() { return ai_data_; }

  void LoadSimplePageAndData() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::NavigateToURLBlockUntilNavigationsComplete(web_contents, url_, 1);

    AiDataKeyedService* ai_data_service =
        AiDataKeyedServiceFactory::GetAiDataKeyedService(browser()->profile());

    base::RunLoop run_loop;
    auto dom_node_id = 0;
    ai_data_service->GetAiDataWithSpecifiers(
        1, dom_node_id, web_contents, "test",
        base::BindOnce(&AiDataKeyedServiceBrowserTest::SetAiData,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    DCHECK(ai_data());
  }

 private:
  GURL url_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  AiDataKeyedService::AiData ai_data_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest,
                       AllowlistedExtensionList) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "hpkopmikdojpadgmioifjjodbmnjjjca", "bgbpcgpcobgjpnpiginpidndjpggappi",
      "eefninhhiifgcimjkmkongegpoaikmhm"};

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, GetsData) {
  LoadSimplePageAndData();
  EXPECT_TRUE(ai_data());
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, InnerText) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->page_context().inner_text(), "Non empty simple page");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, InnerTextOffset) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->page_context().inner_text_offset(), 0u);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Title) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->page_context().title(), "OK");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Url) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_NE(ai_data()->page_context().url().find("simple"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, AxTreeUpdate) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  // If there are nodes and the titles is correct, then the AX tree is filled
  // out.
  EXPECT_GT(ai_data()->page_context().ax_tree_data().nodes().size(), 0);
  EXPECT_EQ(ai_data()->page_context().ax_tree_data().tree_data().title(), "OK");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, TabData) {
  chrome::AddTabAt(browser(), GURL("foo.com"), -1, false);
  chrome::AddTabAt(browser(), GURL("bar.com"), -1, false);

  auto* tab_group1 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({0}));
  auto vis_data1 = *tab_group1->visual_data();
  vis_data1.SetTitle(u"ok");
  tab_group1->SetVisualData(vis_data1);

  auto* tab_group2 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({1, 2}));
  auto vis_data2 = *tab_group1->visual_data();
  vis_data2.SetTitle(u"ok");
  tab_group2->SetVisualData(vis_data2);

  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());

  EXPECT_EQ(ai_data()->active_tab_id(), 0);
  EXPECT_EQ(ai_data()->tabs().size(), 3);
  EXPECT_EQ(ai_data()->pre_existing_tab_groups().size(), 2);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, TabInnerText) {
  chrome::AddTabAt(browser(), GURL("foo.com"), -1, false);
  chrome::AddTabAt(browser(), GURL("bar.com"), -1, false);

  auto* tab_group1 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({0}));
  auto vis_data1 = *tab_group1->visual_data();
  vis_data1.SetTitle(u"ok");
  tab_group1->SetVisualData(vis_data1);

  auto* tab_group2 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({1, 2}));
  auto vis_data2 = *tab_group1->visual_data();
  vis_data2.SetTitle(u"ok");
  tab_group2->SetVisualData(vis_data2);

  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->active_tab_id(), 0);
  for (const auto& tab_in_proto : ai_data()->tabs()) {
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
  LoadSimplePageAndData();
  EXPECT_EQ(ai_data()->active_tab_id(), 1);
  for (auto& tab : ai_data()->tabs()) {
    if (tab.tab_id() == 0) {
      EXPECT_EQ(tab.page_context().inner_text(), "Non empty simple page");
    }
    if (tab.tab_id() == 1) {
      EXPECT_EQ(tab.page_context().inner_text(), "");
    }
  }
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Screenshot) {
  LoadSimplePageAndData();
  content::RequestFrame(browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_NE(ai_data()->page_context().tab_screenshot(), "");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, SiteEngagementScores) {
  LoadSimplePageAndData();
  EXPECT_EQ(ai_data()->site_engagement().entries().size(), 1);
  EXPECT_NE(ai_data()->site_engagement().entries()[0].url(), "");
  EXPECT_GE(ai_data()->site_engagement().entries()[0].score(), 0);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, AIPageContent) {
  LoadSimplePageAndData();

  const auto& page_content = ai_data()->page_context().annotated_page_content();
  const auto& content_attributes =
      page_content.root_node().content_attributes();
  EXPECT_EQ(content_attributes.attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
}
#if !BUILDFLAG(IS_ANDROID)
class AiDataKeyedServiceBrowserTestWithFormsPredictions
    : public AiDataKeyedServiceBrowserTest {
 public:
  ~AiDataKeyedServiceBrowserTestWithFormsPredictions() override = default;
  AiDataKeyedServiceBrowserTestWithFormsPredictions() {
    scoped_feature_list_.InitAndEnableFeature(autofill_ai::kAutofillAi);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTestWithFormsPredictions,
                       GetFormsPredictionsDataForModelPrototyping) {
  browser()->profile()->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillPredictionImprovementsEnabled, true);

  // Set up test data.
  auto request =
      std::make_optional<optimization_guide::proto::FormsPredictionsRequest>();
  optimization_guide::proto::UserAnnotationsEntry* entry =
      request->add_entries();
  entry->set_key("test_key");
  entry->set_value("test_value");
  auto response =
      std::make_optional<optimization_guide::proto::FormsPredictionsResponse>();
  optimization_guide::proto::FilledFormData* filled_form_data =
      response->mutable_form_data();
  optimization_guide::proto::FilledFormFieldData* filled_field =
      filled_form_data->add_filled_form_field_data();
  filled_field->set_normalized_label("test_label");

  // Set up mock.
  auto mock_autofill_ai_model_executor =
      std::make_unique<MockAutofillAiModelExecutor>();
  EXPECT_CALL(*mock_autofill_ai_model_executor, GetLatestRequest)
      .WillOnce(ReturnRef(request));
  EXPECT_CALL(*mock_autofill_ai_model_executor, GetLatestResponse)
      .WillOnce(ReturnRef(response));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  ASSERT_TRUE(tab)
      << "Active WebContents isn't a tab. TabInterface::GetFromContents() "
         "was expected to crash.";
  ChromeAutofillAiClient* client =
      tab->GetTabFeatures()->chrome_autofill_ai_client();
  ASSERT_TRUE(client)
      << "TabFeatures hasn't created ChromeAutofillAiClient yet.";
  client->SetModelExecutorForTesting(
      std::move(mock_autofill_ai_model_executor));

  LoadSimplePageAndData();

  ASSERT_TRUE(ai_data());
  ASSERT_EQ(ai_data()->forms_predictions_request().entries().size(), 1);
  EXPECT_EQ(ai_data()->forms_predictions_request().entries()[0].key(),
            "test_key");
  EXPECT_EQ(ai_data()->forms_predictions_request().entries()[0].value(),
            "test_value");
  ASSERT_EQ(ai_data()
                ->forms_predictions_response()
                .form_data()
                .filled_form_field_data()
                .size(),
            1);
  EXPECT_EQ(ai_data()
                ->forms_predictions_response()
                .form_data()
                .filled_form_field_data()[0]
                .normalized_label(),
            "test_label");
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
      "bgbpcgpcobgjpnpiginpidndjpggappi", "eefninhhiifgcimjkmkongegpoaikmhm"};

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
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
      "1234", "hpkopmikdojpadgmioifjjodbmnjjjca",
      "bgbpcgpcobgjpnpiginpidndjpggappi", "eefninhhiifgcimjkmkongegpoaikmhm"};

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
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
  std::vector<std::string> expected_allowlisted_extensions = {
      "hpkopmikdojpadgmioifjjodbmnjjjca", "bgbpcgpcobgjpnpiginpidndjpggappi",
      "eefninhhiifgcimjkmkongegpoaikmhm"};

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
}

}  // namespace
