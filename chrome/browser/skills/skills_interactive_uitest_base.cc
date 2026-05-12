// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_interactive_uitest_base.h"

#include "base/json/json_writer.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/skills/skills_glic_mojom_util.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_window_controller.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/common/channel_info.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/skills/features.h"
#include "components/skills/internal/skills_downloader.h"
#include "components/skills/internal/skills_service_impl.h"
#include "components/skills/proto/skill.pb.h"
#include "components/sync/model/data_type_store_service.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace skills {

namespace {
static constexpr char kClickFn[] = "el => el.click()";
}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSkillsDialogElementId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kElementEnabled);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kElementOpen);

void PrintTo(const Skill* skill, std::ostream* os) {
  if (!skill) {
    *os << "nullptr";
    return;
  }
  *os << *skill;
}

SkillsInteractiveUiTestBase::SkillsInteractiveUiTestBase() {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kGlic, features::kGlicRollout,
                            features::kSkillsEnabled,
                            features::kGlicMultitabUnderlines},
      /*disabled_features=*/{features::kGlicWarming});
  // TODO(b:504651450): Consider adding support for the new FRE.
}

SkillsInteractiveUiTestBase::~SkillsInteractiveUiTestBase() = default;

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::WaitForElementExists(
    const ui::ElementIdentifier& contents_id,
    const DeepQuery& element) {
  StateChange element_exists;
  element_exists.type = StateChange::Type::kExists;
  element_exists.event = kElementExists;
  element_exists.where = element;
  return WaitForStateChange(contents_id, element_exists);
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::WaitForElementEnabled(
    const ui::ElementIdentifier& contents_id,
    const DeepQuery& element) {
  StateChange element_enabled;
  element_enabled.type = StateChange::Type::kExistsAndConditionTrue;
  element_enabled.event = kElementEnabled;
  element_enabled.where = element;
  element_enabled.test_function = "(el) => !el.disabled";
  return WaitForStateChange(contents_id, element_enabled);
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::WaitForElementOpen(
    const ui::ElementIdentifier& contents_id,
    const DeepQuery& element) {
  StateChange element_visible;
  element_visible.type = StateChange::Type::kExistsAndConditionTrue;
  element_visible.event = kElementOpen;
  element_visible.where = element;
  element_visible.test_function = "(el) => el.open === true";
  return WaitForStateChange(contents_id, element_visible);
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::CheckToastIsShowing(ToastId toast_id) {
  return PollUntil(
      [this, toast_id]() {
        auto* controller = browser()->GetFeatures().toast_controller();
        return controller && controller->IsShowingToast() &&
               controller->GetCurrentToastId() == toast_id;
      },
      "polling until toast is showing");
}

void SkillsInteractiveUiTestBase::SetUpOnMainThread() {
  skills::SkillsFunctionalBrowserTestBase::SetUpOnMainThread();

  skills::SkillsServiceFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&SkillsInteractiveUiTestBase::CreateSkillsService,
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

std::unique_ptr<KeyedService> SkillsInteractiveUiTestBase::CreateSkillsService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<skills::SkillsServiceImpl>(
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile), chrome::GetChannel(),
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_));
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::UpdateContextualSkillPreviews(
    std::vector<glic::mojom::SkillPreviewPtr> contextual_skill_previews) {
  return Steps(Do([this, contextual_skill_previews =
                             std::move(contextual_skill_previews)]() mutable {
    GetGlicInstanceImpl()->host().NotifyContextualSkillsChanged(
        std::move(contextual_skill_previews));
  }));
}

ui::test::InteractiveTestApi::StepBuilder
SkillsInteractiveUiTestBase::AddUserOwnedSkill(
    std::optional<skills::Skill> skill,
    std::string* out_skill_id) {
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

ui::test::InteractiveTestApi::StepBuilder
SkillsInteractiveUiTestBase::InvokeSkillDirectly(std::string* skill_id_ptr) {
  return Do([this, skill_id_ptr]() {
    skills::SkillsUiWindowController::From(browser())->InvokeSkill(
        *skill_id_ptr);
  });
}

ui::test::InteractiveTestApi::StepBuilder
SkillsInteractiveUiTestBase::CreateSkill(skills::Skill skill) {
  return Do([this, skill = std::move(skill)]() {
    auto request = glic::mojom::CreateSkillRequest::New(
        skill.id, skill.name, skill.icon,
        skills::SyncPbToGlicMojomSkillSource(skill.source), skill.prompt,
        skill.description);
    skills::SkillsFunctionalBrowserTestBase::CreateSkill(std::move(request));
  });
}

ui::test::InteractiveTestApi::StepBuilder
SkillsInteractiveUiTestBase::UpdateSkill(const std::string* skill_id_ptr) {
  return Do([this, skill_id_ptr]() {
    auto request = glic::mojom::UpdateSkillRequest::New(*skill_id_ptr);
    skills::SkillsFunctionalBrowserTestBase::UpdateSkill(std::move(request));
  });
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::VerifyInvocationInWebUI(
    const std::string& expected_prompt) {
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

ui::test::InteractiveTestApi::StepBuilder
SkillsInteractiveUiTestBase::EditDialogInput(const std::string& value,
                                             const DeepQuery& query) {
  return ExecuteJsAt(
      kSkillsDialogElementId, query,
      base::StringPrintf(
          "el => {"
          "  el.value = '%s';"
          "  el.dispatchEvent(new Event('input', { bubbles: true }));"
          "}",
          value.c_str()));
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::VerifyDialogInput(
    const std::string& expected_value,
    const DeepQuery& query) {
  return Steps(WaitForElementExists(kSkillsDialogElementId, query),
               CheckJsResultAt(kSkillsDialogElementId, query, "el => el.value",
                               expected_value));
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::VerifyAndEditSkillDialogInput(
    const skills::Skill& verify_input,
    const skills::Skill& edit_input) {
  return Steps(VerifyDialogInput(verify_input.name, kNameInputQuery),
               VerifyDialogInput(verify_input.prompt, kDescriptionInputQuery),
               VerifyDialogInput(verify_input.icon, kEmojiInputQuery),
               EditDialogInput(edit_input.name, kNameInputQuery),
               EditDialogInput(edit_input.prompt, kDescriptionInputQuery),
               EditDialogInput(edit_input.icon, kEmojiInputQuery));
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::ClickButtonAndVerifyDialogHides(
    const DeepQuery& button_query) {
  return Steps(WaitForElementEnabled(kSkillsDialogElementId, button_query),
               MoveMouseTo(kSkillsDialogElementId, button_query), ClickMouse(),
               // Verify the dialog hides.
               WaitForHide(skills::SkillsDialogView::kSkillsDialogElementId));
}

ui::test::InteractiveTestApi::StepBuilder
SkillsInteractiveUiTestBase::ActivateTabAt(int tab_index) {
  return Do([this, tab_index]() {
    browser()->GetTabStripModel()->ActivateTabAt(tab_index);
  });
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::WaitForSkillPreviewShown(
    std::string_view skill_name) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSkillPreviewShown);
  StateChange state_change;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.where = {"#skillsList > li > span.skill-name[value=\"" +
                        std::string(skill_name) + "\"]"};
  state_change.test_function = "el => el.checkVisibility()";
  state_change.event = kSkillPreviewShown;
  return WaitForStateChange(glic::test::kGlicContentsElementId, state_change);
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::WaitForSkillPreviewOrder(
    const std::vector<std::string>& skill_names) {
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

ui::test::InteractiveTestApi::StepBuilder
SkillsInteractiveUiTestBase::ClickOnGlicClientElement(DeepQuery where) {
  return ExecuteJsAt(glic::test::kGlicContentsElementId, where, kClickFn);
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::OpenGlicAndInstrument() {
  return Steps(ToggleGlicWindow(GlicWindowMode::kAttached),
               WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents));
}

ui::test::InteractiveTestApi::StepBuilder
SkillsInteractiveUiTestBase::Seed1PSkills(
    const std::vector<skills::proto::Skill>& skills) {
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

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::WaitFor1PSkills() {
  return PollUntil(
      [this]() {
        return !skills::SkillsServiceFactory::GetForProfile(
                    browser()->profile())
                    ->Get1PSkills()
                    .empty();
      },
      "polling until 1P skills are not empty.");
}

ui::test::InteractiveTestApi::MultiStep
SkillsInteractiveUiTestBase::WaitForTabOpenedTo(int tab, GURL url) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<GURL>,
                                      kOpenedTabUrlState);
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

skills::Skill SkillsInteractiveUiTestBase::GetMockSkill() {
  return skills::Skill(/*id=*/"", /*name=*/"test_name", /*icon=*/"🧦",
                       /*prompt=*/"test_prompt",
                       /*description=*/"test_description");
}

skills::Skill SkillsInteractiveUiTestBase::GetEditedSkill() {
  return skills::Skill(/*id=*/"", /*name=*/"Edited Skill Name",
                       /*icon=*/"🤩", /*prompt=*/"Edited Instructions",
                       /*description=*/"");
}

skills::proto::Skill SkillsInteractiveUiTestBase::GetFirstPartySkillProto(
    std::optional<skills::Skill> skill_to_convert) {
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

WebContentsInteractionTestUtil::DeepQuery
SkillsInteractiveUiTestBase::GetSkillCardQuery(const std::string& sub_element) {
  return {"skills-app", "user-skills-page", "skill-card", sub_element};
}

MATCHER_P(VerifyUserCreatedSkillMatcher, expected, "") {
  // Ensures that the skill matches the expected values and that has a "user
  // created" source and a valid ID.
  return arg->name == expected.name && arg->icon == expected.icon &&
         arg->prompt == expected.prompt &&
         arg->source == sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED &&
         arg->source_skill_id.empty() &&
         base::Uuid::ParseLowercase(arg->id).is_valid();
}

MATCHER_P2(VerifyRemixedFirstPartySkillMatcher, expected, source, "") {
  // Ensures that the skill matches the expected values and that has a "derived
  // from first party" source and a valid ID different from the source skill.
  return arg->name == expected.name && arg->icon == expected.icon &&
         arg->prompt == expected.prompt &&
         arg->source ==
             sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY &&
         arg->source_skill_id == source.id && arg->id != source.id &&
         base::Uuid::ParseLowercase(arg->id).is_valid();
}

testing::Matcher<const Skill*> VerifyUserCreatedSkill(const Skill& expected) {
  return VerifyUserCreatedSkillMatcher(expected);
}

testing::Matcher<const Skill*> VerifyRemixedFirstPartySkill(
    const Skill& expected,
    const Skill& source) {
  return VerifyRemixedFirstPartySkillMatcher(expected, source);
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

}  // namespace skills
