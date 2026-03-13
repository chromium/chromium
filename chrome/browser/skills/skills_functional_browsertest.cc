// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_functional_browsertest.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "components/skills/features.h"
#include "content/public/test/browser_test_utils.h"

namespace skills {
namespace {
base::expected<std::string, std::string> GetStringValue(
    const base::DictValue& dict,
    std::string_view key,
    std::string_view error_context) {
  const std::string* value = dict.FindString(key);
  if (!value) {
    return base::unexpected("Missing '" + std::string(key) + "' field in " +
                            std::string(error_context) + ".");
  }
  return base::ok(*value);
}

base::expected<int, std::string> GetIntValue(const base::DictValue& dict,
                                             std::string_view key,
                                             std::string_view error_context) {
  std::optional<int> value = dict.FindInt(key);
  if (!value) {
    return base::unexpected("Missing '" + std::string(key) + "' field in " +
                            std::string(error_context) + ".");
  }
  return base::ok(*value);
}
}  // namespace

SkillsFunctionalBrowserTestBase::SkillsFunctionalBrowserTestBase() {
  scoped_feature_list_.InitWithFeatures(
      {features::kSkillsEnabled, features::kGlic, features::kGlicRollout}, {});
}

SkillsFunctionalBrowserTestBase::~SkillsFunctionalBrowserTestBase() = default;

void SkillsFunctionalBrowserTestBase::SetUpOnMainThread() {
  glic::test::GlicFunctionalBrowserTestBase::SetUpOnMainThread();
  GetSkillsService()->SetServiceStatusForTesting(
      skills::SkillsService::ServiceStatus::kReady);
}

skills::SkillsService* SkillsFunctionalBrowserTestBase::GetSkillsService() {
  return skills::SkillsServiceFactory::GetForProfile(browser()->profile());
}

void SkillsFunctionalBrowserTestBase::CreateSkill(
    const glic::mojom::CreateSkillRequestPtr& request) {
  base::DictValue dict;
  if (!request->id.empty()) {
    dict.Set("id", request->id);
  }
  if (!request->name.empty()) {
    dict.Set("name", request->name);
  }
  if (!request->icon.empty()) {
    dict.Set("icon", request->icon);
  }
  if (!request->prompt.empty()) {
    dict.Set("prompt", request->prompt);
  }
  if (!request->description.empty()) {
    dict.Set("description", request->description);
  }
  dict.Set("source", static_cast<int>(request->source));

  EXPECT_OK(EvalJsInGlic(content::JsReplace(
      "window.client.browser.createSkill($1)", std::move(dict))));
}

void SkillsFunctionalBrowserTestBase::UpdateSkill(
    const glic::mojom::UpdateSkillRequestPtr& request) {
  base::DictValue dict;
  CHECK(!request->id.empty());
  dict.Set("id", request->id);

  EXPECT_OK(EvalJsInGlic(content::JsReplace(
      "window.client.browser.updateSkill($1)", std::move(dict))));
}

base::expected<glic::mojom::SkillPtr, std::string>
SkillsFunctionalBrowserTestBase::GetSkill(const std::string& skill_id) {
  ASSIGN_OR_RETURN(base::Value result,
                   EvalJsInGlic(content::JsReplace(
                       "window.client.browser.getSkill($1)", skill_id)));
  return ToMojoSkill(result);
}

base::expected<glic::mojom::SkillPtr, std::string>
SkillsFunctionalBrowserTestBase::ToMojoSkill(const base::Value& value) {
  if (!value.is_dict()) {
    return base::unexpected("Skill value is not a dictionary.");
  }

  const base::DictValue* preview_dict = value.GetDict().FindDict("preview");
  if (!preview_dict) {
    return base::unexpected("Missing 'preview' field in Skill base::Value.");
  }

  auto preview = glic::mojom::SkillPreview::New();

  ASSIGN_OR_RETURN(preview->id,
                   GetStringValue(*preview_dict, "id", "SkillPreview"));
  ASSIGN_OR_RETURN(preview->name,
                   GetStringValue(*preview_dict, "name", "SkillPreview"));
  ASSIGN_OR_RETURN(preview->icon,
                   GetStringValue(*preview_dict, "icon", "SkillPreview"));
  ASSIGN_OR_RETURN(
      preview->description,
      GetStringValue(*preview_dict, "description", "SkillPreview"));

  ASSIGN_OR_RETURN(int source,
                   GetIntValue(*preview_dict, "source", "SkillPreview"));
  preview->source = static_cast<glic::mojom::SkillSource>(source);

  auto skill = glic::mojom::Skill::New();
  skill->preview = std::move(preview);

  ASSIGN_OR_RETURN(skill->prompt, GetStringValue(value.GetDict(), "prompt",
                                                 "Skill base::Value"));

  // sourceSkillId is optional.
  const std::string* source_skill_id =
      value.GetDict().FindString("sourceSkillId");
  if (source_skill_id) {
    skill->source_skill_id = *source_skill_id;
  }

  return base::ok(std::move(skill));
}

}  // namespace skills
