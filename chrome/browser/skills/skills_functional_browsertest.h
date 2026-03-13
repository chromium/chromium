// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_FUNCTIONAL_BROWSERTEST_H_
#define CHROME_BROWSER_SKILLS_SKILLS_FUNCTIONAL_BROWSERTEST_H_

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/glic_functional_browsertest.h"
#include "components/skills/public/skills_service.h"

namespace skills {

// Base class for Chrome Skills functional tests.
// Provides shared helpers for interacting with the Skills-related endpoints of
// the Glic API, with support for Mojo struct validation.
// NOTE: This class focuses on wrapping API endpoints that take in arguments and
// do not return observable data.
// To validate those, modify and listen on changes to the Skills section of the
// test client.
class SkillsFunctionalBrowserTestBase
    : public glic::test::GlicFunctionalBrowserTestBase {
 public:
  SkillsFunctionalBrowserTestBase();
  ~SkillsFunctionalBrowserTestBase() override;

 protected:
  void SetUpOnMainThread() override;

  skills::SkillsService* GetSkillsService();

  // Helper to call createSkill via the TS API (triggers UI).
  void CreateSkill(const glic::mojom::CreateSkillRequestPtr& request);

  // Helper to call updateSkill via the TS API (triggers UI).
  void UpdateSkill(const glic::mojom::UpdateSkillRequestPtr& request);

  // Helper to call getSkill via the TS API and return a Mojo struct.
  base::expected<glic::mojom::SkillPtr, std::string> GetSkill(
      const std::string& skill_id);

  // Conversion helpers from base::Value (JS response) to Mojo structs.
  base::expected<glic::mojom::SkillPtr, std::string> ToMojoSkill(
      const base::Value& value);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_FUNCTIONAL_BROWSERTEST_H_
