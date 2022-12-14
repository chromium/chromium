// Copyright 2021 The Chromium Authors
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
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
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
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/login/login_state/scoped_test_public_session_login_state.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#endif

namespace {
using browsing_topics::Topic;
using privacy_sandbox::CanonicalTopic;
using testing::ElementsAre;

const char kFirstPartySetsStateHistogram[] = "Settings.FirstPartySets.State";
const char kPrivacySandboxStartupHistogram[] =
    "Settings.PrivacySandbox.StartupState";

const base::Version kFirstPartySetsVersion("1.2.3");

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
  void GetAllInterestGroupDataKeys(
      base::OnceCallback<void(std::vector<InterestGroupDataKey>)> callback)
      override {
    std::move(callback).Run({});
  }
  void RemoveInterestGroupsByDataKey(InterestGroupDataKey data_key,
                                     base::OnceClosure callback) override {
    std::move(callback).Run();
  }

 private:
  std::vector<url::Origin> origins_;
};

class MockPrivacySandboxSettings
    : public privacy_sandbox::PrivacySandboxSettings {
 public:
  void SetUpDefaultResponse() {
    ON_CALL(*this, IsPrivacySandboxRestricted).WillByDefault([]() {
      return false;
    });
  }
  MOCK_METHOD(bool, IsPrivacySandboxRestricted, (), (const, override));
};

struct PromptTestState {
  bool consent_required;
  bool old_api_pref;
  bool new_api_pref;
  bool notice_displayed;
  bool consent_decision_made;
  bool confirmation_not_shown;
};

struct ExpectedPromptOutput {
  bool dcheck_failure;
  PrivacySandboxService::PromptType prompt_type;
  bool new_api_pref;
};

struct PromptTestCase {
  PromptTestState test_setup;
  ExpectedPromptOutput expected_output;
};

std::vector<PromptTestCase> kPromptTestCases = {
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

void SetupPromptTestState(
    base::test::ScopedFeatureList* feature_list,
    sync_preferences::TestingPrefServiceSyncable* pref_service,
    const PromptTestState& test_state) {
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

// Remove any user preference settings for First Party Set related preferences,
// returning them to their default value.
void ClearFpsUserPrefs(
    sync_preferences::TestingPrefServiceSyncable* pref_service) {
  pref_service->RemoveUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled);
  pref_service->RemoveUserPref(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized);
}

}  // namespace

class PrivacySandboxServiceTest : public testing::Test {
 public:
  PrivacySandboxServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    CreateService();

    base::RunLoop run_loop;
    first_party_sets_policy_service_.WaitForFirstInitCompleteForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
    first_party_sets_policy_service_.ResetForTesting();
  }

  virtual std::unique_ptr<
      privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  GetMockDelegate() {
    auto mock_delegate = std::make_unique<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>();
    mock_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/false);
    return mock_delegate;
  }

  void CreateService() {
    privacy_sandbox_settings_ =
        std::make_unique<privacy_sandbox::PrivacySandboxSettings>(
            GetMockDelegate(), host_content_settings_map(), cookie_settings(),
            prefs());
#if !BUILDFLAG(IS_ANDROID)
    mock_sentiment_service_ =
        std::make_unique<::testing::NiceMock<MockTrustSafetySentimentService>>(
            profile());
#endif
    privacy_sandbox_service_ = std::make_unique<PrivacySandboxService>(
        privacy_sandbox_settings(), cookie_settings(), profile()->GetPrefs(),
        test_interest_group_manager(), GetProfileType(),
        browsing_data_remover(),
#if !BUILDFLAG(IS_ANDROID)
        mock_sentiment_service(),
#endif
        mock_browsing_topics_service(), first_party_sets_policy_service());
  }

  virtual profile_metrics::BrowserProfileType GetProfileType() {
    return profile_metrics::BrowserProfileType::kRegular;
  }

  void ConfirmRequiredPromptType(
      PrivacySandboxService::PromptType prompt_type) {
    // The required prompt type should never change between successive calls to
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
  TestInterestGroupManager* test_interest_group_manager() {
    return &test_interest_group_manager_;
  }
  content::BrowsingDataRemover* browsing_data_remover() {
    return profile()->GetBrowsingDataRemover();
  }
  browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service() {
    return &mock_browsing_topics_service_;
  }
  first_party_sets::ScopedMockFirstPartySetsHandler&
  mock_first_party_sets_handler() {
    return mock_first_party_sets_handler_;
  }
  first_party_sets::FirstPartySetsPolicyService*
  first_party_sets_policy_service() {
    return &first_party_sets_policy_service_;
  }
#if !BUILDFLAG(IS_ANDROID)
  MockTrustSafetySentimentService* mock_sentiment_service() {
    return mock_sentiment_service_.get();
  }
#endif

 private:
  content::BrowserTaskEnvironment browser_task_environment_;

  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  TestInterestGroupManager test_interest_group_manager_;
  browsing_topics::MockBrowsingTopicsService mock_browsing_topics_service_;
  first_party_sets::ScopedMockFirstPartySetsHandler
      mock_first_party_sets_handler_;
  first_party_sets::FirstPartySetsPolicyService
      first_party_sets_policy_service_ =
          first_party_sets::FirstPartySetsPolicyService(
              profile_.GetOriginalProfile());
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<MockTrustSafetySentimentService> mock_sentiment_service_;
#endif
  std::unique_ptr<privacy_sandbox::PrivacySandboxSettings>
      privacy_sandbox_settings_;

  std::unique_ptr<PrivacySandboxService> privacy_sandbox_service_;
};

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

  // Results should be alphabetically ordered.
  TestCase test_case_4 = {{
                              url::Origin::Create(GURL("https://d.com")),
                              url::Origin::Create(GURL("https://b.com")),
                              url::Origin::Create(GURL("https://a.com")),
                              url::Origin::Create(GURL("https://c.com")),
                          },
                          {"a.com", "b.com", "c.com", "d.com"}};

  std::vector<TestCase> test_cases = {test_case_1, test_case_2, test_case_3,
                                      test_case_4};

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

TEST_F(PrivacySandboxServiceTest, PromptActionUpdatesRequiredPrompt) {
  // Confirm that when the service is informed a prompt action occurred, it
  // correctly adjusts the required prompt type and Privacy Sandbox pref.

  // Consent accepted:
  SetupPromptTestState(feature_list(), prefs(),
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
  SetupPromptTestState(feature_list(), prefs(),
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
  SetupPromptTestState(feature_list(), prefs(),
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

TEST_F(PrivacySandboxServiceTest, PromptActionsUMAActions) {
  base::UserActionTester user_action_tester;

  SetupPromptTestState(feature_list(), prefs(),
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

  SetupPromptTestState(feature_list(), prefs(),
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

  SetupPromptTestState(feature_list(), prefs(),
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

  SetupPromptTestState(feature_list(), prefs(),
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

  SetupPromptTestState(feature_list(), prefs(),
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

  SetupPromptTestState(feature_list(), prefs(),
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

  SetupPromptTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeMoreInfoOpened);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.LearnMoreExpanded"));

  SetupPromptTestState(feature_list(), prefs(),
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

  SetupPromptTestState(feature_list(), prefs(),
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

  SetupPromptTestState(feature_list(), prefs(),
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

  SetupPromptTestState(feature_list(), prefs(),
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

  SetupPromptTestState(feature_list(), prefs(),
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
    SetupPromptTestState(feature_list(), prefs(),
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
    SetupPromptTestState(feature_list(), prefs(),
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
    SetupPromptTestState(feature_list(), prefs(),
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
    SetupPromptTestState(feature_list(), prefs(),
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
    SetupPromptTestState(feature_list(), prefs(),
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
    SetupPromptTestState(feature_list(), prefs(),
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
    SetupPromptTestState(feature_list(), prefs(),
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
    SetupPromptTestState(feature_list(), prefs(),
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
    SetupPromptTestState(feature_list(), prefs(),
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
    SetupPromptTestState(feature_list(), prefs(),
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
    SetupPromptTestState(feature_list(), prefs(),
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

TEST_F(PrivacySandboxServiceTest, Block3PCookieNoPrompt) {
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

TEST_F(PrivacySandboxServiceTest, BlockAllCookiesNoPrompt) {
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
  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX,
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

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PrivacySandboxServiceTest, DeviceLocalAccountUser) {
  // No prompt should be shown if the user is associated with a device local
  // account on CrOS.
  SetupPromptTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  // No prompt should be shown for a public session account.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedTestPublicSessionLoginState login_state;
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

TEST_F(PrivacySandboxServiceTest, PrivacySandboxPromptNoticeWaiting) {
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
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kPromptWaiting, 1);
}

TEST_F(PrivacySandboxServiceTest,
       FirstPartySetsNotRelevantMetricAllowedCookies) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(true));
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxService::FirstPartySetsState::kFpsNotRelevant, 1);
}

TEST_F(PrivacySandboxServiceTest,
       FirstPartySetsNotRelevantMetricBlockedCookies) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(true));
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxService::FirstPartySetsState::kFpsNotRelevant, 1);
}

TEST_F(PrivacySandboxServiceTest, FirstPartySetsEnabledMetric) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(true));
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxService::FirstPartySetsState::kFpsEnabled, 1);
}

TEST_F(PrivacySandboxServiceTest, FirstPartySetsDisabledMetric) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(false));
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxService::FirstPartySetsState::kFpsDisabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxPromptConsentWaiting) {
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
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kPromptWaiting, 1);
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
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kPromptOffV1OffDisabled, 1);
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
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kPromptOffV1OffEnabled, 1);
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
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kPromptOffRestricted, 1);
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
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kPromptOffManagedEnabled, 1);
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
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kPromptOffManagedDisabled, 1);
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
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kPromptOff3PCOffEnabled, 1);
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
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kPromptOff3PCOffDisabled, 1);
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
  CreateService();

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
  CreateService();

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
  CreateService();

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
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kNoticeShownDisabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxManuallyControlledEnabled) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationManuallyControlled,
                       std::make_unique<base::Value>(true));
  CreateService();
  histogram_tester.ExpectUniqueSample(kPrivacySandboxStartupHistogram,
                                      PrivacySandboxService::PSStartupStates::
                                          kPromptOffManuallyControlledEnabled,
                                      1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxManuallyControlledDisabled) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kPrivacySandboxNoConfirmationManuallyControlled,
                       std::make_unique<base::Value>(true));
  CreateService();
  histogram_tester.ExpectUniqueSample(kPrivacySandboxStartupHistogram,
                                      PrivacySandboxService::PSStartupStates::
                                          kPromptOffManuallyControlledDisabled,
                                      1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxNoPromptDisabled) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(false));
  CreateService();
  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kNoPromptRequiredDisabled, 1);
}

TEST_F(PrivacySandboxServiceTest, PrivacySandboxNoPromptEnabled) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                       std::make_unique<base::Value>(true));
  CreateService();
  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxStartupHistogram,
      PrivacySandboxService::PSStartupStates::kNoPromptRequiredEnabled, 1);
}

TEST_F(PrivacySandboxServiceTest, MetricsLoggingOccursCorrectly) {
  base::HistogramTester histograms;
  const std::string histogram_name = "Settings.PrivacySandbox.Enabled";

  // The histogram should start off empty.
  histograms.ExpectTotalCount(histogram_name, 0);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  CreateService();

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

  CreateService();

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

  CreateService();

  histograms.ExpectTotalCount(histogram_name, 3);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledBlockAll),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  CreateService();

  histograms.ExpectTotalCount(histogram_name, 4);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledAllowAll),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  CreateService();

  histograms.ExpectTotalCount(histogram_name, 5);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledBlock3P),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  CreateService();

  histograms.ExpectTotalCount(histogram_name, 6);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::PrivacySandboxService::
                           SettingsPrivacySandboxEnabled::kPSDisabledBlockAll),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});

  CreateService();

  histograms.ExpectTotalCount(histogram_name, 7);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledPolicyBlockAll),
      1);
}

TEST_F(PrivacySandboxServiceTest, SampleFpsData) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxFirstPartySetsUI,
      {{"use-sample-sets", "true"}});
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  prefs()->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled, true);

  EXPECT_EQ(u"google.com",
            privacy_sandbox_service()->GetFirstPartySetOwnerForDisplay(
                GURL("https://mail.google.com.au")));
  EXPECT_EQ(u"google.com",
            privacy_sandbox_service()->GetFirstPartySetOwnerForDisplay(
                GURL("https://youtube.com")));
  EXPECT_EQ(absl::nullopt,
            privacy_sandbox_service()->GetFirstPartySetOwnerForDisplay(
                GURL("https://example.com")));
}

TEST_F(PrivacySandboxServiceTest,
       GetFirstPartySetOwner_SimulatedFpsData_DisabledWhen3pcAllowed) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets(
      kFirstPartySetsVersion,
      {{associate1_site,
        {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                 0)}}},
      {});

  // Simulate 3PC are allowed while:
  // - FPS pref is enabled
  // - FPS backend Feature is enabled
  // - FPS UI Feature is enabled
  feature_list()->InitWithFeatures(
      {privacy_sandbox::kPrivacySandboxFirstPartySetsUI,
       features::kFirstPartySets},
      {});
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(true));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();
  // We shouldn't get associate1's owner since FPS is disabled.
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl),
            absl::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       GetFirstPartySetOwner_SimulatedFpsData_DisabledWhenAllCookiesBlocked) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets(
      kFirstPartySetsVersion,
      {{associate1_site,
        {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                 0)}}},
      {});

  // Simulate all cookies are blocked while:
  // - FPS pref is enabled
  // - FPS backend Feature is enabled
  // - FPS UI Feature is enabled
  feature_list()->InitWithFeatures(
      {privacy_sandbox::kPrivacySandboxFirstPartySetsUI,
       features::kFirstPartySets},
      {});
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(true));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();
  // We shouldn't get associate1's owner since FPS is disabled.
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl),
            absl::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       GetFirstPartySetOwner_SimulatedFpsData_DisabledByFpsUiFeature) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets(
      kFirstPartySetsVersion,
      {{associate1_site,
        {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                 0)}}},
      {});

  // Simulate FPS UI feature disabled while:
  // - FPS pref is enabled
  // - FPS backend Feature is enabled
  // - 3PC are being blocked
  feature_list()->InitWithFeatures(
      {features::kFirstPartySets},
      {privacy_sandbox::kPrivacySandboxFirstPartySetsUI});
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(true));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();

  // We shouldn't get associate1's owner since FPS is disabled.
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl),
            absl::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       GetFirstPartySetOwner_SimulatedFpsData_DisabledByFpsFeature) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets(
      kFirstPartySetsVersion,
      {{associate1_site,
        {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                 0)}}},
      {});

  // Simulate FPS backend feature disabled while:
  // - FPS pref is enabled
  // - FPS UI Feature is enabled
  // - 3PC are being blocked
  feature_list()->InitWithFeatures(
      {privacy_sandbox::kPrivacySandboxFirstPartySetsUI},
      {features::kFirstPartySets});
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(true));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());
  first_party_sets_policy_service()->InitForTesting();

  // We shouldn't get associate1's owner since FPS is disabled.
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl),
            absl::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       GetFirstPartySetOwner_SimulatedFpsData_DisabledByFpsPref) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets(
      kFirstPartySetsVersion,
      {{associate1_site,
        {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                 0)}}},
      {});

  // Simulate FPS pref disabled while:
  // - FPS UI Feature is enabled
  // - FPS backend Feature is enabled
  // - 3PC are being blocked
  feature_list()->InitWithFeatures(
      {features::kFirstPartySets,
       privacy_sandbox::kPrivacySandboxFirstPartySetsUI},
      {});
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(false));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();

  // We shouldn't get associate1's owner since FPS is disabled.
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl),
            absl::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       SimulatedFpsData_FpsEnabled_WithoutGlobalSets) {
  GURL primary_gurl("https://primary.test");
  GURL associate1_gurl("https://associate1.test");
  GURL associate2_gurl("https://associate2.test");
  net::SchemefulSite primary_site(primary_gurl);
  net::SchemefulSite associate1_site(associate1_gurl);
  net::SchemefulSite associate2_site(associate2_gurl);

  // Set up state that fully enables the First-Party Sets for UI; blocking 3PC,
  // and enabling the FPS UI and backend features and the FPS enabled pref.
  feature_list()->InitWithFeatures(
      {features::kFirstPartySets,
       privacy_sandbox::kPrivacySandboxFirstPartySetsUI},
      {});
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(true));

  // Verify `GetFirstPartySetOwner` returns empty if FPS is enabled but the
  // Global sets are not ready yet.
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl),
            absl::nullopt);
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate2_gurl),
            absl::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       SimulatedFpsData_FpsEnabled_WithGlobalSetsAndProfileSets) {
  GURL primary_gurl("https://primary.test");
  GURL associate1_gurl("https://associate1.test");
  GURL associate2_gurl("https://associate2.test");
  net::SchemefulSite primary_site(primary_gurl);
  net::SchemefulSite associate1_site(associate1_gurl);
  net::SchemefulSite associate2_site(associate2_gurl);

  // Set up state that fully enables the First-Party Sets for UI; blocking 3PC,
  // and enabling the FPS UI and backend features and the FPS enabled pref.
  feature_list()->InitWithFeatures(
      {features::kFirstPartySets,
       privacy_sandbox::kPrivacySandboxFirstPartySetsUI},
      {});
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(true));

  // Simulate that the Global First-Party Sets are ready with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test", "https://associate2.test"] }
  mock_first_party_sets_handler().SetGlobalSets(net::GlobalFirstPartySets(
      kFirstPartySetsVersion,
      {{associate1_site,
        {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated, 0)}},
       {associate2_site,
        {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                 1)}}},
      {}));

  // Simulate that associate2 is removed from the Global First-Party Sets for
  // this profile.
  mock_first_party_sets_handler().SetContextConfig(
      net::FirstPartySetsContextConfig(
          {{net::SchemefulSite(GURL("https://associate2.test")),
            net::FirstPartySetEntryOverride()}}));

  first_party_sets_policy_service()->InitForTesting();

  // Verify that primary owns associate1, but no longer owns associate2.
  EXPECT_EQ(
      privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl).value(),
      primary_site);
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate2_gurl),
            absl::nullopt);
}

TEST_F(PrivacySandboxServiceTest, FpsPrefInit) {
  // Check that the init of the FPS pref occurs correctly.
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));

  // Whilst the FPS UI is not available, the pref should not be init.
  feature_list()->InitAndDisableFeature(
      privacy_sandbox::kPrivacySandboxFirstPartySetsUI);

  CreateService();
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized));

  // If the UI is available, the user blocks 3PC, and the pref has not been
  // previously init, it should be.
  ClearFpsUserPrefs(prefs());
  feature_list()->Reset();
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxFirstPartySetsUI);

  CreateService();
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized));

  // Once the pref has been init, it should not be re-init, and updated user
  // cookie settings should not impact it.
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));

  CreateService();
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized));

  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized));

  // Blocking all cookies should also init the FPS pref to off.
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));

  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CreateService();
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized));
}

TEST_F(PrivacySandboxServiceTest, UsesFpsSampleSetsWhenProvided) {
  // Confirm that when the FPS sample sets are provided, they are used to answer
  // First-Party Sets queries instead of the actual sets.

  // Set up state that fully enables the First-Party Sets for UI; blocking
  // 3PC, and enabling the FPS UI and backend features and the FPS enabled pref.
  //
  // Note: this indicates that the sample sets should be used.
  feature_list()->InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kFirstPartySets, {}},
                            {privacy_sandbox::kPrivacySandboxFirstPartySetsUI,
                             {{"use-sample-sets", "true"}}}},
      /*disabled_features=*/{});
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(true));

  // Simulate that the Global First-Party Sets are ready with the following
  // set:
  // { primary: "https://youtube-primary.test",
  // associatedSites: ["https://youtube.com"]
  // }
  net::SchemefulSite youtube_primary_site(GURL("https://youtube-primary.test"));
  GURL youtube_gurl("https://youtube.com");
  net::SchemefulSite youtube_site(youtube_gurl);

  mock_first_party_sets_handler().SetGlobalSets(net::GlobalFirstPartySets(
      kFirstPartySetsVersion,
      {{youtube_site,
        {net::FirstPartySetEntry(youtube_primary_site,
                                 net::SiteType::kAssociated, 0)}}},
      {}));

  // Simulate that https://google.de is moved into a new First-Party Set for
  // this profile.
  mock_first_party_sets_handler().SetContextConfig(
      net::FirstPartySetsContextConfig(
          {{net::SchemefulSite(GURL("https://google.de")),
            net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                net::SchemefulSite(GURL("https://new-primary.test")),
                net::SiteType::kAssociated, 0))}}));

  first_party_sets_policy_service()->InitForTesting();

  // Expect queries to be resolved based on the FPS sample sets.
  EXPECT_GT(privacy_sandbox_service()->GetSampleFirstPartySets().size(), 0u);
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(
                GURL("https://youtube.com")),
            net::SchemefulSite(GURL("https://google.com")));
  EXPECT_TRUE(privacy_sandbox_service()->IsPartOfManagedFirstPartySet(
      net::SchemefulSite(GURL("https://googlesource.com"))));
  EXPECT_FALSE(privacy_sandbox_service()->IsPartOfManagedFirstPartySet(
      net::SchemefulSite(GURL("https://google.de"))));

  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {features::kFirstPartySets,
       privacy_sandbox::kPrivacySandboxFirstPartySetsUI},
      {});
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxFirstPartySetsEnabled,
                       std::make_unique<base::Value>(true));

  // Expect queries to be resolved based on the FPS backend.
  EXPECT_EQ(privacy_sandbox_service()->GetSampleFirstPartySets().size(), 0u);
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(youtube_gurl),
            youtube_primary_site);
  EXPECT_FALSE(privacy_sandbox_service()->IsPartOfManagedFirstPartySet(
      net::SchemefulSite(GURL("https://googlesource.com"))));
  EXPECT_TRUE(privacy_sandbox_service()->IsPartOfManagedFirstPartySet(
      net::SchemefulSite(GURL("https://google.de"))));
}

class PrivacySandboxServiceTestNonRegularProfile
    : public PrivacySandboxServiceTest {
  profile_metrics::BrowserProfileType GetProfileType() override {
    return profile_metrics::BrowserProfileType::kSystem;
  }
};

TEST_F(PrivacySandboxServiceTestNonRegularProfile, NoMetricsRecorded) {
  // Check that non-regular profiles do not record metrics.
  base::HistogramTester histograms;
  const std::string histogram_name = "Settings.PrivacySandbox.Enabled";

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  CreateService();

  // The histogram should remain empty.
  histograms.ExpectTotalCount(histogram_name, 0);
}

TEST_F(PrivacySandboxServiceTestNonRegularProfile, NoPromptRequired) {
  CreateService();
  // Non-regular profiles should never have a prompt shown.
  SetupPromptTestState(feature_list(), prefs(),
                       {/*consent_required=*/true,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());

  SetupPromptTestState(feature_list(), prefs(),
                       {/*consent_required=*/false,
                        /*old_api_pref=*/true,
                        /*new_api_pref=*/false,
                        /*notice_displayed=*/false,
                        /*consent_decision_made=*/false,
                        /*confirmation_not_shown=*/false});
  EXPECT_EQ(PrivacySandboxService::PromptType::kNone,
            privacy_sandbox_service()->GetRequiredPromptType());
}

class PrivacySandboxServicePromptTestBase {
 public:
  PrivacySandboxServicePromptTestBase() {
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUp() {
    user_manager_ = std::make_unique<ash::FakeChromeUserManager>();
    user_manager_->Initialize();
  }

  void TearDown() {
    // Clean up user manager.
    user_manager_->Shutdown();
    user_manager_->Destroy();
    user_manager_.reset();
  }
#endif

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
  std::unique_ptr<ash::FakeChromeUserManager> user_manager_;
#endif
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  MockPrivacySandboxSettings privacy_sandbox_settings_;
};

class PrivacySandboxServicePromptTest
    : public PrivacySandboxServicePromptTestBase,
      public testing::Test {
#if BUILDFLAG(IS_CHROMEOS_ASH)
 public:
  void SetUp() override { PrivacySandboxServicePromptTestBase::SetUp(); }

  void TearDown() override { PrivacySandboxServicePromptTestBase::TearDown(); }
#endif
};

TEST_F(PrivacySandboxServicePromptTest, RestrictedPrompt) {
  // Confirm that when the Privacy Sandbox is restricted, that no prompt is
  // shown.
  SetupPromptTestState(feature_list(), prefs(),
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

TEST_F(PrivacySandboxServicePromptTest, ManagedNoPrompt) {
  // Confirm that when the Privacy Sandbox is managed, that no prompt is
  // shown.
  SetupPromptTestState(feature_list(), prefs(),
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

TEST_F(PrivacySandboxServicePromptTest, ManuallyControlledNoPrompt) {
  // Confirm that if the Privacy Sandbox V2 is manually controlled by the user,
  // that no prompt is shown.
  SetupPromptTestState(feature_list(), prefs(),
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

TEST_F(PrivacySandboxServicePromptTest, NoParamNoPrompt) {
  // Confirm that if neither the consent or notice parameter is set, no prompt
  // is required.
  EXPECT_EQ(
      PrivacySandboxService::PromptType::kNone,
      PrivacySandboxService::GetRequiredPromptTypeInternal(
          prefs(), profile_metrics::BrowserProfileType::kRegular,
          privacy_sandbox_settings(), /*third_party_cookies_blocked=*/false));
}

class PrivacySandboxServiceDeathTest
    : public PrivacySandboxServicePromptTestBase,
      public testing::TestWithParam<int> {
#if BUILDFLAG(IS_CHROMEOS_ASH)
 public:
  void SetUp() override { PrivacySandboxServicePromptTestBase::SetUp(); }

  void TearDown() override { PrivacySandboxServicePromptTestBase::TearDown(); }
#endif
};

TEST_P(PrivacySandboxServiceDeathTest, GetRequiredPromptType) {
  const auto& test_case = kPromptTestCases[GetParam()];
  privacy_sandbox_settings()->SetUpDefaultResponse();

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

  SetupPromptTestState(feature_list(), prefs(), test_case.test_setup);
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

TEST_F(PrivacySandboxServiceTestCoverageTest, PromptTestCoverage) {
  // Confirm that the set of prompt test cases exhaustively covers all possible
  // combinations of input.
  std::set<int> test_case_properties;
  for (const auto& test_case : kPromptTestCases) {
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
  EXPECT_EQ(test_case_properties.size(), kPromptTestCases.size());
  EXPECT_EQ(64u, test_case_properties.size());
}
