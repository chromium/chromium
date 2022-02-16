// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace {
using privacy_sandbox::CanonicalTopic;
using testing::ElementsAre;

class TestInterestGroupManager : public content::InterestGroupManager {
 public:
  void SetInterestGroupJoiningOrigins(const std::vector<url::Origin>& origins) {
    origins_ = origins;
  }

  // content::InterestGroupManager:
  void GetAllInterestGroupJoiningOrigins(
      base::OnceCallback<void(std::vector<url::Origin>)> callback) override {
    std::move(callback).Run(origins_);
  }

 private:
  std::vector<url::Origin> origins_;
};

struct DialogTestState {
  bool consent_required;
  bool old_api_pref;
  bool new_api_pref;
  bool notice_displayed;
  bool consent_decision_made;
  bool confirmation_not_shown;
};

struct ExpectedDialogOutput {
  bool dcheck_failure;
  PrivacySandboxService::DialogType dialog_type;
  bool new_api_pref;
};

struct DialogTestCase {
  DialogTestState test_setup;
  ExpectedDialogOutput expected_output;
};

std::vector<DialogTestCase> kDialogTestCases = {
    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNotice,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kConsent,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/true,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/true,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/true,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/true,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kConsent,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kConsent,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*dialog_type=*/PrivacySandboxService::DialogType::kNone,
      /*new_api_pref=*/true}},
};

void SetupDialogTestState(
    base::test::ScopedFeatureList* feature_list,
    sync_preferences::TestingPrefServiceSyncable* pref_service,
    const DialogTestState& test_state) {
  feature_list->Reset();
  feature_list->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", test_state.consent_required ? "true" : "false"}});

  pref_service->SetUserPref(
      prefs::kPrivacySandboxApisEnabled,
      std::make_unique<base::Value>(test_state.old_api_pref));

  pref_service->SetUserPref(
      prefs::kPrivacySandboxApisEnabledV2,
      std::make_unique<base::Value>(test_state.new_api_pref));

  pref_service->SetUserPref(
      prefs::kPrivacySandboxNoticeDisplayed,
      std::make_unique<base::Value>(test_state.notice_displayed));

  pref_service->SetUserPref(
      prefs::kPrivacySandboxConsentDecisionMade,
      std::make_unique<base::Value>(test_state.consent_decision_made));

  pref_service->SetUserPref(
      prefs::kPrivacySandboxNoConfirmationSandboxDisabled,
      std::make_unique<base::Value>(test_state.confirmation_not_shown));
}

}  // namespace

class PrivacySandboxServiceTest : public testing::Test {
 public:
  PrivacySandboxServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    InitializePrefsBeforeStart();

    privacy_sandbox_service_ = std::make_unique<PrivacySandboxService>(
        PrivacySandboxSettingsFactory::GetForProfile(profile()),
        CookieSettingsFactory::GetForProfile(profile()).get(),
        profile()->GetPrefs(), policy_service(), sync_service(),
        identity_test_env()->identity_manager(), test_interest_group_manager(),
        GetProfileType());
  }

  virtual void InitializePrefsBeforeStart() {}

  virtual profile_metrics::BrowserProfileType GetProfileType() {
    return profile_metrics::BrowserProfileType::kRegular;
  }

  void ConfirmRequiredDialogType(
      PrivacySandboxService::DialogType dialog_type) {
    // The required dialog type should never change between successive calls to
    // GetRequiredDialogType.
    EXPECT_EQ(dialog_type, privacy_sandbox_service()->GetRequiredDialogType());
  }

  TestingProfile* profile() { return &profile_; }
  PrivacySandboxService* privacy_sandbox_service() {
    return privacy_sandbox_service_.get();
  }
  PrivacySandboxSettings* privacy_sandbox_settings() {
    return PrivacySandboxSettingsFactory::GetForProfile(profile());
  }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }
  HostContentSettingsMap* host_content_settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }
  syncer::TestSyncService* sync_service() { return &sync_service_; }
  policy::MockPolicyService* policy_service() { return &mock_policy_service_; }
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }
  TestInterestGroupManager* test_interest_group_manager() {
    return &test_interest_group_manager_;
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  testing::NiceMock<policy::MockPolicyService> mock_policy_service_;

  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  syncer::TestSyncService sync_service_;
  TestInterestGroupManager test_interest_group_manager_;

  std::unique_ptr<PrivacySandboxService> privacy_sandbox_service_;
};

TEST_F(PrivacySandboxServiceTest, GetFlocDescriptionForDisplay) {
  EXPECT_EQ(
      l10n_util::GetPluralStringFUTF16(IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 7),
      privacy_sandbox_service()->GetFlocDescriptionForDisplay());
}

TEST_F(PrivacySandboxServiceTest, GetFlocIdForDisplay) {
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID),
            privacy_sandbox_service()->GetFlocIdForDisplay());
}

TEST_F(PrivacySandboxServiceTest, GetFlocIdNextUpdateForDisplay) {
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE_INVALID),
            privacy_sandbox_service()->GetFlocIdNextUpdateForDisplay(
                base::Time::Now()));
}

TEST_F(PrivacySandboxServiceTest, GetFlocResetExplanationForDisplay) {
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 7),
            privacy_sandbox_service()->GetFlocResetExplanationForDisplay());
}

TEST_F(PrivacySandboxServiceTest, GetFlocStatusForDisplay) {
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_STATUS_NOT_ACTIVE),
      privacy_sandbox_service()->GetFlocStatusForDisplay());
}

TEST_F(PrivacySandboxServiceTest, IsFlocIdResettable) {
  EXPECT_FALSE(privacy_sandbox_service()->IsFlocIdResettable());
}

TEST_F(PrivacySandboxServiceTest, UserResetFlocID) {
  // Check that the PrivacySandboxSettings is informed, and the appropriate
  // actions are logged, in response to a user resetting the floc id.
  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());

  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnFlocDataAccessibleSinceUpdated(true)).Times(2);

  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.ResetFloc"));

  privacy_sandbox_service()->ResetFlocId(/*user_initiated=*/true);

  EXPECT_NE(base::Time(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.ResetFloc"));

  privacy_sandbox_service()->ResetFlocId(/*user_initiated=*/false);
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.ResetFloc"));
}

TEST_F(PrivacySandboxServiceTest, IsFlocPrefEnabled) {
  // IsFlocPrefEnabled should directly reflect the state of the FLoC pref.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  EXPECT_TRUE(privacy_sandbox_service()->IsFlocPrefEnabled());

  // The Privacy Sandbox APIs pref should not impact the return value.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  EXPECT_TRUE(privacy_sandbox_service()->IsFlocPrefEnabled());

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_FALSE(privacy_sandbox_service()->IsFlocPrefEnabled());
}

TEST_F(PrivacySandboxServiceTest, SetFlocPrefEnabled) {
  // The FLoc pref should always be updated by this function, regardless of
  // other Sandbox State.
  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocEnabled"));
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocDisabled"));

  privacy_sandbox_service()->SetFlocPrefEnabled(false);
  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxFlocEnabled));
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocEnabled"));
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocDisabled"));

  // Disabling the sandbox shouldn't prevent the pref from being updated. This
  // state is not directly allowable by the UI, but the state itself is valid
  // as far as the PrivacySandboxService service is concerned.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  privacy_sandbox_service()->SetFlocPrefEnabled(true);
  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxFlocEnabled));
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocEnabled"));
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocDisabled"));
}

TEST_F(PrivacySandboxServiceTest, OnPrivacySandboxPrefChanged) {
  // When either the main Privacy Sandbox pref, or the FLoC pref, are changed
  // the FLoC ID should be reset. This will be propagated to the settings
  // instance, which should then notify observers.
  privacy_sandbox_test_util::MockPrivacySandboxObserver
      mock_privacy_sandbox_observer;
  PrivacySandboxSettingsFactory::GetForProfile(profile())->AddObserver(
      &mock_privacy_sandbox_observer);
  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);

  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);

  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);

  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);
}

TEST_F(PrivacySandboxServiceTest, GetFledgeJoiningEtldPlusOne) {
  // Confirm that the set of FLEDGE origins which were top-frame for FLEDGE join
  // actions is correctly converted into a list of eTLD+1s.

  using TestCase =
      std::pair<std::vector<url::Origin>, std::vector<std::string>>;

  // Items which map to the same eTLD+1 should be coalesced into a single entry.
  TestCase test_case_1 = {
      {url::Origin::Create(GURL("https://www.example.com")),
       url::Origin::Create(GURL("https://example.com:8080")),
       url::Origin::Create(GURL("http://www.example.com"))},
      {"example.com"}};

  // eTLD's should return the host instead, this is relevant for sites which
  // are themselves on the PSL, e.g. github.io.
  TestCase test_case_2 = {{
                              url::Origin::Create(GURL("https://co.uk")),
                              url::Origin::Create(GURL("http://co.uk")),
                              url::Origin::Create(GURL("http://example.co.uk")),
                          },
                          {"co.uk", "example.co.uk"}};

  // IP addresses should also return the host.
  TestCase test_case_3 = {
      {
          url::Origin::Create(GURL("https://192.168.1.2")),
          url::Origin::Create(GURL("https://192.168.1.2:8080")),
          url::Origin::Create(GURL("https://192.168.1.3:8080")),
      },
      {"192.168.1.2", "192.168.1.3"}};

  std::vector<TestCase> test_cases = {test_case_1, test_case_2, test_case_3};

  for (const auto& origins_to_expected : test_cases) {
    test_interest_group_manager()->SetInterestGroupJoiningOrigins(
        {origins_to_expected.first});

    bool callback_called = false;
    auto callback = base::BindLambdaForTesting(
        [&](std::vector<std::string> items_for_display) {
          ASSERT_EQ(items_for_display.size(),
                    origins_to_expected.second.size());
          for (size_t i = 0; i < items_for_display.size(); i++)
            EXPECT_EQ(origins_to_expected.second[i], items_for_display[i]);
          callback_called = true;
        });

    privacy_sandbox_service()->GetFledgeJoiningEtldPlusOneForDisplay(callback);
    EXPECT_TRUE(callback_called);
  }
}

TEST_F(PrivacySandboxServiceTest, GetFledgeBlockedEtldPlusOne) {
  // Confirm that blocked FLEDGE top frame eTLD+1's are correctly produced
  // for display.
  const std::vector<std::string> sites = {"google.com", "example.com",
                                          "google.com.au"};
  for (const auto& site : sites)
    privacy_sandbox_settings()->SetFledgeJoiningAllowed(site, false);

  // Sites should be returned in lexographical order.
  auto returned_sites =
      privacy_sandbox_service()->GetBlockedFledgeJoiningTopFramesForDisplay();
  ASSERT_EQ(3u, returned_sites.size());
  EXPECT_EQ(returned_sites[0], sites[1]);
  EXPECT_EQ(returned_sites[1], sites[0]);
  EXPECT_EQ(returned_sites[2], sites[2]);

  // Settings a site back to allowed should appropriately remove it from the
  // display list.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("google.com", true);
  returned_sites =
      privacy_sandbox_service()->GetBlockedFledgeJoiningTopFramesForDisplay();
  ASSERT_EQ(2u, returned_sites.size());
  EXPECT_EQ(returned_sites[0], sites[1]);
  EXPECT_EQ(returned_sites[1], sites[2]);
}

TEST_F(PrivacySandboxServiceTest, DialogActionUpdatesRequiredDialog) {
  // Confirm that when the service is informed a dialog action occurred, it
  // correctly adjusts the required dialog type and Privacy Sandbox pref.

  // Consent accepted:
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::DialogType::kConsent,
            privacy_sandbox_service()->GetRequiredDialogType());
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  privacy_sandbox_service()->DialogActionOccurred(
      PrivacySandboxService::DialogAction::kConsentAccepted);

  EXPECT_EQ(PrivacySandboxService::DialogType::kNone,
            privacy_sandbox_service()->GetRequiredDialogType());
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  // Consent declined:
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::DialogType::kConsent,
            privacy_sandbox_service()->GetRequiredDialogType());
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  privacy_sandbox_service()->DialogActionOccurred(
      PrivacySandboxService::DialogAction::kConsentDeclined);

  EXPECT_EQ(PrivacySandboxService::DialogType::kNone,
            privacy_sandbox_service()->GetRequiredDialogType());
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  // Notice shown:
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::DialogType::kNotice,
            privacy_sandbox_service()->GetRequiredDialogType());
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  privacy_sandbox_service()->DialogActionOccurred(
      PrivacySandboxService::DialogAction::kNoticeShown);

  EXPECT_EQ(PrivacySandboxService::DialogType::kNone,
            privacy_sandbox_service()->GetRequiredDialogType());
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
}

class PrivacySandboxServiceTestReconciliationBlocked
    : public PrivacySandboxServiceTest {
 public:
  void InitializePrefsBeforeStart() override {
    // Set the reconciled preference to true here, so when the service is
    // created prior to each test case running, it does not attempt to reconcile
    // the preferences. Tests must call ResetReconciledPref before testing to
    // reset the preference to it's default value.
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kPrivacySandboxPreferencesReconciled,
        std::make_unique<base::Value>(true));
  }

  void ResetReconciledPref() {
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kPrivacySandboxPreferencesReconciled,
        std::make_unique<base::Value>(false));
  }
};

TEST_F(PrivacySandboxServiceTestReconciliationBlocked, ReconciliationOutcome) {
  // Check that reconciling preferences has the appropriate outcome based on
  // the current user cookie settings.
  ResetReconciledPref();

  // Blocking 3P cookies should disable.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Blocking all cookies should disable.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Blocking cookies via content setting exceptions, now matter how broad,
  // should not disable.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"[*.]com", "*", ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // If the user has already expressed control over the privacy sandbox, it
  // should not be disabled.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxManuallyControlled,
      std::make_unique<base::Value>(true));

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Allowing cookies should leave the sandbox enabled.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxManuallyControlled,
      std::make_unique<base::Value>(true));

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Reconciliation should not enable the privacy sandbox.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxManuallyControlled,
      std::make_unique<base::Value>(false));

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       ImmediateReconciliationNoSync) {
  // Check that if the user is not syncing preferences, reconciliation occurs
  // immediately.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  auto registered_types =
      sync_service()->GetUserSettings()->GetRegisteredSelectableTypes();
  registered_types.Remove(syncer::UserSelectableType::kPreferences);
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, registered_types);

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       ImmediateReconciliationSyncComplete) {
  // Check that if sync has completed a cycle that reconciliation occurs
  // immediately.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetNonEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       ImmediateReconciliationPersistentSyncError) {
  // Check that if sync has a persistent error that reconciliation occurs
  // immediately.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       ImmediateReconciliationNoDisable) {
  // Check that if the local settings would not disable the privacy sandbox
  // that reconciliation runs.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       DelayedReconciliationSyncSuccess) {
  // Check that a sync service which has not yet started delays reconciliation
  // until it has completed a sync cycle.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  sync_service()->SetNonEmptyLastCycleSnapshot();
  sync_service()->FireSyncCycleCompleted();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       DelayedReconciliationSyncFailure) {
  // Check that a sync service which has not yet started delays reconciliation
  // until a persistent error has occurred.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A transient sync startup state should not result in reconciliation.
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::START_DEFERRED);
  sync_service()->FireStateChanged();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A state update after an unrecoverable error should result in
  // reconciliation.
  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       DelayedReconciliationIdentityFailure) {
  // Check that a sync service which has not yet started delays reconciliation
  // until a persistent identity error has occurred.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // An account becoming available should not result in reconciliation.
  identity_test_env()->MakePrimaryAccountAvailable("test@test.com",
                                                   signin::ConsentLevel::kSync);

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A successful update to refresh tokens should not result in reconciliation.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A persistent authentication error for a non-primary account should not
  // result in reconciliation.
  auto non_primary_account =
      identity_test_env()->MakeAccountAvailable("unrelated@unrelated.com");
  identity_test_env()->SetRefreshTokenForAccount(
      non_primary_account.account_id);
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      non_primary_account.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A persistent authentication error for the primary account should result
  // in reconciliation.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSync),
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       DelayedReconciliationSyncIssueThenManaged) {
  // Check that if before an initial sync issue is resolved, the cookie settings
  // are disabled by policy, that reconciliation does not run until the policy
  // is removed.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // Apply a management state that is disabling cookies. This should result
  // in the policy service being observed when the sync issue is resolved.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});

  EXPECT_CALL(*policy_service(), AddObserver(policy::POLICY_DOMAIN_CHROME,
                                             privacy_sandbox_service()))
      .Times(1);

  sync_service()->SetNonEmptyLastCycleSnapshot();
  sync_service()->FireSyncCycleCompleted();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // Removing the management state and firing the policy update listener should
  // result in reconciliation running.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  // The HostContentSettingsMap & PrefService are inspected directly, and not
  // the PolicyMap provided here. The associated browser tests confirm that this
  // is a valid approach.
  privacy_sandbox_service()->OnPolicyUpdated(
      policy::PolicyNamespace(), policy::PolicyMap(), policy::PolicyMap());

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       NoReconciliationAlreadyRun) {
  // Reconciliation should not run if it is recorded as already occurring.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxPreferencesReconciled,
      std::make_unique<base::Value>(true));

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  // If run, reconciliation would have disabled the sandbox.
  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       MetricsLoggingOccursCorrectly) {
  base::HistogramTester histograms;
  const std::string histogram_name = "Settings.PrivacySandbox.Enabled";
  ResetReconciledPref();

  // The histogram should start off empty.
  histograms.ExpectTotalCount(histogram_name, 0);

  // For buckets that do not explicitly mention FLoC, it is assumed to be on,
  // or its state is irrelevant, i.e. overridden by the Privacy Sandbox pref.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 1);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledAllowAll),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 2);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledBlock3P),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 3);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledBlockAll),
      1);

  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 4);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledAllowAll),
      1);

  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 5);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledBlock3P),
      1);

  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 6);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::PrivacySandboxService::
                           SettingsPrivacySandboxEnabled::kPSDisabledBlockAll),
      1);

  // Verify that delayed reconciliation still logs properly.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  histograms.ExpectTotalCount(histogram_name, 6);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledBlockAll),
      1);

  sync_service()->SetNonEmptyLastCycleSnapshot();
  sync_service()->FireSyncCycleCompleted();

  histograms.ExpectTotalCount(histogram_name, 7);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledBlockAll),
      2);

  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 8);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledPolicyBlockAll),
      1);

  // Disable FLoC and test the buckets that reflect a disabled FLoC state.
  ResetReconciledPref();
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 9);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledFlocDisabledAllowAll),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 10);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledFlocDisabledBlock3P),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 11);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledFlocDisabledBlockAll),
      1);
}

class PrivacySandboxServiceTestNonRegularProfile
    : public PrivacySandboxServiceTestReconciliationBlocked {
  profile_metrics::BrowserProfileType GetProfileType() override {
    return profile_metrics::BrowserProfileType::kSystem;
  }
};

TEST_F(PrivacySandboxServiceTestNonRegularProfile, NoMetricsRecorded) {
  // Check that non-regular profiles do not record metrics.
  base::HistogramTester histograms;
  const std::string histogram_name = "Settings.PrivacySandbox.Enabled";
  ResetReconciledPref();

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  // The histogram should remain empty.
  histograms.ExpectTotalCount(histogram_name, 0);
}

TEST_F(PrivacySandboxServiceTestNonRegularProfile, TestFakeTopics) {
  CanonicalTopic topic1(1, CanonicalTopic::AVAILABLE_TAXONOMY);
  CanonicalTopic topic2(2, CanonicalTopic::AVAILABLE_TAXONOMY);
  CanonicalTopic topic3(3, CanonicalTopic::AVAILABLE_TAXONOMY);
  CanonicalTopic topic4(4, CanonicalTopic::AVAILABLE_TAXONOMY);

  auto* service = privacy_sandbox_service();
  EXPECT_THAT(service->GetCurrentTopTopics(), ElementsAre(topic1, topic2));
  EXPECT_THAT(service->GetBlockedTopics(), ElementsAre(topic3, topic4));

  service->SetTopicAllowed(topic1, false);
  EXPECT_THAT(service->GetCurrentTopTopics(), ElementsAre(topic2));
  EXPECT_THAT(service->GetBlockedTopics(), ElementsAre(topic1, topic3, topic4));

  service->SetTopicAllowed(topic4, true);
  EXPECT_THAT(service->GetCurrentTopTopics(), ElementsAre(topic2, topic4));
  EXPECT_THAT(service->GetBlockedTopics(), ElementsAre(topic1, topic3));
}

TEST_F(PrivacySandboxServiceTestNonRegularProfile, NoDialogRequired) {
  // Non-regular profiles should never have a dialog shown.
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::DialogType::kNone,
            privacy_sandbox_service()->GetRequiredDialogType());

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::DialogType::kNone,
            privacy_sandbox_service()->GetRequiredDialogType());
}

class PrivacySandboxServiceDeathTest : public testing::TestWithParam<int> {
 public:
  PrivacySandboxServiceDeathTest() {
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());
  }

 protected:
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return &pref_service_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

TEST_P(PrivacySandboxServiceDeathTest, GetRequiredDialogType) {
  const auto& test_case = kDialogTestCases[GetParam()];

  testing::Message scope_message;
  scope_message << "consent_required:" << test_case.test_setup.consent_required
                << " old_api_pref:" << test_case.test_setup.old_api_pref
                << " new_api_pref:" << test_case.test_setup.new_api_pref
                << " notice_displayed:" << test_case.test_setup.notice_displayed
                << " consent_decision_made:"
                << test_case.test_setup.consent_decision_made
                << " confirmation_not_shown:"
                << test_case.test_setup.confirmation_not_shown;
  SCOPED_TRACE(scope_message);

  SetupDialogTestState(feature_list(), prefs(), test_case.test_setup);
  if (test_case.expected_output.dcheck_failure) {
    EXPECT_DCHECK_DEATH(PrivacySandboxService::GetRequiredDialogTypeInternal(
        prefs(), profile_metrics::BrowserProfileType::kRegular));
    return;
  }

  // Returned dialog type should never change between successive calls.
  EXPECT_EQ(test_case.expected_output.dialog_type,
            PrivacySandboxService::GetRequiredDialogTypeInternal(
                prefs(), profile_metrics::BrowserProfileType::kRegular));
  EXPECT_EQ(test_case.expected_output.dialog_type,
            PrivacySandboxService::GetRequiredDialogTypeInternal(
                prefs(), profile_metrics::BrowserProfileType::kRegular));

  EXPECT_EQ(test_case.expected_output.new_api_pref,
            prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  // The old Privacy Sandbox pref should never change from the initial test
  // state.
  EXPECT_EQ(test_case.test_setup.old_api_pref,
            prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabled));
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxServiceDeathTestInstance,
                         PrivacySandboxServiceDeathTest,
                         testing::Range(0, 64));

using PrivacySandboxServiceTestCoverageTest = testing::Test;

TEST_F(PrivacySandboxServiceTestCoverageTest, DialogTestCoverage) {
  // Confirm that the set of dialog test cases exhaustively covers all possible
  // combinations of input.
  std::set<int> test_case_properties;
  for (const auto& test_case : kDialogTestCases) {
    int test_case_property = 0;
    test_case_property |= test_case.test_setup.consent_required ? 1 << 0 : 0;
    test_case_property |= test_case.test_setup.old_api_pref ? 1 << 1 : 0;
    test_case_property |= test_case.test_setup.new_api_pref ? 1 << 2 : 0;
    test_case_property |= test_case.test_setup.notice_displayed ? 1 << 3 : 0;
    test_case_property |=
        test_case.test_setup.consent_decision_made ? 1 << 4 : 0;
    test_case_property |=
        test_case.test_setup.confirmation_not_shown ? 1 << 5 : 0;
    test_case_properties.insert(test_case_property);
  }
  EXPECT_EQ(test_case_properties.size(), kDialogTestCases.size());
  EXPECT_EQ(64u, test_case_properties.size());
}
