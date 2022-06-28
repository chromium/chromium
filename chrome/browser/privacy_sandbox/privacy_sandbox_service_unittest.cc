// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"

#include "base/memory/raw_ptr.h"
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
#include "components/browsing_topics/test_util.h"
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
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/login/login_state/login_state.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#endif

namespace {
using browsing_topics::Topic;
using privacy_sandbox::CanonicalTopic;
using testing::ElementsAre;

const char kPrivacySandboxStartupHistogram[] =
    "Settings.PrivacySandbox.StartupState";

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

class MockPrivacySandboxSettings
    : public privacy_sandbox::PrivacySandboxSettings {
 public:
  void SetupDefaultResponse() {
    ON_CALL(*this, IsPrivacySandboxRestricted).WillByDefault([]() {
      return false;
    });
  }
  MOCK_METHOD(bool, IsPrivacySandboxRestricted, (), (override));
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
  PrivacySandboxService::PromptType prompt_type;
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
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNotice,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kConsent,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNotice,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kConsent,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kConsent,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kConsent,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/false},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/false,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/false, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/false,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/false}},

    {{/*consent_required=*/false, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/false,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/false, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},

    {{/*consent_required=*/true, /*old_api_pref=*/true,
      /*new_api_pref=*/true,
      /*notice_displayed=*/true, /*consent_decision_made=*/true,
      /*confirmation_not_shown=*/true},
     {/*dcheck_failure=*/false,
      /*prompt_type=*/PrivacySandboxService::PromptType::kNone,
      /*new_api_pref=*/true}},
};

void SetupDialogTestState(
    base::test::ScopedFeatureList* feature_list,
    sync_preferences::TestingPrefServiceSyncable* pref_service,
    const DialogTestState& test_state) {
  feature_list->Reset();
  feature_list->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", test_state.consent_required ? "true" : "false"},
       {"notice-required", !test_state.consent_required ? "true" : "false"}});

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
    auto mock_delegate = std::make_unique<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>();
    mock_delegate_ = mock_delegate.get();

    InitializeBeforeStart();

    privacy_sandbox_settings_ =
        std::make_unique<privacy_sandbox::PrivacySandboxSettings>(
            std::move(mock_delegate), host_content_settings_map(),
            cookie_settings(), prefs(), /*incognito_profile=*/false);
#if !BUILDFLAG(IS_ANDROID)
    mock_sentiment_service_ =
        std::make_unique<::testing::NiceMock<MockTrustSafetySentimentService>>(
            profile());
#endif
    privacy_sandbox_service_ = std::make_unique<PrivacySandboxService>(
        privacy_sandbox_settings(), cookie_settings(), profile()->GetPrefs(),
        policy_service(), sync_service(),
        identity_test_env()->identity_manager(), test_interest_group_manager(),
        GetProfileType(), browsing_data_remover(),
#if !BUILDFLAG(IS_ANDROID)
        mock_sentiment_service(),
#endif
        mock_browsing_topics_service());
  }

  virtual void InitializeBeforeStart() {
    mock_delegate()->SetupDefaultResponse(/*restricted=*/false,
                                          /*confirmed=*/true);
  }

  virtual profile_metrics::BrowserProfileType GetProfileType() {
    return profile_metrics::BrowserProfileType::kRegular;
  }

  void ConfirmRequiredPromptType(
      PrivacySandboxService::PromptType prompt_type) {
    // The required dialog type should never change between successive calls to
    // GetRequiredPromptType.
    EXPECT_EQ(prompt_type, privacy_sandbox_service()->GetRequiredPromptType());
  }

  TestingProfile* profile() { return &profile_; }
  PrivacySandboxService* privacy_sandbox_service() {
    return privacy_sandbox_service_.get();
  }
  privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings() {
    return privacy_sandbox_settings_.get();
  }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }
  HostContentSettingsMap* host_content_settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }
  content_settings::CookieSettings* cookie_settings() {
    return CookieSettingsFactory::GetForProfile(profile()).get();
  }
  syncer::TestSyncService* sync_service() { return &sync_service_; }
  policy::MockPolicyService* policy_service() { return &mock_policy_service_; }
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }
  TestInterestGroupManager* test_interest_group_manager() {
    return &test_interest_group_manager_;
  }
  content::BrowsingDataRemover* browsing_data_remover() {
    return profile()->GetBrowsingDataRemover();
  }
  privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate*
  mock_delegate() {
    return mock_delegate_;
  }
  browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service() {
    return &mock_browsing_topics_service_;
  }
#if !BUILDFLAG(IS_ANDROID)
  MockTrustSafetySentimentService* mock_sentiment_service() {
    return mock_sentiment_service_.get();
  }
#endif

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  testing::NiceMock<policy::MockPolicyService> mock_policy_service_;

  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  syncer::TestSyncService sync_service_;
  TestInterestGroupManager test_interest_group_manager_;
  raw_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
      mock_delegate_;
  browsing_topics::MockBrowsingTopicsService mock_browsing_topics_service_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<MockTrustSafetySentimentService> mock_sentiment_service_;
#endif
  std::unique_ptr<privacy_sandbox::PrivacySandboxSettings>
      privacy_sandbox_settings_;

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

TEST_F(PrivacySandboxServiceTest, PromptActionUpdatesRequiredDialog) {
  // Confirm that when the service is informed a dialog action occurred, it
  // correctly adjusts the required prompt type and Privacy Sandbox pref.

  // Consent accepted:
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::PromptType::kConsent,
            privacy_sandbox_service()->GetRequiredPromptType());
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentAccepted);

  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  // Consent declined:
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::PromptType::kConsent,
            privacy_sandbox_service()->GetRequiredPromptType());
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentDeclined);

  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  // Notice shown:
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::PromptType::kNotice,
            privacy_sandbox_service()->GetRequiredPromptType());
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeShown);

  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxDialogNoticeWaiting) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3, {{"notice-required", "true"}});
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationSandboxDisabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kPrivacySandboxNoticeDisplayed,
                       std::make_unique<base::Value>(false));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kDialogWaiting, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxDialogConsentWaiting) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", "true" /* consent required */}});
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationSandboxDisabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kPrivacySandboxConsentDecisionMade,
                       std::make_unique<base::Value>(false));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kDialogWaiting, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxV1OffDisabled) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", "false" /* consent required */}});
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationSandboxDisabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(false));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kDialogOffV1OffDisabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxV1OffEnabled) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", "false" /* consent required */}});
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationSandboxDisabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(true));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kDialogOffV1OffEnabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxRestricted) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", "false" /* consent required */}});
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationSandboxRestricted,
                       std::make_unique<base::Value>(true));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kDialogOffRestricted, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxManagedEnabled) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", "false" /* consent required */}});
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationSandboxManaged,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(true));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kDialogOffManagedEnabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxManagedDisabled) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", "false" /* consent required */}});
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationSandboxManaged,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(false));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kDialogOffManagedDisabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandbox3PCOffEnabled) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", "false" /* consent required */}});
  prefs()->SetUserPref(
      prefs::kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked,
      std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(true));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kDialogOff3PCOffEnabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandbox3PCOffDisabled) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", "false" /* consent required */}});
  prefs()->SetUserPref(
      prefs::kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked,
      std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(false));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kDialogOff3PCOffDisabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxConsentEnabled) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", "true" /* consent required */}});
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationSandboxDisabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kPrivacySandboxConsentDecisionMade,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(true));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kConsentShownEnabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxConsentDisabled) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"consent-required", "true" /* consent required */}});
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationSandboxDisabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kPrivacySandboxConsentDecisionMade,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(false));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kConsentShownDisabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxNoticeEnabled) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"notice-required", "true" /* consent required */}});
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationSandboxDisabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kPrivacySandboxNoticeDisplayed,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(true));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kNoticeShownEnabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxNoticeDisabled) {
  base::HistogramTester histogram_tester;
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"notice-required", "true" /* consent required */}});
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationSandboxDisabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kPrivacySandboxNoticeDisplayed,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(false));

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kNoticeShownDisabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxManuallyControlledEnabled) {
  base::HistogramTester histogram_tester;
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationManuallyControlled,
                       std::make_unique<base::Value>(true));
  privacy_sandbox_service()->LogPrivacySandboxState();
  histogram_tester.ExpectUniqueSample(kPrivacySandboxStartupHistogram,
                                      PrivacySandboxService::PSStartupStates::
                                          kDialogOffManuallyControlledEnabled,
                                      1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxManuallyControlledDisabled) {
  base::HistogramTester histogram_tester;
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationManuallyControlled,
                       std::make_unique<base::Value>(true));
  privacy_sandbox_service()->LogPrivacySandboxState();
  histogram_tester.ExpectUniqueSample(kPrivacySandboxStartupHistogram,
                                      PrivacySandboxService::PSStartupStates::
                                          kDialogOffManuallyControlledDisabled,
                                      1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxNoDialogDisabled) {
  base::HistogramTester histogram_tester;
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(false));
  privacy_sandbox_service()->LogPrivacySandboxState();
  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kNoDialogRequiredDisabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxNoDialogEnabled) {
  base::HistogramTester histogram_tester;
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(true));
  privacy_sandbox_service()->LogPrivacySandboxState();
  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kNoDialogRequiredEnabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PromptActionsUMAActions) {
  base::UserActionTester user_action_tester;

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeShown);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.Shown"));

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeOpenSettings);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.OpenedSettings"));

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeAcknowledge);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.Acknowledged"));

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeDismiss);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.Dismissed"));

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.ClosedNoInteraction"));

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeLearnMore);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.LearnMore"));

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentShown);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.Shown"));

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentAccepted);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.Accepted"));

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentDeclined);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.Declined"));

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentMoreInfoOpened);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.LearnMoreExpanded"));

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentClosedNoDecision);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.ClosedNoInteraction"));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PrivacySandboxServiceTest, PromptActionsSentimentService) {
  {
    EXPECT_CALL(*mock_sentiment_service(),
                InteractedWithPrivacySandbox3(testing::_))
        .Times(0);
    SetupDialogTestState(feature_list(), prefs(),
                         {/*consent_required=*/false,
                          /*old_api_pref=*/true,
                          /*new_api_pref=*/false,
                          /*notice_displayed=*/false,
                          /*consent_decision_made=*/false,
                          /*confirmation_not_shown=*/false});
    privacy_sandbox_service()->PromptActionOccurred(
        PrivacySandboxService::PromptAction::kNoticeShown);
  }
  {
    EXPECT_CALL(
        *mock_sentiment_service(),
        InteractedWithPrivacySandbox3(TrustSafetySentimentService::FeatureArea::
                                          kPrivacySandbox3NoticeSettings))
        .Times(1);
    SetupDialogTestState(feature_list(), prefs(),
                         {/*consent_required=*/false,
                          /*old_api_pref=*/true,
                          /*new_api_pref=*/false,
                          /*notice_displayed=*/false,
                          /*consent_decision_made=*/false,
                          /*confirmation_not_shown=*/false});
    privacy_sandbox_service()->PromptActionOccurred(
        PrivacySandboxService::PromptAction::kNoticeOpenSettings);
  }
  {
    EXPECT_CALL(
        *mock_sentiment_service(),
        InteractedWithPrivacySandbox3(
            TrustSafetySentimentService::FeatureArea::kPrivacySandbox3NoticeOk))
        .Times(1);
    SetupDialogTestState(feature_list(), prefs(),
                         {/*consent_required=*/false,
                          /*old_api_pref=*/true,
                          /*new_api_pref=*/false,
                          /*notice_displayed=*/false,
                          /*consent_decision_made=*/false,
                          /*confirmation_not_shown=*/false});
    privacy_sandbox_service()->PromptActionOccurred(
        PrivacySandboxService::PromptAction::kNoticeAcknowledge);
  }
  {
    EXPECT_CALL(
        *mock_sentiment_service(),
        InteractedWithPrivacySandbox3(TrustSafetySentimentService::FeatureArea::
                                          kPrivacySandbox3NoticeDismiss))
        .Times(1);
    SetupDialogTestState(feature_list(), prefs(),
                         {/*consent_required=*/false,
                          /*old_api_pref=*/true,
                          /*new_api_pref=*/false,
                          /*notice_displayed=*/false,
                          /*consent_decision_made=*/false,
                          /*confirmation_not_shown=*/false});
    privacy_sandbox_service()->PromptActionOccurred(
        PrivacySandboxService::PromptAction::kNoticeDismiss);
  }
  {
    EXPECT_CALL(*mock_sentiment_service(),
                InteractedWithPrivacySandbox3(testing::_))
        .Times(0);
    SetupDialogTestState(feature_list(), prefs(),
                         {/*consent_required=*/false,
                          /*old_api_pref=*/true,
                          /*new_api_pref=*/false,
                          /*notice_displayed=*/false,
                          /*consent_decision_made=*/false,
                          /*confirmation_not_shown=*/false});
    privacy_sandbox_service()->PromptActionOccurred(
        PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction);
  }
  {
    EXPECT_CALL(
        *mock_sentiment_service(),
        InteractedWithPrivacySandbox3(TrustSafetySentimentService::FeatureArea::
                                          kPrivacySandbox3NoticeLearnMore))
        .Times(1);
    SetupDialogTestState(feature_list(), prefs(),
                         {/*consent_required=*/false,
                          /*old_api_pref=*/true,
                          /*new_api_pref=*/false,
                          /*notice_displayed=*/false,
                          /*consent_decision_made=*/false,
                          /*confirmation_not_shown=*/false});
    privacy_sandbox_service()->PromptActionOccurred(
        PrivacySandboxService::PromptAction::kNoticeLearnMore);
  }
  {
    EXPECT_CALL(*mock_sentiment_service(),
                InteractedWithPrivacySandbox3(testing::_))
        .Times(0);
    SetupDialogTestState(feature_list(), prefs(),
                         {/*consent_required=*/true,
                          /*old_api_pref=*/true,
                          /*new_api_pref=*/false,
                          /*notice_displayed=*/false,
                          /*consent_decision_made=*/false,
                          /*confirmation_not_shown=*/false});
    privacy_sandbox_service()->PromptActionOccurred(
        PrivacySandboxService::PromptAction::kConsentShown);
  }
  {
    EXPECT_CALL(
        *mock_sentiment_service(),
        InteractedWithPrivacySandbox3(TrustSafetySentimentService::FeatureArea::
                                          kPrivacySandbox3ConsentAccept))
        .Times(1);
    SetupDialogTestState(feature_list(), prefs(),
                         {/*consent_required=*/true,
                          /*old_api_pref=*/true,
                          /*new_api_pref=*/false,
                          /*notice_displayed=*/false,
                          /*consent_decision_made=*/false,
                          /*confirmation_not_shown=*/false});
    privacy_sandbox_service()->PromptActionOccurred(
        PrivacySandboxService::PromptAction::kConsentAccepted);
  }
  {
    EXPECT_CALL(
        *mock_sentiment_service(),
        InteractedWithPrivacySandbox3(TrustSafetySentimentService::FeatureArea::
                                          kPrivacySandbox3ConsentDecline))
        .Times(1);
    SetupDialogTestState(feature_list(), prefs(),
                         {/*consent_required=*/true,
                          /*old_api_pref=*/true,
                          /*new_api_pref=*/false,
                          /*notice_displayed=*/false,
                          /*consent_decision_made=*/false,
                          /*confirmation_not_shown=*/false});
    privacy_sandbox_service()->PromptActionOccurred(
        PrivacySandboxService::PromptAction::kConsentDeclined);
  }
  {
    EXPECT_CALL(*mock_sentiment_service(),
                InteractedWithPrivacySandbox3(testing::_))
        .Times(0);
    SetupDialogTestState(feature_list(), prefs(),
                         {/*consent_required=*/true,
                          /*old_api_pref=*/true,
                          /*new_api_pref=*/false,
                          /*notice_displayed=*/false,
                          /*consent_decision_made=*/false,
                          /*confirmation_not_shown=*/false});
    privacy_sandbox_service()->PromptActionOccurred(
        PrivacySandboxService::PromptAction::kConsentMoreInfoOpened);
  }
  {
    EXPECT_CALL(*mock_sentiment_service(),
                InteractedWithPrivacySandbox3(testing::_))
        .Times(0);
    SetupDialogTestState(feature_list(), prefs(),
                         {/*consent_required=*/true,
                          /*old_api_pref=*/true,
                          /*new_api_pref=*/false,
                          /*notice_displayed=*/false,
                          /*consent_decision_made=*/false,
                          /*confirmation_not_shown=*/false});
    privacy_sandbox_service()->PromptActionOccurred(
        PrivacySandboxService::PromptAction::kConsentClosedNoDecision);
  }
}
#endif

TEST_F(PrivacySandboxServiceTest, Block3PCookieNoDialog) {
  // Confirm that when 3P cookies are blocked, that no prompt is shown.
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());

  // This should persist even if 3P cookies become allowed.
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());
}

TEST_F(PrivacySandboxServiceTest, BlockAllCookiesNoDialog) {
  // Confirm that when all cookies are blocked, that no prompt is shown.
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());

  // This should persist even if cookies become allowed.
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());
}

TEST_F(PrivacySandboxServiceTest, FledgeBlockDeletesData) {
  // Allowing FLEDGE joining should not start a removal task.
  privacy_sandbox_service()->SetFledgeJoiningAllowed("example.com", true);
  EXPECT_EQ(0xffffffffffffffffull,  // -1, indicates no last removal task.
            browsing_data_remover()->GetLastUsedRemovalMaskForTesting());

  // When FLEDGE joining is blocked, a removal task should be started.
  privacy_sandbox_service()->SetFledgeJoiningAllowed("example.com", false);
  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS,
            browsing_data_remover()->GetLastUsedRemovalMaskForTesting());
  EXPECT_EQ(base::Time::Min(),
            browsing_data_remover()->GetLastUsedBeginTimeForTesting());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            browsing_data_remover()->GetLastUsedOriginTypeMaskForTesting());
}

TEST_F(PrivacySandboxServiceTest, DisablingV2SandboxClearsData) {
  // Confirm that when the V2 sandbox preference is disabled, a browsing data
  // remover task is started and Topics Data is deleted. V1 should remain
  // unaffected.
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(0);
  prefs()->SetBoolean(prefs::kPrivacySandboxApisEnabled, false);
  constexpr uint64_t kNoRemovalTask = -1ull;
  EXPECT_EQ(kNoRemovalTask,
            browsing_data_remover()->GetLastUsedRemovalMaskForTesting());

  // Enabling should not cause a removal task.
  prefs()->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, true);
  EXPECT_EQ(kNoRemovalTask,
            browsing_data_remover()->GetLastUsedRemovalMaskForTesting());

  // Disabling should start a task clearing all kAPI information.
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(1);
  prefs()->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, false);
  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS |
                content::BrowsingDataRemover::DATA_TYPE_AGGREGATION_SERVICE |
                content::BrowsingDataRemover::DATA_TYPE_ATTRIBUTION_REPORTING |
                content::BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS,
            browsing_data_remover()->GetLastUsedRemovalMaskForTesting());
  EXPECT_EQ(base::Time::Min(),
            browsing_data_remover()->GetLastUsedBeginTimeForTesting());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            browsing_data_remover()->GetLastUsedOriginTypeMaskForTesting());
}

TEST_F(PrivacySandboxServiceTest, GetTopTopics) {
  // Check that the service correctly de-dupes and orders top topics. Topics
  // should be alphabetically ordered.
  const privacy_sandbox::CanonicalTopic kFirstTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(24),  // "Blues"
          privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY);
  const privacy_sandbox::CanonicalTopic kSecondTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(23),  // "Music & audio"
          privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY);

  const std::vector<privacy_sandbox::CanonicalTopic> kTopTopics = {
      kSecondTopic, kSecondTopic, kFirstTopic};

  EXPECT_CALL(*mock_browsing_topics_service(), GetTopTopicsForDisplay())
      .WillOnce(testing::Return(kTopTopics));

  auto topics = privacy_sandbox_service()->GetCurrentTopTopics();

  ASSERT_EQ(2u, topics.size());
  EXPECT_EQ(kFirstTopic, topics[0]);
  EXPECT_EQ(kSecondTopic, topics[1]);
}

TEST_F(PrivacySandboxServiceTest, GetBlockedTopics) {
  // Check that blocked topics are correctly alphabetically sorted and returned.
  const privacy_sandbox::CanonicalTopic kFirstTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(24),  // "Blues"
          privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY);
  const privacy_sandbox::CanonicalTopic kSecondTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(23),  // "Music & audio"
          privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY);

  // The PrivacySandboxService assumes that the PrivacySandboxSettings service
  // dedupes blocked topics. Check that assumption here.
  privacy_sandbox_settings()->SetTopicAllowed(kSecondTopic, false);
  privacy_sandbox_settings()->SetTopicAllowed(kSecondTopic, false);
  privacy_sandbox_settings()->SetTopicAllowed(kFirstTopic, false);
  privacy_sandbox_settings()->SetTopicAllowed(kFirstTopic, false);

  auto blocked_topics = privacy_sandbox_service()->GetBlockedTopics();

  ASSERT_EQ(2u, blocked_topics.size());
  EXPECT_EQ(kFirstTopic, blocked_topics[0]);
  EXPECT_EQ(kSecondTopic, blocked_topics[1]);
}

TEST_F(PrivacySandboxServiceTest, SetTopicAllowed) {
  const privacy_sandbox::CanonicalTopic kTestTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(10),
          privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY);
  EXPECT_CALL(*mock_browsing_topics_service(), ClearTopic(kTestTopic)).Times(1);
  privacy_sandbox_service()->SetTopicAllowed(kTestTopic, false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(kTestTopic));

  testing::Mock::VerifyAndClearExpectations(mock_browsing_topics_service());
  EXPECT_CALL(*mock_browsing_topics_service(), ClearTopic(kTestTopic)).Times(0);
  privacy_sandbox_service()->SetTopicAllowed(kTestTopic, true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(kTestTopic));
}

TEST_F(PrivacySandboxServiceTest, InitializeV2Pref) {
  // Check that when the feature + parameters dictate, the V2 preference is
  // turned on.
  feature_list()->InitAndDisableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  privacy_sandbox_service()->InitializePrivacySandboxV2Pref();
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
  prefs()->RemoveUserPref(prefs::kPrivacySandboxApisEnabledV2Init);

  feature_list()->Reset();
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  privacy_sandbox_service()->InitializePrivacySandboxV2Pref();
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
  prefs()->RemoveUserPref(prefs::kPrivacySandboxApisEnabledV2Init);

  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{"setting-default-on", "true"}});
  privacy_sandbox_service()->InitializePrivacySandboxV2Pref();
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
  prefs()->RemoveUserPref(prefs::kPrivacySandboxApisEnabledV2);
  prefs()->RemoveUserPref(prefs::kPrivacySandboxApisEnabledV2Init);

  // Blocking 3PC should prevent the pref from being enabled.
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  privacy_sandbox_service()->InitializePrivacySandboxV2Pref();
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
  prefs()->RemoveUserPref(prefs::kPrivacySandboxApisEnabledV2Init);
  prefs()->RemoveUserPref(prefs::kCookieControlsMode);

  // Blocking all cookies should prevent the pref from being enabled.
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  privacy_sandbox_service()->InitializePrivacySandboxV2Pref();
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
  prefs()->RemoveUserPref(prefs::kPrivacySandboxApisEnabledV2Init);
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);

  // Having a disabled Privacy Sandbox V1 control should prevent the pref from
  // being enabled.
  prefs()->SetBoolean(prefs::kPrivacySandboxApisEnabled, false);
  privacy_sandbox_service()->InitializePrivacySandboxV2Pref();
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
  prefs()->RemoveUserPref(prefs::kPrivacySandboxApisEnabledV2Init);
  prefs()->RemoveUserPref(prefs::kPrivacySandboxApisEnabled);

  // Otherwise the pref should be enabled, but only once.
  privacy_sandbox_service()->InitializePrivacySandboxV2Pref();
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
  prefs()->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, false);
  privacy_sandbox_service()->InitializePrivacySandboxV2Pref();
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PrivacySandboxServiceTest, DeviceLocalAccountUser) {
  // No prompt should be shown if the user is associated with a device local
  // account on CrOS.
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  // No prompt should be shown for a public session account.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::LoginState::Initialize();
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      ash::LoginState::LoggedInUserType::LOGGED_IN_USER_PUBLIC_ACCOUNT);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::BrowserInitParamsPtr init_params =
      crosapi::mojom::BrowserInitParams::New();
  init_params->session_type = crosapi::mojom::SessionType::kPublicSession;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());

  // No prompt should be shown for a web kiosk account.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      ash::LoginState::LoggedInUserType::LOGGED_IN_USER_KIOSK);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->session_type = crosapi::mojom::SessionType::kWebKioskSession;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());

  // A prompt should be shown for a regular user.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      ash::LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->session_type = crosapi::mojom::SessionType::kRegularSession;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
  EXPECT_EQ(PrivacySandboxService::PromptType::kConsent,
            privacy_sandbox_service()->GetRequiredPromptType());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class PrivacySandboxPrefInitTest : public PrivacySandboxServiceTest {
  void InitializeBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings3,
        {{"setting-default-on", "true"}});
  }
};

TEST_F(PrivacySandboxPrefInitTest, InitalizeV2PrefOnStartup) {
  // Confirm that the V2 pref has been initialized as part of the service
  // startup. Conditions for initialization were set in the test creation.
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
}

class PrivacySandboxRestrictedTest : public PrivacySandboxServiceTest {
  void InitializeBeforeStart() override {
    prefs()->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, true);
    mock_delegate()->SetupDefaultResponse(/*restricted=*/true,
                                          /*confirmed=*/true);

    // A restriction should override a default on preference.
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings3,
        {{"setting-default-on", "true"}});
  }
};

TEST_F(PrivacySandboxRestrictedTest, DisablePreferenceOnStartup) {
  // Confirm that because it is restricted, the Privacy Sandbox, which will have
  // been enabled in the test setup, has been disabled by the service during
  // creation.
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
}

TEST_F(PrivacySandboxServiceTest, TestNoFakeTopics) {
  auto* service = privacy_sandbox_service();
  EXPECT_THAT(service->GetCurrentTopTopics(), testing::IsEmpty());
  EXPECT_THAT(service->GetBlockedTopics(), testing::IsEmpty());
}

TEST_F(PrivacySandboxServiceTest, TestFakeTopics) {
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings3,
      {{privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting.name,
        "true"}});
  CanonicalTopic topic1(Topic(1), CanonicalTopic::AVAILABLE_TAXONOMY);
  CanonicalTopic topic2(Topic(2), CanonicalTopic::AVAILABLE_TAXONOMY);
  CanonicalTopic topic3(Topic(3), CanonicalTopic::AVAILABLE_TAXONOMY);
  CanonicalTopic topic4(Topic(4), CanonicalTopic::AVAILABLE_TAXONOMY);

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

class PrivacySandboxServiceTestReconciliationBlocked
    : public PrivacySandboxServiceTest {
 public:
  void InitializeBeforeStart() override {
    // Set the reconciled preference to true here, so when the service is
    // created prior to each test case running, it does not attempt to reconcile
    // the preferences. Tests must call ResetReconciledPref before testing to
    // reset the preference to it's default value.
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kPrivacySandboxPreferencesReconciled,
        std::make_unique<base::Value>(true));
    mock_delegate()->SetupDefaultResponse(/*restricted=*/false,
                                          /*confirmed=*/true);
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

TEST_F(PrivacySandboxServiceTestNonRegularProfile, NoDialogRequired) {
  // Non-regular profiles should never have a prompt shown.
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());

  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());
}

class PrivacySandboxServiceDialogTestBase {
 public:
  PrivacySandboxServiceDialogTestBase() {
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (!user_manager::UserManager::IsInitialized())
      user_manager_.Initialize();
#endif
  }

 protected:
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return &pref_service_;
  }
  MockPrivacySandboxSettings* privacy_sandbox_settings() {
    return &privacy_sandbox_settings_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FakeChromeUserManager user_manager_;
#endif
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  MockPrivacySandboxSettings privacy_sandbox_settings_;
};

class PrivacySandboxServiceDialogTest
    : public PrivacySandboxServiceDialogTestBase,
      public testing::Test {};

TEST_F(PrivacySandboxServiceDialogTest, RestrictedDialog) {
  // Confirm that when the Privacy Sandbox is restricted, that no dialog is
  // shown.
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});

  EXPECT_CALL(*privacy_sandbox_settings(), IsPrivacySandboxRestricted())
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_EQ(
      PrivacySandboxService::PromptType::kNone,
      PrivacySandboxService::GetRequiredPromptTypeInternal(
          prefs(), profile_metrics::BrowserProfileType::kRegular,
          privacy_sandbox_settings(), /*third_party_cookies_blocked=*/false));

  // After being restricted, even if the restriction is removed, no prompt
  // should be shown. No call should even need to be made to see if the
  // sandbox is still restricted.
  EXPECT_CALL(*privacy_sandbox_settings(), IsPrivacySandboxRestricted())
      .Times(0);
  EXPECT_EQ(
      PrivacySandboxService::PromptType::kNone,
      PrivacySandboxService::GetRequiredPromptTypeInternal(
          prefs(), profile_metrics::BrowserProfileType::kRegular,
          privacy_sandbox_settings(), /*third_party_cookies_blocked=*/false));
}

TEST_F(PrivacySandboxServiceDialogTest, ManagedNoDialog) {
  // Confirm that when the Privacy Sandbox is managed, that no prompt is
  // shown.
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});

  prefs()->SetManagedPref(prefs::kPrivacySandboxApisEnabledV2,
                          base::Value(true));
  EXPECT_EQ(
      PrivacySandboxService::PromptType::kNone,
      PrivacySandboxService::GetRequiredPromptTypeInternal(
          prefs(), profile_metrics::BrowserProfileType::kRegular,
          privacy_sandbox_settings(), /*third_party_cookies_blocked=*/false));

  // This should persist even if the preference becomes unmanaged.
  prefs()->RemoveManagedPref(prefs::kPrivacySandboxApisEnabledV2);
  EXPECT_EQ(
      PrivacySandboxService::PromptType::kNone,
      PrivacySandboxService::GetRequiredPromptTypeInternal(
          prefs(), profile_metrics::BrowserProfileType::kRegular,
          privacy_sandbox_settings(), /*third_party_cookies_blocked=*/false));
}

TEST_F(PrivacySandboxServiceDialogTest, ManuallyControlledNoDialog) {
  // Confirm that if the Privacy Sandbox V2 is manually controlled by the user,
  // that no prompt is shown.
  SetupDialogTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  prefs()->SetUserPref(prefs::kPrivacySandboxManuallyControlledV2,
                       base::Value(true));
  EXPECT_EQ(
      PrivacySandboxService::PromptType::kNone,
      PrivacySandboxService::GetRequiredPromptTypeInternal(
          prefs(), profile_metrics::BrowserProfileType::kRegular,
          privacy_sandbox_settings(), /*third_party_cookies_blocked=*/false));
}

TEST_F(PrivacySandboxServiceDialogTest, NoParamNoDialog) {
  // Confirm that if neither the consent or notice parameter is set, no prompt
  // is required.
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  EXPECT_EQ(
      PrivacySandboxService::PromptType::kNone,
      PrivacySandboxService::GetRequiredPromptTypeInternal(
          prefs(), profile_metrics::BrowserProfileType::kRegular,
          privacy_sandbox_settings(), /*third_party_cookies_blocked=*/false));
}

class PrivacySandboxServiceDeathTest
    : public PrivacySandboxServiceDialogTestBase,
      public testing::TestWithParam<int> {};

TEST_P(PrivacySandboxServiceDeathTest, GetRequiredPromptType) {
  const auto& test_case = kDialogTestCases[GetParam()];
  privacy_sandbox_settings()->SetupDefaultResponse();

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
    EXPECT_DCHECK_DEATH(
        PrivacySandboxService::GetRequiredPromptTypeInternal(
            prefs(), profile_metrics::BrowserProfileType::kRegular,
            privacy_sandbox_settings(), /*third_party_cookies_blocked=*/false);

    );
    return;
  }

  // Returned prompt type should never change between successive calls.
  EXPECT_EQ(
      test_case.expected_output.prompt_type,
      PrivacySandboxService::GetRequiredPromptTypeInternal(
          prefs(), profile_metrics::BrowserProfileType::kRegular,
          privacy_sandbox_settings(), /*third_party_cookies_blocked=*/false));
  EXPECT_EQ(
      test_case.expected_output.prompt_type,
      PrivacySandboxService::GetRequiredPromptTypeInternal(
          prefs(), profile_metrics::BrowserProfileType::kRegular,
          privacy_sandbox_settings(), /*third_party_cookies_blocked=*/false));

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
  // Confirm that the set of prompt test cases exhaustively covers all possible
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
