// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_skills_manager.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_histogram_tester.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/webui_url_constants.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/skills/features.h"
#include "components/skills/proto/skill.pb.h"
#include "components/skills/public/skills_prefs.h"
#include "components/skills/public/skills_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/skills/skills_ui_tab_controller.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace glic {

class GlicApiTestWithSkills : public GlicApiBrowserTest {
 public:
  GlicApiTestWithSkills()
      : GlicApiBrowserTest(GlicTestJsPath("./glic_api_skills_browsertest.js")) {
    scoped_feature_list_.InitAndEnableFeature(::features::kSkillsEnabled);
  }

  void SetUpOnMainThread() override {
    GlicApiBrowserTest::SetUpOnMainThread();
    service_ = skills::SkillsServiceFactory::GetForProfile(GetProfile());
    ASSERT_TRUE(service_);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return service_->GetServiceStatus() !=
             skills::SkillsService::ServiceStatus::kNotInitialized;
    }));
    service_->SetServiceStatusForTesting(
        skills::SkillsService::ServiceStatus::kReady);
    ASSERT_OK(OpenGlicForActiveTab());
  }

  void TearDownOnMainThread() override {
    service_ = nullptr;
    GlicApiBrowserTest::TearDownOnMainThread();
  }

  skills::SkillsService* SkillsService() { return service_; }

  void WaitForSkillsTab(const std::string& path) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
      return tab && base::StartsWith(
                        tab->GetContents()->GetLastCommittedURL().spec(),
                        GURL(chrome::kChromeUISkillsURL).Resolve(path).spec());
    }));
  }

 private:
  raw_ptr<skills::SkillsService> service_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills, testGetSkillSuccess) {
  SkillsService()->AddSkill(/*source_skill_id=*/"source_id_1",
                            /*name=*/"test_skill_1",
                            /*icon=*/"test_icon_1",
                            /*prompt=*/"test_prompt_1");
  SkillsService()->AddSkill(/*source_skill_id=*/"source_id_2",
                            /*name=*/"test_skill_2",
                            /*icon=*/"test_icon_2",
                            /*prompt=*/"test_prompt_2");
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills, testGetSkillPreviewsSuccess) {
  SkillsService()->AddSkill(/*source_skill_id=*/"source_id_1",
                            /*name=*/"test_skill_1",
                            /*icon=*/"test_icon_1",
                            /*prompt=*/"test_prompt_1");
  SkillsService()->AddSkill(/*source_skill_id=*/"source_id_2",
                            /*name=*/"test_skill_2",
                            /*icon=*/"test_icon_2",
                            /*prompt=*/"test_prompt_2");
  ExecuteJsTest();
}

class GlicApiTestWithSkillsDisabled : public GlicApiBrowserTest {
 public:
  GlicApiTestWithSkillsDisabled()
      : GlicApiBrowserTest(GlicTestJsPath("./glic_api_skills_browsertest.js")) {
    scoped_feature_list_.InitAndEnableFeature(::features::kSkillsEnabled);
  }

  void SetUpOnMainThread() override {
    GlicApiBrowserTest::SetUpOnMainThread();
    GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                         false);
    ASSERT_OK(OpenGlicForActiveTab());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkillsDisabled, testGetSkillDisabled) {
  ExecuteJsTest();
}

// TODO(b/546606964): enable these tests on android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkillsDisabled,
                       testSkillsEnabledToggledAtRuntime) {
  ExecuteJsTest();
  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       true);
  ContinueJsTest();
  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       false);
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkillsDisabled,
                       testContextualSkillsRetainedWhenStartingPrefDisabled) {
  const GURL url = GetTestUrl("page.html");
  skills::proto::SkillsList skills_list;
  skills::proto::Skill* skill = skills_list.add_skills();
  skill->set_id("contextual_skill_id_1");
  skill->set_name("contextual_skill_1");
  skill->set_icon("contextual_skill_icon_1");
  skill->set_description("contextual_skill_description_1");
  skill->set_prompt("contextual_skill_prompt_1");

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/skills.proto.SkillsList");
  skills_list.SerializeToString(any_metadata.mutable_value());
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_any_metadata(any_metadata);

  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile());
  optimization_guide_decider->AddHintForTesting(
      url, optimization_guide::proto::OptimizationType::SKILLS, metadata);

  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), url));

  ExecuteJsTest();

  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       true);
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills, testSkillsEnabledState) {
  glic::GlicHistogramTester histogram_tester;
  SkillsService()->AddSkill(/*source_skill_id=*/"source_id_1",
                            /*name=*/"test_skill_1",
                            /*icon=*/"test_icon_1",
                            /*prompt=*/"test_prompt_1");
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  histogram_tester.ExpectBucketCount(
      "Glic.Skills.WebClient.Event",
      static_cast<int>(mojom::SkillsWebClientEvent::kOpenedMenu), 1);
  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       false);
  ContinueJsTest();
  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       true);
  ContinueJsTest();
  histogram_tester.ExpectBucketCount(
      "Glic.Skills.WebClient.Event",
      static_cast<int>(mojom::SkillsWebClientEvent::kOpenedMenu), 1);
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills, testCreateSkillAndDisable) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
    auto* controller = static_cast<skills::SkillsUiTabController*>(
        skills::SkillsUiTabControllerInterface::From(tab));
    return controller && controller->IsShowing();
  }));
  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       false);
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  auto* controller = static_cast<skills::SkillsUiTabController*>(
      skills::SkillsUiTabControllerInterface::From(tab));
  ASSERT_TRUE(controller);
  controller->CloseDialog();
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills, testDisplaySkillInDialogSuccess) {
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
    auto* controller = static_cast<skills::SkillsUiTabController*>(
        skills::SkillsUiTabControllerInterface::From(tab));
    if (controller && controller->IsShowing()) {
      const auto& skill = controller->GetCurrentSkillForTesting();
      return skill.has_value() && skill->id == "id" && skill->name == "name" &&
             skill->icon == "icon" && skill->prompt == "prompt" &&
             skill->source == sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
    }
    return false;
  }));
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills, testShowManageSkillsUi) {
  ExecuteJsTest();
  WaitForSkillsTab(chrome::kChromeUISkillsYourSkillsPath);
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills, testShowBrowseSkillsUi) {
  ExecuteJsTest();
  WaitForSkillsTab(chrome::kChromeUISkillsBrowsePath);
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills,
                       testSendingContextualSkillsToGlic) {
  SkillsService()->AddSkill(/*source_skill_id=*/"", /*name=*/"user_skill_1",
                            /*icon=*/"user_icon_1",
                            /*prompt=*/"test_prompt_1");
  SkillsService()->AddSkill(/*source_skill_id=*/"", /*name=*/"user_skill_2",
                            /*icon=*/"user_icon_2",
                            /*prompt=*/"user_prompt_2");

  ExecuteJsTest();

  std::vector<mojom::SkillPreviewPtr> skills_batch_1;
  skills_batch_1.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_1", "contextual_skill_1", "contextual_skill_icon_1",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_1",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com"),
      /*category=*/std::nullopt, /*creation_time=*/std::nullopt));
  skills_batch_1.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_2", "contextual_skill_2", "contextual_skill_icon_2",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_2",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com"),
      /*category=*/std::nullopt, /*creation_time=*/std::nullopt));

  GlicInstanceImpl* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);
  instance->skills_manager().NotifyContextualSkillsChanged(
      std::move(skills_batch_1));

  ContinueJsTest();

  std::vector<mojom::SkillPreviewPtr> skills_batch_2;
  skills_batch_2.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_3", "contextual_skill_3", "contextual_skill_icon_3",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_3",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com"),
      /*category=*/std::nullopt, /*creation_time=*/std::nullopt));
  instance->skills_manager().NotifyContextualSkillsChanged(
      std::move(skills_batch_2));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills,
                       testSendingPendingContextualSkillsToGlic) {
  ToggleGlicForActiveTab(/*prevent_close=*/true);
  GlicInstanceImpl* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);

  std::vector<mojom::SkillPreviewPtr> skills_batch;
  skills_batch.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_1", "contextual_skill_1", "contextual_skill_icon_1",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_1",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com"),
      /*category=*/std::nullopt, /*creation_time=*/std::nullopt));

  instance->skills_manager().NotifyContextualSkillsChanged(
      std::move(skills_batch));

  ASSERT_OK(WaitForGlicOpen());

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills,
                       testChangingActiveTabClearsPendingContextualSkills) {
  GetProfile()->GetPrefs()->SetBoolean(
      prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, false);

  ToggleGlicForActiveTab(/*prevent_close=*/true);
  GlicInstanceImpl* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);

  std::vector<mojom::SkillPreviewPtr> skills_batch;
  skills_batch.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_1", "contextual_skill_1", "contextual_skill_icon_1",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_1",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com"),
      /*category=*/std::nullopt, /*creation_time=*/std::nullopt));

  instance->skills_manager().NotifyContextualSkillsChanged(
      std::move(skills_batch));

  // Change the active tab before Glic is opened.
  CreateAndActivateTab(
      embedded_test_server()->GetURL("/glic/browser_tests/test.html"));

  ASSERT_OK_AND_ASSIGN(auto* instance2, OpenGlicForActiveTab());

  ExecuteJsTest({.instance = instance2});
}

// TODO(b/546606964): enable these tests on android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills, testShowManageSkillsUiNoWindow) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndDetach());
  BrowserWindowInterface* browser_to_close = GetBrowserWindowInterface();
  PlatformBrowserTest::CreateIncognitoBrowser();
  CloseBrowserAsynchronously(browser_to_close);

  ui_test_utils::WaitForBrowserToClose(browser_to_close);

  ExecuteJsTest({.instance = instance});

  ASSERT_TRUE(base::test::RunUntil([&]() -> bool {
    auto all_bwis = GetAllBrowserWindowInterfaces();
    for (auto* bwi : all_bwis) {
      for (auto* tab : TabListInterface::From(bwi)->GetAllTabs()) {
        if (tab->GetContents()->GetLastCommittedURL().spec().starts_with(
                chrome::kChromeUISkillsURL)) {
          return true;
        }
      }
    }
    return false;
  }));
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithSkills, testCreateSkillNoWindow) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndDetach());
  BrowserWindowInterface* browser_to_close = GetBrowserWindowInterface();
  PlatformBrowserTest::CreateIncognitoBrowser();
  CloseBrowserAsynchronously(browser_to_close);

  ui_test_utils::WaitForBrowserToClose(browser_to_close);

  ExecuteJsTest({.instance = instance});

  ASSERT_TRUE(base::test::RunUntil([&]() -> bool {
    auto all_bwis = GetAllBrowserWindowInterfaces();
    for (auto* bwi : all_bwis) {
      for (auto* tab : TabListInterface::From(bwi)->GetAllTabs()) {
        if (tab->GetContents()->GetLastCommittedURL().spec().starts_with(
                chrome::kChromeUISkillsURL)) {
          return true;
        }
      }
    }
    return false;
  }));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace glic
