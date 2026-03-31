// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "base/values.h"
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
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
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
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/base/interaction/interactive_test.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSkillsDialogElementId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<GURL>,
                                    kOpenedTabUrlState);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementEnabled);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementOpen);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

static constexpr char kClickFn[] = "el => el.click()";

}  // namespace

namespace skills {
void PrintTo(const Skill* skill, std::ostream* os) {
  if (!skill) {
    *os << "nullptr";
    return;
  }
  *os << *skill;
}
}  // namespace skills

namespace {

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

  ui::test::InteractiveTestApi::MultiStep WaitForElementOpen(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    StateChange element_visible;
    element_visible.type = WebContentsInteractionTestUtil::StateChange::Type::
        kExistsAndConditionTrue;
    element_visible.event = kElementOpen;
    element_visible.where = element;
    element_visible.test_function = "(el) => el.open === true";
    return WaitForStateChange(contents_id, element_visible);
  }

  ui::test::InteractiveTestApi::MultiStep CheckToastIsShowing(
      ToastId toast_id) {
    return PollUntil(
        [this, toast_id]() {
          auto* controller = browser()->GetFeatures().toast_controller();
          return controller && controller->IsShowingToast() &&
                 controller->GetCurrentToastId() == toast_id;
        },
        "polling until toast is showing");
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

  // Adds a user owned skill to the SkillsService and optionally sets
  // `out_skill_id` if provided and if `skill` was added successfully.
  auto AddUserOwnedSkill(std::optional<skills::Skill> skill = std::nullopt,
                         std::string* out_skill_id = nullptr) {
    return Do([this, skill = std::move(skill), out_skill_id]() mutable {
      bool has_value = skill.has_value();
      skills::Skill skill_to_add = std::move(skill).value_or(GetMockSkill());
      if (!has_value) {
        skill_to_add.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
      }

      auto* added_skill =
          GetSkillsService()->AddSkill(skill_to_add.id, skill_to_add.name,
                                       skill_to_add.icon, skill_to_add.prompt);

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

  auto UpdateSkill(const std::string* skill_id_ptr) {
    return Do([this, skill_id_ptr]() {
      auto request = glic::mojom::UpdateSkillRequest::New(*skill_id_ptr);
      skills::SkillsFunctionalBrowserTestBase::UpdateSkill(std::move(request));
    });
  }

  // Verifies that the value of the skills prompt input in the test client
  // matches `expected_prompt`. Observes the getSkillToInvoke() API endpoint.
  auto VerifyInvocationInWebUI(const std::string& expected_prompt) {
    return Steps(
        Log("Verifying Glic Panel Opened via Toast Interaction"),
        WaitForShow(glic::test::kGlicHostElementId),

        WaitForJsResult(
            glic::test::kGlicContentsElementId,
            base::StringPrintf(
                "() => {"
                "  const input = document.getElementById('skillPromptInput');"
                "  return !!input && input.value === '%s';"
                "}",
                expected_prompt.c_str())));
  }

  // Edits the Skills dialog input at `query` to `value`.
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

  // Verifies that the value of the Skills dialog input at `query` matches
  // `expected_value`.
  auto VerifyDialogInput(const std::string& expected_value,
                         const DeepQuery& query) {
    return Steps(WaitForElementExists(kSkillsDialogElementId, query),
                 CheckJsResultAt(kSkillsDialogElementId, query,
                                 "el => el.value", expected_value));
  }

  // Verifies that the dialog inputs match the values in `verify_input`,
  // and then edits them to match the values in `edit_input`.
  auto VerifyAndEditSkillDialogInput(const skills::Skill& verify_input,
                                     const skills::Skill& edit_input) {
    return Steps(VerifyDialogInput(verify_input.name, kNameInputQuery),
                 VerifyDialogInput(verify_input.prompt, kDescriptionInputQuery),
                 VerifyDialogInput(verify_input.icon, kEmojiInputQuery),
                 EditDialogInput(edit_input.name, kNameInputQuery),
                 EditDialogInput(edit_input.prompt, kDescriptionInputQuery),
                 EditDialogInput(edit_input.icon, kEmojiInputQuery));
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

  // Waits for a SkillPreview with `skill_name` to be shown in the test client's
  // Skills list (Observes the getSkillPreviews() API endpoint).
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

  // Waits for the SkillPreviews in the test client's Skills list to be in the
  // same order as the given skill names.
  auto WaitForSkillPreviewOrder(const std::vector<std::string>& skill_names) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSkillPreviewOrderMatched);
    // Construct a JSON array of the expected names.
    base::ListValue expected_list;
    for (const auto& name : skill_names) {
      expected_list.Append(name);
    }
    std::string expected_json;
    base::JSONWriter::Write(expected_list, &expected_json);

    StateChange state_change;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.where = {"#skillsList"};
    // Ensures that the skill names in the test client match the expected order.
    state_change.test_function = base::StringPrintf(
        "(el) => {"
        "  const names = Array.from(el.querySelectorAll('.skill-name'), "
        "span => span.getAttribute('value'));"
        "  return JSON.stringify(names) === JSON.stringify(%s);"
        "}",
        expected_json.c_str());
    state_change.event = kSkillPreviewOrderMatched;
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

  skills::proto::Skill GetFirstPartySkillProto(
      std::optional<skills::Skill> skill_to_convert = std::nullopt) {
    skills::Skill skill = std::move(skill_to_convert).value_or(GetMockSkill());
    if (skill.id.empty()) {
      skill.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
    }

    skills::proto::Skill proto_skill;
    proto_skill.set_id(skill.id);
    proto_skill.set_name(skill.name);
    proto_skill.set_icon(skill.icon);
    proto_skill.set_prompt(skill.prompt);
    proto_skill.set_description(skill.description);
    return proto_skill;
  }

  const DeepQuery kNameInputQuery{"skills-dialog-app", "cr-input#nameText"};
  const DeepQuery kDescriptionInputQuery{"skills-dialog-app",
                                         "textarea#instructionsText"};
  const DeepQuery kEmojiInputQuery{"skills-dialog-app", "input#emojiTrigger"};
  const DeepQuery kSaveButtonQuery{"skills-dialog-app", "cr-button#saveButton"};
  const DeepQuery kCancelButtonQuery{"skills-dialog-app",
                                     "cr-button#cancelButton"};

  DeepQuery GetSkillCardQuery(const std::string& sub_element) {
    return {"skills-app", "user-skills-page", "skill-card", sub_element};
  }

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
                  AddUserOwnedSkill(derived_skill),
                  WaitForSkillPreviewShown(derived_skill.name));
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
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), CreateSkill(user_created_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(user_created_skill, edited_skill),
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
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), CreateSkill(user_created_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(user_created_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kCancelButtonQuery));

  // Verify skill was not saved in SkillsService.
  const auto& user_skills = GetSkillsService()->GetSkills();
  ASSERT_EQ(user_skills.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, UpdateUserSkill) {
  // Used to initially create the skill and verify the dialog
  // contents when update is triggered.
  auto user_created_skill = GetMockSkill();
  // Used to update the skill via the dialog and verify that
  // the skill was updated correctly in SkillsService.
  auto edited_skill = GetEditedSkill();
  // Used to update the skill via the API.
  std::string skill_id;

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(),
      AddUserOwnedSkill(user_created_skill, &skill_id),
      WaitForSkillPreviewShown(user_created_skill.name), UpdateSkill(&skill_id),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(user_created_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      WaitForSkillPreviewShown(edited_skill.name));

  // Verify skill was saved correctly in SkillsService.
  const auto* updated_skill = GetSkillsService()->GetSkillById(skill_id);
  ASSERT_TRUE(updated_skill);
  EXPECT_THAT(updated_skill, VerifyUserCreatedSkill(edited_skill));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       UpdateUserSkill_NotSavedOnCancel) {
  // `user_created_skill` is used to initially create the skill and verify the
  // dialog contents when update is triggered. It is then used to verify that
  // the skill was not changed in SkillsService.
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();
  std::string skill_id;

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(),
      AddUserOwnedSkill(user_created_skill, &skill_id),
      WaitForSkillPreviewShown(user_created_skill.name), UpdateSkill(&skill_id),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(user_created_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kCancelButtonQuery));

  // Verify skill was NOT changed in SkillsService.
  const auto* updated_skill = GetSkillsService()->GetSkillById(skill_id);
  ASSERT_TRUE(updated_skill);
  EXPECT_THAT(updated_skill, VerifyUserCreatedSkill(user_created_skill));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       RemixFirstPartySkillAndUpdateSkill) {
  // Create a first party skill with a valid UUID and source.
  auto first_party_skill = GetMockSkill();
  first_party_skill.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  first_party_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
  // Used to edit the remixed skill in the creation dialog.
  auto edited_skill = GetEditedSkill();
  // Used to update the remixed skill in the update dialog.
  auto updated_skill =
      skills::Skill(/*id=*/"", /*name=*/"Updated Skill Name",
                    /*icon=*/"⭐", /*prompt=*/"Updated Instructions",
                    /*description=*/"");

  std::string remixed_skill_id = "";

  // Add a first party skill to the service.
  GetSkillsService()->AddOrUpdateSkillFromSync(
      first_party_skill.id, /*source_skill_id=*/"", first_party_skill.name,
      first_party_skill.icon, first_party_skill.prompt,
      first_party_skill.description,
      /*creation_time=*/base::Time::Now(),
      /*last_update_time=*/base::Time::Now(), first_party_skill.source);

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), CreateSkill(first_party_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(first_party_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      WaitForSkillPreviewShown(edited_skill.name),
      Do([this, &first_party_skill, &edited_skill, &remixed_skill_id]() {
        // Verify skill was saved correctly as a derived skill in SkillsService.
        const skills::Skill* remixed_skill = nullptr;

        for (const auto& skill : GetSkillsService()->GetSkills()) {
          if (skill->source_skill_id == first_party_skill.id) {
            remixed_skill = skill.get();
            remixed_skill_id = skill->id;
            break;
          }
        }
        ASSERT_TRUE(remixed_skill);
        EXPECT_THAT(remixed_skill, VerifyRemixedFirstPartySkill(
                                       edited_skill, first_party_skill));
        ASSERT_FALSE(remixed_skill_id.empty());
      }),
      // Call UpdateSkill on the remixed skill.
      UpdateSkill(&remixed_skill_id),
      UninstrumentWebContents(kSkillsDialogElementId,
                              /*fail_if_not_instrumented=*/false),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(edited_skill, updated_skill),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      WaitForSkillPreviewShown(updated_skill.name));
  // Verify skill was updated correctly in SkillsService.
  const auto* updated_remixed_skill =
      GetSkillsService()->GetSkillById(remixed_skill_id);
  ASSERT_TRUE(updated_remixed_skill);
  EXPECT_THAT(updated_remixed_skill,
              VerifyRemixedFirstPartySkill(updated_skill, first_party_skill));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       DISABLED_UpdateSkillSortsByLastUpdateTime) {
  std::string updated_skill_name = "Updated Skill";
  std::string first_party_skill_name = "1P Skill";

  std::vector<skills::Skill> test_skills;
  for (int i = 0; i <= 2; ++i) {
    auto skill = GetMockSkill();
    skill.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
    skill.name = base::StringPrintf("Skill %d", i);
    test_skills.push_back(std::move(skill));
  }

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), Do([this, test_skills]() {
        for (const auto& skill : test_skills) {
          GetSkillsService()->AddOrUpdateSkillFromSync(
              skill.id, /*source_skill_id=*/"", skill.name, skill.icon,
              skill.prompt, skill.description,
              /*creation_time=*/base::Time::Now(),
              /*last_update_time=*/base::Time::Now(),
              sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED);
        }
      }),
      // SkillPreviews should be sorted by last update time in descending order.
      WaitForSkillPreviewOrder(
          {test_skills[2].name, test_skills[1].name, test_skills[0].name}),
      // Update oldest skill (Skill 0).
      UpdateSkill(&test_skills[0].id),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      EditDialogInput(updated_skill_name, kNameInputQuery),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      // Skill 0 should now move to the top because it was just updated.
      WaitForSkillPreviewOrder(
          {updated_skill_name, test_skills[2].name, test_skills[1].name}),
      // Add a 1P skill with a creation and last update time older than all
      // other user created skills.
      Do([this, first_party_skill_name]() {
        auto older_time = base::Time::Now() - base::Minutes(10);

        GetSkillsService()->AddOrUpdateSkillFromSync(
            base::Uuid::GenerateRandomV4().AsLowercaseString(),
            /*source_skill_id=*/"", first_party_skill_name, "icon", "prompt",
            "description", /*creation_time=*/older_time,
            /*last_update_time=*/older_time,
            sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY);
      }),
      // Ensure the 1P skill is at the end of the list.
      WaitForSkillPreviewOrder({updated_skill_name, test_skills[2].name,
                                test_skills[1].name, first_party_skill_name}));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, Invoke1PSkillFromFloatyGic) {
  skills::proto::Skill skill_proto = GetFirstPartySkillProto();
  std::string skill_id = skill_proto.id();
  RunTestSequence(
      Seed1PSkills({skill_proto}), ToggleGlicWindow(GlicWindowMode::kDetached),
      PollForAndAcceptFre(),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      WaitFor1PSkills(), InvokeSkillDirectly(&skill_id),
      VerifyInvocationInWebUI(skill_proto.prompt()));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, Invoke1PSkill) {
  skills::proto::Skill skill_proto = GetFirstPartySkillProto();
  std::string skill_id = skill_proto.id();
  RunTestSequence(
      Seed1PSkills({skill_proto}), ToggleGlicWindow(GlicWindowMode::kAttached),
      PollForAndAcceptFre(),
      WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents),
      WaitFor1PSkills(), InvokeSkillDirectly(&skill_id),
      VerifyInvocationInWebUI(skill_proto.prompt()));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       NotifySkillToInvokeChanged_UpdatesGetSkillToInvoke) {
  auto skill = GetMockSkill();
  std::string generated_skill_id;

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(),
      AddUserOwnedSkill(skill, &generated_skill_id),
      // Simulate a notification that the skill to invoke has changed.
      Do([this, skill, skill_id_ptr = &generated_skill_id]() mutable {
        skill.id = *skill_id_ptr;
        auto mojo_skill = glic::mojom::Skill::New(
            skills::SkillToGlicMojomSkillPreview(&skill), skill.prompt,
            /*source_skill_id=*/std::nullopt);
        glic_service()
            ->GetInstanceForTab(browser()->GetActiveTabInterface())
            ->host()
            .NotifySkillToInvokeChanged(std::move(mojo_skill));
      }),
      // Verify the getSkillToInvoke() endpoint reflects the update.
      VerifyInvocationInWebUI(skill.prompt));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest, InvokeCreatedSkillViaToast) {
  auto user_created_skill = GetMockSkill();
  auto edited_skill = GetEditedSkill();

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), CreateSkill(user_created_skill),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      VerifyAndEditSkillDialogInput(user_created_skill, edited_skill),
      ClickButtonAndVerifyDialogHides(kSaveButtonQuery),
      // Wait for the "Skill saved" toast and click its action button.
      CheckToastIsShowing(ToastId::kSkillSaved),
      PressButton(toasts::ToastView::kToastActionButton),
      // Verify the action button invoked the created skill.
      VerifyInvocationInWebUI(edited_skill.prompt));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       UpdateAndInvokeContextualSkill) {
  auto contextual_skill = GetMockSkill();
  contextual_skill.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  contextual_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;

  // Add a contextual skill to the optimization guide decider.
  std::vector<glic::mojom::SkillPtr> contextual_skills;
  contextual_skills.push_back(glic::mojom::Skill::New(
      skills::SkillToGlicMojomSkillPreview(&contextual_skill),
      contextual_skill.prompt, std::nullopt));

  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
  optimization_guide_decider->AddHintForTesting(
      GURL("https://enabled.com/"),
      optimization_guide::proto::OptimizationType::SKILLS,
      SkillVectorToOptimizationMetaData(std::move(contextual_skills)));

  // Add the contextual skill to SkillsService.
  skills::proto::Skill skill_proto = GetFirstPartySkillProto(contextual_skill);

  RunTestSequence(
      Seed1PSkills({skill_proto}), InstrumentTab(kFirstTabId),
      OpenGlicAcceptFreAndInstrument(),
      // Navigate to the site with contextual hint for `contextual_skill`.
      NavigateWebContents(kFirstTabId, GURL("https://enabled.com/")),
      WaitForWebContentsReady(kFirstTabId), WaitFor1PSkills(),
      WaitForSkillPreviewShown(contextual_skill.name),
      // Invoke the contextual skill and verify invocation WebUI.
      InvokeSkillDirectly(&contextual_skill.id),
      VerifyInvocationInWebUI(contextual_skill.prompt));
}

IN_PROC_BROWSER_TEST_F(SkillsInteractiveUiTest,
                       DeleteAndUndoSkillUpdatesSkillPreviews) {
  auto user_created_skill = GetMockSkill();

  const DeepQuery kMenuButtonQuery =
      GetSkillCardQuery("cr-icon-button#moreButton");
  const DeepQuery kMenuDropdownQuery = GetSkillCardQuery("cr-action-menu#menu");
  const DeepQuery kDeleteButtonQuery =
      GetSkillCardQuery("cr-button#deleteButton");

  RunTestSequence(
      OpenGlicAcceptFreAndInstrument(), AddUserOwnedSkill(user_created_skill),
      WaitForSkillPreviewOrder({user_created_skill.name}),
      // Navigate to the "Your Skills" page.
      InstrumentTab(kFirstTabId),
      NavigateWebContents(kFirstTabId,
                          GURL(chrome::kChromeUISkillsURL)
                              .Resolve(chrome::kChromeUISkillsYourSkillsPath)),
      WaitForWebContentsReady(kFirstTabId),
      // Delete the skill via the skill card.
      WaitForElementExists(kFirstTabId, kMenuButtonQuery),
      MoveMouseTo(kFirstTabId, kMenuButtonQuery), ClickMouse(),
      WaitForElementOpen(kFirstTabId, kMenuDropdownQuery),
      MoveMouseTo(kFirstTabId, kDeleteButtonQuery), ClickMouse(),
      // Verify deletion and toast is showing.
      WaitForSkillPreviewOrder({}), CheckToastIsShowing(ToastId::kSkillDeleted),
      // Undo the deletion and verify that the skill is restored.
      PressButton(toasts::ToastView::kToastActionButton),
      WaitForSkillPreviewOrder({user_created_skill.name}));
}
