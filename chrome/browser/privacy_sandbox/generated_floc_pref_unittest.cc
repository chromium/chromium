// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/generated_floc_pref.h"

#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

namespace settings_private = extensions::settings_private;

typedef settings_private::GeneratedPrefTestBase GeneratedFlocPrefTest;

TEST_F(GeneratedFlocPrefTest, SetPreference) {
  // Confirm that the backing preference is updated appropriately, or if not,
  // the appropriate error is returned.
  auto pref = std::make_unique<GeneratedFlocPref>(profile());

  // Disabling the Privacy Sandbox APIs pref should prevent the generated pref
  // from being changed, and the backing real pref should not change.
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kPrivacySandboxFlocEnabled,
                       std::make_unique<base::Value>(false));
  EXPECT_EQ(settings_private::SetPrefResult::PREF_NOT_MODIFIABLE,
            pref->SetPref(std::make_unique<base::Value>(true).get()));
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxFlocEnabled));

  // Enabling the Privacy Sandbox APIs pref should allow the generated pref to
  // change, as well as the backing real pref.
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabled,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(settings_private::SetPrefResult::SUCCESS,
            pref->SetPref(std::make_unique<base::Value>(true).get()));
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxFlocEnabled));

  // The pref should only accept boolean values.
  EXPECT_EQ(settings_private::SetPrefResult::PREF_TYPE_MISMATCH,
            pref->SetPref(std::make_unique<base::Value>(23).get()));

  // Disabling the Privacy Sandbox APIs pref via management should also prevent
  // the generated pref & real pref from changing.
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxFlocEnabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetManagedPref(prefs::kPrivacySandboxApisEnabled,
                          std::make_unique<base::Value>(false));

  EXPECT_EQ(settings_private::SetPrefResult::PREF_NOT_MODIFIABLE,
            pref->SetPref(std::make_unique<base::Value>(true).get()));
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxFlocEnabled));
}

TEST_F(GeneratedFlocPrefTest, GetPreference) {
  // The generated preference should correctly reflect the effective state
  // of FLoC, rather than simply the real pref.
  auto pref = std::make_unique<GeneratedFlocPref>(profile());

  // When the Privacy Sandbox APIs pref is disabled, the generated pref should
  // be disabled with user control also disabled, regardless of the real FLoC
  // pref state.
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kPrivacySandboxFlocEnabled,
                       std::make_unique<base::Value>(false));
  EXPECT_FALSE(pref->GetPrefObject()->value->GetBool());
  EXPECT_TRUE(*pref->GetPrefObject()->user_control_disabled);

  prefs()->SetUserPref(prefs::kPrivacySandboxFlocEnabled,
                       std::make_unique<base::Value>(true));
  EXPECT_FALSE(pref->GetPrefObject()->value->GetBool());
  EXPECT_TRUE(*pref->GetPrefObject()->user_control_disabled);

  // When the Privacy Sandbox APIs pref is enabled, the generated pref should
  // follow the state of the real FLoC pref, and user control should be enabled.
  // TODO(crbug.com/1287951): User control disabled while OT is not active.
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxFlocEnabled,
                       std::make_unique<base::Value>(true));
  EXPECT_FALSE(pref->GetPrefObject()->value->GetBool());
  EXPECT_TRUE(*pref->GetPrefObject()->user_control_disabled);

  prefs()->SetUserPref(prefs::kPrivacySandboxFlocEnabled,
                       std::make_unique<base::Value>(false));
  EXPECT_FALSE(pref->GetPrefObject()->value->GetBool());
  EXPECT_TRUE(*pref->GetPrefObject()->user_control_disabled);

  // The generated pref should inherit the management state of the Privacy
  // Sandbox APIs pref.
  // TODO(crbug.com/1287951): No managenent state while OT not active.
  prefs()->SetManagedPref(prefs::kPrivacySandboxApisEnabled,
                          std::make_unique<base::Value>(false));
  EXPECT_EQ(settings_api::Enforcement::ENFORCEMENT_NONE,
            pref->GetPrefObject()->enforcement);
  EXPECT_EQ(settings_api::ControlledBy::CONTROLLED_BY_NONE,
            pref->GetPrefObject()->controlled_by);
}

TEST_F(GeneratedFlocPrefTest, NotifyPrefUpdates) {
  // Confirm that when the relevant real preferences change, the generated
  // pref notifies observers it has been updated.
  auto pref = std::make_unique<GeneratedFlocPref>(profile());
  prefs()->SetDefaultPrefValue(prefs::kPrivacySandboxApisEnabled,
                               base::Value(false));
  prefs()->SetDefaultPrefValue(prefs::kPrivacySandboxFlocEnabled,
                               base::Value(false));

  settings_private::TestGeneratedPrefObserver test_observer;
  pref->AddObserver(&test_observer);

  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabled,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(test_observer.GetUpdatedPrefName(), kGeneratedFlocPref);
  test_observer.Reset();

  prefs()->SetUserPref(prefs::kPrivacySandboxFlocEnabled,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(test_observer.GetUpdatedPrefName(), kGeneratedFlocPref);
  test_observer.Reset();
}
