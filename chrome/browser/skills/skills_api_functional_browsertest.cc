// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/skills/skills_functional_browsertest.h"
#include "chrome/browser/skills/skills_ui_tab_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "components/skills/public/skill.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {
namespace {

using glic::test::ErrorHasSubstr;

class SkillsApiFunctionalBrowserTest : public SkillsFunctionalBrowserTestBase {
 public:
  SkillsApiFunctionalBrowserTest() = default;
  ~SkillsApiFunctionalBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SkillsFunctionalBrowserTestBase::SetUpOnMainThread();
    RunTestSequence(OpenGlic());
  }
};

// Verify that a skill added to the service can be retrieved via Mojo.
IN_PROC_BROWSER_TEST_F(SkillsApiFunctionalBrowserTest,
                       GetSkill_ReturnsCorrectSkill) {
  const std::string kName = "test_skill_name";
  const std::string kPrompt = "test_skill_prompt";
  const skills::Skill* added_skill =
      GetSkillsService()->AddSkill(/*source_skill_id=*/"", /*name=*/kName,
                                   /*icon=*/"🧪", /*prompt=*/kPrompt);
  ASSERT_TRUE(added_skill);

  auto result = GetSkill(added_skill->id);

  ASSERT_OK_AND_ASSIGN(glic::mojom::SkillPtr mojo_skill, std::move(result));
  ASSERT_TRUE(mojo_skill);

  EXPECT_EQ(mojo_skill->preview->name, kName);
  EXPECT_EQ(mojo_skill->prompt, kPrompt);
  EXPECT_EQ(mojo_skill->preview->source,
            glic::mojom::SkillSource::kUserCreated);
}

// Verify that requesting a non-existent ID throws a JS error.
IN_PROC_BROWSER_TEST_F(SkillsApiFunctionalBrowserTest,
                       GetSkill_InvalidIdThrowsError) {
  auto result = GetSkill("invalid");
  EXPECT_THAT(result, ErrorHasSubstr("getSkill: failed"));
}

// Verify that requesting a skill when the service is not ready returns an
// error.
IN_PROC_BROWSER_TEST_F(SkillsApiFunctionalBrowserTest,
                       GetSkill_ServiceNotReady) {
  // Simulate the service not being ready.
  GetSkillsService()->SetServiceStatusForTesting(
      skills::SkillsService::ServiceStatus::kNotInitialized);

  const std::string kName = "test_skill_name";
  const std::string kPrompt = "test_skill_prompt";
  const skills::Skill* added_skill =
      GetSkillsService()->AddSkill(/*source_skill_id=*/"", /*name=*/kName,
                                   /*icon=*/"🧪", /*prompt=*/kPrompt);
  ASSERT_FALSE(added_skill);

  auto result = GetSkill("some_id");
  EXPECT_THAT(result, ErrorHasSubstr("getSkill: failed"));
}

}  // namespace
}  // namespace skills
