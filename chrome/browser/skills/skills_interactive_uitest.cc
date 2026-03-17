// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_functional_browsertest.h"
#include "chrome/browser/skills/skills_glic_mojom_util.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_window_controller.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_window_test.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/skills/features.h"
#include "components/skills/internal/skills_downloader.h"
#include "components/skills/internal/skills_service_impl.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/model/data_type_store_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/interactive_test.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSkillsDialogElementId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<GURL>,
                                    kOpenedTabUrlState);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementEnabled);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

static constexpr char kClickFn[] = "el => el.click()";

MATCHER_P(VerifyUserCreatedSkill, expected, "") {
  // Ensures that the skill matches the expected values and that has a "user
  // created" source and a valid ID.
  return arg->name == expected.name && arg->icon == expected.icon &&
         arg->prompt == expected.prompt &&
         arg->source == sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED &&
         arg->source_skill_id.empty() &&
         base::Uuid::ParseLowercase(arg->id).is_valid();
}

MATCHER_P2(VerifyRemixedFirstPartySkill, expected, source, "") {
  // Ensures that the skill matches the expected values and that has a "derived
  // from first party" source and a valid ID different from the source skill.
  return arg->name == expected.name && arg->icon == expected.icon &&
         arg->prompt == expected.prompt &&
         arg->source ==
             sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY &&
         arg->source_skill_id == source.id && arg->id != source.id &&
         base::Uuid::ParseLowercase(arg->id).is_valid();
}

optimization_guide::OptimizationMetadata SkillVectorToOptimizationMetaData(
    std::vector<glic::mojom::SkillPtr> skills) {
  skills::proto::SkillsList skills_list;
  for (const auto& glicSkill : skills) {
    skills::proto::Skill* skill = skills_list.add_skills();
    skill->set_id(glicSkill->preview->id);
    skill->set_name(glicSkill->preview->name);
    skill->set_icon(glicSkill->preview->icon);
    skill->set_description(glicSkill->preview->description);
    skill->set_prompt(glicSkill->prompt);
  }
  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/skills.proto.SkillsList");
  skills_list.SerializeToString(any_metadata.mutable_value());
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_any_metadata(any_metadata);
  return metadata;
}

}  // namespace

class SkillsInteractiveUiTest : public TabStripInteractiveTestMixin<
                                    skills::SkillsFunctionalBrowserTestBase> {
 public:
  SkillsInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kGlicRollout,
                              features::kSkillsEnabled,
                              features::kGlicMultitabUnderlines},
        /*disabled_features=*/{features::kGlicWarming,
                               features::kGlicTrustFirstOnboarding});
    // Ensure that we open the FRE.
    glic_test_environment().SetFreStatusForNewProfiles(std::nullopt);
  }

  ~SkillsInteractiveUiTest() override = default;

  using StateChange = WebContentsInteractionTestUtil::StateChange;

  ui::test::InteractiveTestApi::MultiStep WaitForElementExists(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    StateChange element_exists;
    element_exists.type =
        WebContentsInteractionTestUtil::StateChange::Type::kExists;
    element_exists.event = kElementExists;
    element_exists.where = element;
    return WaitForStateChange(contents_id, element_exists);
  }

  ui::test::InteractiveTestApi::MultiStep WaitForElementEnabled(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    StateChange element_enabled;
    element_enabled.type = WebContentsInteractionTestUtil::StateChange::Type::
        kExistsAndConditionTrue;
    element_enabled.event = kElementEnabled;
    element_enabled.where = element;
    element_enabled.test_function = "(el) => !el.disabled";
    return WaitForStateChange(contents_id, element_enabled);
  }

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();

    skills::SkillsServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&SkillsInteractiveUiTest::CreateSkillsService,
                            base::Unretained(this)));

    skills::SkillsServiceFactory::GetForProfile(browser()->profile())
        ->SetServiceStatusForTesting(
            skills::SkillsService::ServiceStatus::kReady);

    // Initially seed zero 1p Skills.
    skills::proto::SkillsList skills_list;
    std::string response_data;
    ASSERT_TRUE(skills_list.SerializeToString(&response_data));
    GURL expected_url(skills::kSkillsDownloaderGstaticUrl);
    test_url_loader_factory_.AddResponse(expected_url.spec(), response_data,
                                         net::HTTP_OK);
  }

  std::unique_ptr<KeyedService> CreateSkillsService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<skills::SkillsServiceImpl>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
        chrome::GetChannel(),
        DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  auto UpdateContextualSkillPreviews(
      std::vector<glic::mojom::SkillPreviewPtr> contextual_skill_previews) {
    return Steps(Do([this, contextual_skill_previews =
                               std::move(contextual_skill_previews)]() mutable {
      glic_service()
          ->GetInstanceForTab(browser()->GetActiveTabInterface())
          ->host()
          .NotifyContextualSkillsChanged(std::move(contextual_skill_previews));
    }));
  }

  auto AddUserOwnedSkill(glic::mojom::SkillPtr skill,
                         std::string* out_skill_id = nullptr) {
    return Do([this, skill = std::move(skill), out_skill_id]() mutable {
      auto* added_skill =
          GetSkillsService()->AddSkill(skill->preview->id, skill->preview->name,
                                       skill->preview->icon, skill->prompt);

      if (out_skill_id && added_skill) {
        *out_skill_id = added_skill->id;
      }
    });
  }

  auto InvokeSkillDirectly(std::string* skill_id_ptr) {
    return Do([this, skill_id_ptr]() {
      skills::SkillsUiWindowController::From(browser())->InvokeSkill(
          *skill_id_ptr);
    });
  }

  auto CreateSkill(skills::Skill skill) {
    return Do([this, skill = std::move(skill)]() {
      auto request = glic::mojom::CreateSkillRequest::New(
          skill.id, skill.name, skill.icon,
          skills::SyncPbToGlicMojomSkillSource(skill.source), skill.prompt,
          skill.description);
      skills::SkillsFunctionalBrowserTestBase::CreateSkill(std::move(request));
    });
  }

  auto VerifyInvocationInWebUI(const std::string& expected_prompt) {
    return Steps(
        Log("Verifying Glic Panel Opened via Toast Interaction"),
        WaitForShow(glic::test::kGlicHostElementId),

        // This will now pass because test_client.js updates the value!
        WaitForJsResult(
            glic::test::kGlicContentsElementId,
            base::StringPrintf(
                "() => {"
                "  const input = document.getElementById('skillPromptInput');"
                "  return !!input && input.value === '%s';"
                "}",
                expected_prompt.c_str())));
  }

  auto EditDialogInput(const std::string& value, const DeepQuery& query) {
    return ExecuteJsAt(
        kSkillsDialogElementId, query,
        base::StringPrintf(
            "el => {"
            "  el.value = '%s';"
            "  el.dispatchEvent(new Event('input', { bubbles: true }));"
            "}",
            value.c_str()));
  }

  auto VerifyDialogInput(const std::string& expected_value,
                         const DeepQuery& query) {
    return Steps(WaitForElementExists(kSkillsDialogElementId, query),
                 CheckJsResultAt(kSkillsDialogElementId, query,
                                 "el => el.value", expected_value));
  }

  auto ClickButtonAndVerifyDialogHides(const DeepQuery& button_query) {
    return Steps(WaitForElementEnabled(kSkillsDialogElementId, button_query),
                 MoveMouseTo(kSkillsDialogElementId, button_query),
                 ClickMouse(),
                 // Verify the dialog hides.
                 WaitForHide(skills::SkillsDialogView::kSkillsDialogElementId));
  }

  auto ActivateTabAt(int tab_index) {
    return Do([this, tab_index]() {
      browser()->GetTabStripModel()->ActivateTabAt(tab_index);
    });
  }

  auto WaitForSkillPreviewShown(std::string_view skill_name) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSkillPreviewShown);
    StateChange state_change;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.where = {"#skillsList > li > span.skill-name[value=\"" +
                          std::string(skill_name) + "\"]"};
    state_change.test_function = "el => el.checkVisibility()";
    state_change.event = kSkillPreviewShown;
    return WaitForStateChange(glic::test::kGlicContentsElementId, state_change);
  }

  auto ClickOnGlicClientElement(DeepQuery where) {
    return ExecuteJsAt(glic::test::kGlicContentsElementId, where, kClickFn);
  }

  auto PollForAndAcceptFre() {
    return Steps(
        PollUntil(
            [this]() {
              return glic_service()->fre_controller().GetWebUiState() ==
                     glic::mojom::FreWebUiState::kReady;
            },
            "polling until the fre is ready"),
        Do([this]() { glic_service()->fre_controller().AcceptFre(nullptr); }));
  }

  auto OpenGlicAcceptFreAndInstrument() {
    return Steps(
        ToggleGlicWindow(GlicWindowMode::kAttached), PollForAndAcceptFre(),
        WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents));
  }

  auto Seed1PSkills(const std::vector<skills::proto::Skill>& skills) {
    return Do([this, skills]() {
      skills::proto::SkillsList skills_list;
      for (const auto& skill : skills) {
        *skills_list.add_skills() = skill;
      }

      std::string response_data;
      ASSERT_TRUE(skills_list.SerializeToString(&response_data));

      GURL expected_url(skills::kSkillsDownloaderGstaticUrl);
      test_url_loader_factory_.AddResponse(expected_url.spec(), response_data,
                                           net::HTTP_OK);
    });
  }

  auto WaitFor1PSkills() {
    return PollUntil(
        [this]() {
          return !skills::SkillsServiceFactory::GetForProfile(
                      browser()->profile())
                      ->Get1PSkills()
                      .empty();
        },
        "polling until 1P skills are not empty.");
  }

  // Waits for the nth `tab` to be open to `url`.
  auto WaitForTabOpenedTo(int tab, GURL url) {
    return Steps(
        PollState(
            kOpenedTabUrlState,
            [this, tab]() {
              auto* const model = browser()->tab_strip_model();
              if (model->active_index() != tab) {
                return GURL();
              }
              return model->GetTabAtIndex(tab)->GetContents()->GetVisibleURL();
            }),
        WaitForState(kOpenedTabUrlState, url),
        StopObservingState(kOpenedTabUrlState));
  }

 protected:
  // Returns a mock user created skill with no ID that could be modified to
  // fit the needs of a particular test case.
  skills::Skill GetMockSkill() {
    return skills::Skill(/*id=*/"", /*name=*/"test_name", /*icon=*/"🧦",
                         /*prompt=*/"test_prompt",
                         /*description=*/"test_description");
  }

  // Returns a mock skill to be used for filling out the Skill dialog.
  skills::Skill GetEditedSkill() {
    return skills::Skill(/*id=*/"", /*name=*/"Edited Skill Name",
                         /*icon=*/"🤩", /*prompt=*/"Edited Instructions",
                         /*description=*/"");
  }

  const DeepQuery kNameInputQuery{"skills-dialog-app", "cr-input#nameText"};
  const DeepQuery kDescriptionInputQuery{"skills-dialog-app",
                                         "textarea#instructionsText"};
  const DeepQuery kEmojiInputQuery{"skills-dialog-app", "input#emojiTrigger"};
  const DeepQuery kSaveButtonQuery{"skills-dialog-app", "cr-button#saveButton"};
  const DeepQuery kCancelButtonQuery{"skills-dialog-app",
                                     "cr-button#cancelButton"};

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, UpdateSkillPreviews) {
  // Create a mock contextual 1P skill.
  auto contextual_skill = GetMockSkill();
  contextual_skill.name = "contextual_skill";
  contextual_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
  std::vector<glic::mojom::SkillPreviewPtr> skill_previews;
  skill_previews.push_back(
      skills::SkillToGlicMojomSkillPreview(&contextual_skill));

  // Create a mock derived skill.
  auto derived_skill = GetMockSkill();
  derived_skill.source =
      sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY;
  glic::mojom::SkillPtr derived_skill_ptr = glic::mojom::Skill::New(
      skills::SkillToGlicMojomSkillPreview(&derived_skill),
      derived_skill.prompt, contextual_skill.id);

  RunTestSequence(OpenGlicAcceptFreAndInstrument(),
                  UpdateContextualSkillPreviews(std::move(skill_previews)),
                  WaitForSkillPreviewShown(contextual_skill.name),
                  AddUserOwnedSkill(std::move(derived_skill_ptr)),
                  WaitForSkillPreviewShown(derived_skill.name));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, InvokeSkill) {
  auto mock_skill = GetMockSkill();
  mock_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
  std::string generated_skill_id;

  glic::mojom::SkillPtr mock_skill_ptr =
      glic::mojom::Skill::New(skills::SkillToGlicMojomSkillPreview(&mock_skill),
                              mock_skill.prompt, std::nullopt);

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(),
      AddUserOwnedSkill(std::move(mock_skill_ptr), &generated_skill_id),
      InvokeSkillDirectly(&generated_skill_id),
      VerifyInvocationInWebUI(mock_skill.prompt));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, UpdateContextualSkill) {
  auto contextual_skill = GetMockSkill();
  contextual_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
  std::vector<glic::mojom::SkillPtr> contextual_skills;
  glic::mojom::SkillPtr skill = glic::mojom::Skill::New(
      skills::SkillToGlicMojomSkillPreview(&contextual_skill),
      contextual_skill.prompt, std::nullopt);
  contextual_skills.push_back(std::move(skill));

  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
  optimization_guide_decider->AddHintForTesting(
      GURL("https://enabled.com/"),
      optimization_guide::proto::OptimizationType::SKILLS,
      SkillVectorToOptimizationMetaData(std::move(contextual_skills)));

  RunTestSequence(
      InstrumentTab(kFirstTabId), OpenGlicAcceptFreAndInstrument(),
      NavigateWebContents(kFirstTabId, GURL("https://enabled.com/")),
      WaitForWebContentsReady(kFirstTabId),
      WaitForSkillPreviewShown(contextual_skill.name));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, ShowManageSkillsUi) {
  const DeepQuery kManageSkillsBtn{{"#manageSkillsBtn"}};
  RunTestSequence(
      InstrumentTab(kFirstTabId), OpenGlicAcceptFreAndInstrument(),
      ClickOnGlicClientElement(kManageSkillsBtn),
      WaitForTabOpenedTo(1, GURL("chrome://skills/yourSkills")),
      ActivateTabAt(0),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      // Click the manage skills button again and verify that it activates the
      // existing tab on chrome://skills without opening a new tab.
      ClickOnGlicClientElement(kManageSkillsBtn), WaitForActiveTabChange(1));
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 2);
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       GetSkill_CreatedViaCreateSkill) {
  auto mock_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), CreateSkill(mock_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      VerifyDialogInput(mock_skill.name, kNameInputQuery),
      VerifyDialogInput(mock_skill.prompt, kDescriptionInputQuery),
      VerifyDialogInput(mock_skill.icon, kEmojiInputQuery),
      EditDialogInput(edited_skill.name, kNameInputQuery),
      EditDialogInput(edited_skill.prompt, kDescriptionInputQuery),
      EditDialogInput(edited_skill.icon, kEmojiInputQuery),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      WaitForSkillPreviewShown(edited_skill.name));

  // Verify skill was saved correctly in SkillsService.
  const auto& user_skills = GetSkillsService()->GetSkills();
  ASSERT_EQ(user_skills.size(), 1u);

  auto added_skill = user_skills[0].get();
  EXPECT_THAT(added_skill, VerifyUserCreatedSkill(edited_skill));

  // Verify that the created skill can be retrieved via the API and is correct.
  auto result = GetSkill(added_skill->id);
  ASSERT_OK_AND_ASSIGN(glic::mojom::SkillPtr mojo_skill, std::move(result));

  EXPECT_EQ(mojo_skill->preview->name, edited_skill.name);
  EXPECT_EQ(mojo_skill->prompt, edited_skill.prompt);
  EXPECT_EQ(mojo_skill->preview->source,
            glic::mojom::SkillSource::kUserCreated);
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       CreateUserSkill_NotSavedOnCancel) {
  auto mock_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), CreateSkill(mock_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      VerifyDialogInput(mock_skill.name, kNameInputQuery),
      VerifyDialogInput(mock_skill.prompt, kDescriptionInputQuery),
      VerifyDialogInput(mock_skill.icon, kEmojiInputQuery),
      EditDialogInput(edited_skill.name, kNameInputQuery),
      EditDialogInput(edited_skill.prompt, kDescriptionInputQuery),
      EditDialogInput(edited_skill.icon, kEmojiInputQuery),
      ClickButtonAndVerifyDialogHides(kCancelButtonQuery));

  // Verify skill was not saved in SkillsService.
  const auto& user_skills = GetSkillsService()->GetSkills();
  ASSERT_EQ(user_skills.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, RemixFirstPartySkill) {
  // Create a first party skill with a valid UUID and source.
  auto mock_skill = GetMockSkill();
  mock_skill.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  mock_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
  auto edited_skill = GetEditedSkill();

  // Add a first party skill to the service.
  GetSkillsService()->AddOrUpdateSkillFromSync(
      mock_skill.id, /*source_skill_id=*/"", mock_skill.name, mock_skill.icon,
      mock_skill.prompt, mock_skill.description,
      /*creation_time=*/base::Time::Now(),
      /*last_update_time=*/base::Time::Now(), mock_skill.source);

  // Create a request to remix the first party skill.
  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), CreateSkill(mock_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      VerifyDialogInput(mock_skill.name, kNameInputQuery),
      VerifyDialogInput(mock_skill.prompt, kDescriptionInputQuery),
      VerifyDialogInput(mock_skill.icon, kEmojiInputQuery),
      EditDialogInput(edited_skill.name, kNameInputQuery),
      EditDialogInput(edited_skill.prompt, kDescriptionInputQuery),
      EditDialogInput(edited_skill.icon, kEmojiInputQuery),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      WaitForSkillPreviewShown(edited_skill.name));
  // Verify skill was saved correctly as a derived skill in SkillsService.
  const skills::Skill* remixed_skill = nullptr;
  for (const auto& skill : GetSkillsService()->GetSkills()) {
    if (skill->source_skill_id == mock_skill.id) {
      remixed_skill = skill.get();
      break;
    }
  }
  ASSERT_TRUE(remixed_skill);
  EXPECT_THAT(remixed_skill,
              VerifyRemixedFirstPartySkill(edited_skill, mock_skill));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       Ensure1PSkillLoadsAfterOpeningGlicPanel) {
  std::string skill_id = "1p_skill_id";
  skills::proto::Skill skill;
  skill.set_id("1p_skill_id");
  skill.set_name("1P Skill Name");
  skill.set_icon("1P Skill Icon");
  skill.set_prompt("1P Skill Prompt");
  skill.set_description("1P Skill Description");

  RunTestSequence(
      Seed1PSkills({skill}), ToggleGlicWindow(GlicWindowMode::kAttached),
      PollForAndAcceptFre(),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      WaitFor1PSkills());
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, Invoke1PSkill) {
  std::string skill_id = "1p_skill_id";
  skills::proto::Skill skill;
  skill.set_id("1p_skill_id");
  skill.set_name("1P Skill Name");
  skill.set_icon("1P Skill Icon");
  skill.set_prompt("1P Skill Prompt");
  skill.set_description("1P Skill Description");

  RunTestSequence(
      Seed1PSkills({skill}), ToggleGlicWindow(GlicWindowMode::kAttached),
      PollForAndAcceptFre(),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      WaitFor1PSkills(), InvokeSkillDirectly(&skill_id),
      VerifyInvocationInWebUI("1P Skill Prompt"));
}
