// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/dependency_graph.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace ash::floating_sso {

class FloatingSsoTest : public policy::PolicyTest {
 public:
  FloatingSsoTest() {
    feature_list_.InitAndEnableFeature(ash::features::kFloatingSso);
  }
  ~FloatingSsoTest() override = default;

 protected:
  void SetFloatingSsoEnabledPolicy(bool policy_value) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies, policy::key::kFloatingSsoEnabled,
                                  base::Value(policy_value));
    provider_.UpdateChromePolicy(policies);
  }

  bool IsFloatingSsoServiceRegistered() {
    std::vector<raw_ptr<DependencyNode, VectorExperimental>> nodes;
    const bool success = BrowserContextDependencyManager::GetInstance()
                             ->GetDependencyGraphForTesting()
                             .GetConstructionOrder(&nodes);
    EXPECT_TRUE(success);
    return base::Contains(
        nodes, "FloatingSsoService",
        [](const DependencyNode* node) -> std::string_view {
          return static_cast<const KeyedServiceBaseFactory*>(node)->name();
        });
  }

  Profile* profile() { return browser()->profile(); }

  FloatingSsoService* floating_sso_service() {
    return FloatingSsoServiceFactory::GetForProfile(profile());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, ServiceRegistered) {
  ASSERT_TRUE(IsFloatingSsoServiceRegistered());
}

// TODO: b/346354327 - this test should check if changing cookies
// results in creation of Sync commits when the policy is enabled or
// disabled. For now it just checks a test-only flag which should be
// deprecated once we can test the intended behavior.
IN_PROC_BROWSER_TEST_F(FloatingSsoTest, CanBeEnabledViaPolicy) {
  const FloatingSsoService& service = CHECK_DEREF(floating_sso_service());
  // Policy is disabled so the service shouldn't be enabled yet.
  EXPECT_FALSE(service.is_enabled_for_testing_);
  // Switch the policy on and off and make sure that the service reacts.
  SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
  EXPECT_TRUE(service.is_enabled_for_testing_);
  SetFloatingSsoEnabledPolicy(/*policy_value=*/false);
  EXPECT_FALSE(service.is_enabled_for_testing_);
}

}  // namespace ash::floating_sso
