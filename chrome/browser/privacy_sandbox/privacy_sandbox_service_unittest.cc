// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_topics/test_util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/privacy_sandbox/mock_privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"
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
using PromptAction = PrivacySandboxService::PromptAction;
using PromptSuppressedReason = PrivacySandboxService::PromptSuppressedReason;
using PromptType = PrivacySandboxService::PromptType;

// C++20 introduces the "using enum" construct, which significantly reduces the
// required verbosity here. C++20 is support is coming to Chromium
// (crbug.com/1284275), with Mac / Windows / Linux support at the time of
// writing.
// TODO (crbug.com/1401686): Replace groups with commented lines when C++20 is
// supported.

// using enum privacy_sandbox_test_util::TestState;
using privacy_sandbox_test_util::StateKey;
constexpr auto kHasCurrentTopics = StateKey::kHasCurrentTopics;
constexpr auto kHasBlockedTopics = StateKey::kHasBlockedTopics;
constexpr auto kAdvanceClockBy = StateKey::kAdvanceClockBy;
constexpr auto kActiveTopicsConsent = StateKey::kActiveTopicsConsent;

// using enum privacy_sandbox_test_util::InputKey;
using privacy_sandbox_test_util::InputKey;
constexpr auto kTopicsToggleNewValue = InputKey::kTopicsToggleNewValue;
constexpr auto kTopFrameOrigin = InputKey::kTopFrameOrigin;
constexpr auto kAdMeasurementReportingOrigin =
    InputKey::kAdMeasurementReportingOrigin;
constexpr auto kFledgeAuctionPartyOrigin = InputKey::kFledgeAuctionPartyOrigin;

// using enum privacy_sandbox_test_util::TestOutput;
using privacy_sandbox_test_util::OutputKey;
constexpr auto kTopicsConsentGiven = OutputKey::kTopicsConsentGiven;
constexpr auto kTopicsConsentLastUpdateReason =
    OutputKey::kTopicsConsentLastUpdateReason;
constexpr auto kTopicsConsentLastUpdateTime =
    OutputKey::kTopicsConsentLastUpdateTime;
constexpr auto kTopicsConsentStringIdentifiers =
    OutputKey::kTopicsConsentStringIdentifiers;

using privacy_sandbox_test_util::MultipleInputKeys;
using privacy_sandbox_test_util::MultipleOutputKeys;
using privacy_sandbox_test_util::MultipleStateKeys;
using privacy_sandbox_test_util::SiteDataExceptions;
using privacy_sandbox_test_util::TestCase;
using privacy_sandbox_test_util::TestInput;
using privacy_sandbox_test_util::TestOutput;
using privacy_sandbox_test_util::TestState;

const char kFirstPartySetsStateHistogram[] = "Settings.FirstPartySets.State";
const char kPrivacySandboxStartupHistogram[] =
    "Settings.PrivacySandbox.StartupState";

const base::Version kFirstPartySetsVersion("1.2.3");

constexpr int kTestTaxonomyVersion = 1;

class TestPrivacySandboxService
    : public privacy_sandbox_test_util::PrivacySandboxServiceTestInterface {
 public:
  explicit TestPrivacySandboxService(PrivacySandboxService* service)
      : service_(service) {}

  // PrivacySandboxServiceTestInterface
  void TopicsToggleChanged(bool new_value) const override {
    service_->TopicsToggleChanged(new_value);
  }
  void SetTopicAllowed(privacy_sandbox::CanonicalTopic topic,
                       bool allowed) override {
    service_->SetTopicAllowed(topic, allowed);
  }
  bool TopicsHasActiveConsent() const override {
    return service_->TopicsHasActiveConsent();
  }
  privacy_sandbox::TopicsConsentUpdateSource TopicsConsentLastUpdateSource()
      const override {
    return service_->TopicsConsentLastUpdateSource();
  }
  base::Time TopicsConsentLastUpdateTime() const override {
    return service_->TopicsConsentLastUpdateTime();
  }
  std::string TopicsConsentLastUpdateText() const override {
    return service_->TopicsConsentLastUpdateText();
  }
  void ForceChromeBuildForTests(bool force_chrome_build) const override {
    service_->ForceChromeBuildForTests(force_chrome_build);
  }
  int GetRequiredPromptType() const override {
    return static_cast<int>(service_->GetRequiredPromptType());
  }
  void PromptActionOccurred(int action) const override {
    service_->PromptActionOccurred(static_cast<PromptAction>(action));
  }

 private:
  raw_ptr<PrivacySandboxService> service_;
};

class TestInterestGroupManager : public content::InterestGroupManager {
 public:
  void SetInterestGroupDataKeys(
      const std::vector<InterestGroupDataKey>& data_keys) {
    data_keys_ = data_keys;
  }

  // content::InterestGroupManager:
  void GetAllInterestGroupJoiningOrigins(
      base::OnceCallback<void(std::vector<url::Origin>)> callback) override {
    NOTREACHED();
  }
  void GetAllInterestGroupDataKeys(
      base::OnceCallback<void(std::vector<InterestGroupDataKey>)> callback)
      override {
    std::move(callback).Run(data_keys_);
  }
  void RemoveInterestGroupsByDataKey(InterestGroupDataKey data_key,
                                     base::OnceClosure callback) override {
    NOTREACHED();
  }

 private:
  std::vector<InterestGroupDataKey> data_keys_;
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

// Remove any user preference settings for Anti-abuse related preferences,
// returning them to their default value.
void ResetAntiAbuseSettings(
    sync_preferences::TestingPrefServiceSyncable* pref_service,
    HostContentSettingsMap* host_content_settings_map) {
  pref_service->RemoveUserPref(prefs::kPrivacySandboxAntiAbuseInitialized);
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::ANTI_ABUSE, CONTENT_SETTING_ALLOW);
}

std::vector<int> GetTopicsSettingsStringIdentifiers(bool did_consent,
                                                    bool has_current_topics,
                                                    bool has_blocked_topics) {
  if (did_consent && !has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_DISABLED,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (did_consent && has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_DISABLED,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && has_current_topics && has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && has_current_topics && !has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && !has_current_topics && has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && !has_current_topics && !has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  }

  NOTREACHED() << "Invalid topics settings consent state";
  return {};
}

std::vector<int> GetTopicsConfirmationStringIdentifiers() {
  return {IDS_PRIVACY_SANDBOX_M1_CONSENT_TITLE,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_1,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_2,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_3,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_4,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_EXPAND_LABEL,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_1,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_2,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_3,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_LINK};
}

}  // namespace

class PrivacySandboxServiceTest : public testing::Test {
 public:
  PrivacySandboxServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_attestations_(
            privacy_sandbox::PrivacySandboxAttestations::CreateForTesting()) {}

  void SetUp() override {
    InitializeFeaturesBeforeStart();
    CreateService();

    base::RunLoop run_loop;
    first_party_sets_policy_service_.WaitForFirstInitCompleteForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
    first_party_sets_policy_service_.ResetForTesting();
  }

  virtual void InitializeFeaturesBeforeStart() {}

  virtual std::unique_ptr<
      privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/false);
    return mock_delegate;
  }

  void CreateService() {
    auto mock_delegate = CreateMockDelegate();
    mock_delegate_ = mock_delegate.get();

    privacy_sandbox_settings_ =
        std::make_unique<privacy_sandbox::PrivacySandboxSettingsImpl>(
            std::move(mock_delegate), host_content_settings_map(),
            cookie_settings(), prefs());
#if !BUILDFLAG(IS_ANDROID)
    mock_sentiment_service_ =
        std::make_unique<::testing::NiceMock<MockTrustSafetySentimentService>>(
            profile());
#endif
    privacy_sandbox_service_ = std::make_unique<PrivacySandboxService>(
        privacy_sandbox_settings(), cookie_settings(), profile()->GetPrefs(),
        test_interest_group_manager(), GetProfileType(),
        browsing_data_remover(), host_content_settings_map(),
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
  privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate*
  mock_delegate() {
    return mock_delegate_;
  }
  first_party_sets::ScopedMockFirstPartySetsHandler&
  mock_first_party_sets_handler() {
    return mock_first_party_sets_handler_;
  }
  first_party_sets::FirstPartySetsPolicyService*
  first_party_sets_policy_service() {
    return &first_party_sets_policy_service_;
  }
  content::BrowserTaskEnvironment* browser_task_environment() {
    return &browser_task_environment_;
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
  raw_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate,
          DanglingUntriaged>
      mock_delegate_;

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
  privacy_sandbox::ScopedPrivacySandboxAttestations scoped_attestations_;

  std::unique_ptr<PrivacySandboxService> privacy_sandbox_service_;
};

TEST_F(PrivacySandboxServiceTest, GetFledgeJoiningEtldPlusOne) {
  // Confirm that the set of FLEDGE origins which were top-frame for FLEDGE join
  // actions is correctly converted into a list of eTLD+1s.

  using FledgeTestCase =
      std::pair<std::vector<url::Origin>, std::vector<std::string>>;

  // Items which map to the same eTLD+1 should be coalesced into a single entry.
  FledgeTestCase test_case_1 = {
      {url::Origin::Create(GURL("https://www.example.com")),
       url::Origin::Create(GURL("https://example.com:8080")),
       url::Origin::Create(GURL("http://www.example.com"))},
      {"example.com"}};

  // eTLD's should return the host instead, this is relevant for sites which
  // are themselves on the PSL, e.g. github.io.
  FledgeTestCase test_case_2 = {
      {
          url::Origin::Create(GURL("https://co.uk")),
          url::Origin::Create(GURL("http://co.uk")),
          url::Origin::Create(GURL("http://example.co.uk")),
      },
      {"co.uk", "example.co.uk"}};

  // IP addresses should also return the host.
  FledgeTestCase test_case_3 = {
      {
          url::Origin::Create(GURL("https://192.168.1.2")),
          url::Origin::Create(GURL("https://192.168.1.2:8080")),
          url::Origin::Create(GURL("https://192.168.1.3:8080")),
      },
      {"192.168.1.2", "192.168.1.3"}};

  // Results should be alphabetically ordered.
  FledgeTestCase test_case_4 = {{
                                    url::Origin::Create(GURL("https://d.com")),
                                    url::Origin::Create(GURL("https://b.com")),
                                    url::Origin::Create(GURL("https://a.com")),
                                    url::Origin::Create(GURL("https://c.com")),
                                },
                                {"a.com", "b.com", "c.com", "d.com"}};

  std::vector<FledgeTestCase> test_cases = {test_case_1, test_case_2,
                                            test_case_3, test_case_4};

  for (const auto& origins_to_expected : test_cases) {
    std::vector<content::InterestGroupManager::InterestGroupDataKey> data_keys;
    base::ranges::transform(
        origins_to_expected.first, std::back_inserter(data_keys),
        [](const auto& origin) {
          return content::InterestGroupManager::InterestGroupDataKey{
              url::Origin::Create(GURL("https://embedded.com")), origin};
        });
    test_interest_group_manager()->SetInterestGroupDataKeys(data_keys);

    bool callback_called = false;
    auto callback = base::BindLambdaForTesting(
        [&](std::vector<std::string> items_for_display) {
          ASSERT_EQ(items_for_display.size(),
                    origins_to_expected.second.size());
          for (size_t i = 0; i < items_for_display.size(); i++) {
            EXPECT_EQ(origins_to_expected.second[i], items_for_display[i]);
          }
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
  for (const auto& site : sites) {
    privacy_sandbox_settings()->SetFledgeJoiningAllowed(site, false);
  }

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
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxConsentDecisionMade));

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
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxConsentDecisionMade));

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
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxNoticeDisplayed));
}

TEST_F(PrivacySandboxServiceTest, PromptActionsUMAActions) {
  base::UserActionTester user_action_tester;

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeShown);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.Shown"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeOpenSettings);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.OpenedSettings"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeAcknowledge);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.Acknowledged"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeDismiss);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.Dismissed"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.ClosedNoInteraction"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentShown);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.Shown"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentAccepted);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.Accepted"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentDeclined);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.Declined"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentMoreInfoOpened);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.LearnMoreExpanded"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentMoreInfoClosed);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.LearnMoreClosed"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentClosedNoDecision);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.ClosedNoInteraction"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeLearnMore);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.LearnMore"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeMoreInfoOpened);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.LearnMoreExpanded"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeMoreInfoClosed);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.LearnMoreClosed"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentMoreButtonClicked);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Consent.MoreButtonClicked"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeMoreButtonClicked);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.Notice.MoreButtonClicked"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kRestrictedNoticeOpenSettings);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.RestrictedNotice.OpenedSettings"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kRestrictedNoticeAcknowledge);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.RestrictedNotice.Acknowledged"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kRestrictedNoticeShown);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.RestrictedNotice.Shown"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::
          kRestrictedNoticeClosedNoInteraction);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount(
             "Settings.PrivacySandbox.RestrictedNotice.ClosedNoInteraction"));

  privacy_sandbox_service()->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kRestrictedNoticeMoreButtonClicked);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.RestrictedNotice.MoreButtonClicked"));
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

TEST_F(PrivacySandboxServiceTest, DisablingTopicsPrefClearsData) {
  // Confirm that when the topics preference is disabled, topics data is
  // deleted. No browsing data remover tasks are started.
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(0);
  // Enabling should not delete data.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  constexpr uint64_t kNoRemovalTask = -1ull;
  EXPECT_EQ(kNoRemovalTask,
            browsing_data_remover()->GetLastUsedRemovalMaskForTesting());

  // Disabling should start delete topics data.
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(1);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
  EXPECT_EQ(kNoRemovalTask,
            browsing_data_remover()->GetLastUsedRemovalMaskForTesting());
}

TEST_F(PrivacySandboxServiceTest, DisablingFledgePrefClearsData) {
  // Confirm that when the fledge preference is disabled, a browsing data
  // remover task is started. Topics data isn't deleted.
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(0);
  // Enabling should not cause a removal task.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
  constexpr uint64_t kNoRemovalTask = -1ull;
  EXPECT_EQ(kNoRemovalTask,
            browsing_data_remover()->GetLastUsedRemovalMaskForTesting());

  // Disabling should start a task clearing all related information.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, false);
  EXPECT_EQ(
      content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS |
          content::BrowsingDataRemover::DATA_TYPE_SHARED_STORAGE |
          content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS_INTERNAL,
      browsing_data_remover()->GetLastUsedRemovalMaskForTesting());
  EXPECT_EQ(base::Time::Min(),
            browsing_data_remover()->GetLastUsedBeginTimeForTesting());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            browsing_data_remover()->GetLastUsedOriginTypeMaskForTesting());
}

TEST_F(PrivacySandboxServiceTest, DisablingAdMeasurementePrefClearsData) {
  // Confirm that when the ad measurement preference is disabled, a browsing
  // data remover task is started. Topics data isn't deleted.
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(0);
  // Enabling should not cause a removal task.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
  constexpr uint64_t kNoRemovalTask = -1ull;
  EXPECT_EQ(kNoRemovalTask,
            browsing_data_remover()->GetLastUsedRemovalMaskForTesting());

  // Disabling should start a task clearing all related information.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, false);
  EXPECT_EQ(
      content::BrowsingDataRemover::DATA_TYPE_ATTRIBUTION_REPORTING |
          content::BrowsingDataRemover::DATA_TYPE_AGGREGATION_SERVICE |
          content::BrowsingDataRemover::DATA_TYPE_PRIVATE_AGGREGATION_INTERNAL,
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
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(24),  // "Blues"
                                      kTestTaxonomyVersion);
  const privacy_sandbox::CanonicalTopic kSecondTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(23),  // "Music & audio"
          kTestTaxonomyVersion);

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
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(24),  // "Blues"
                                      kTestTaxonomyVersion);
  const privacy_sandbox::CanonicalTopic kSecondTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(23),  // "Music & audio"
          kTestTaxonomyVersion);

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
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(10),
                                      kTestTaxonomyVersion);
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

TEST_F(PrivacySandboxServiceTest, TestNoFakeTopicsPrefOff) {
  // Sample data won't be returned for current topics when the pref is off, only
  // the blocked list.
  prefs()->SetUserPref(prefs::kPrivacySandboxM1TopicsEnabled,
                       std::make_unique<base::Value>(false));

  feature_list()->InitWithFeaturesAndParameters(
      {{privacy_sandbox::kPrivacySandboxSettings4,
        {{privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting
              .name,
          "true"}}}},
      {});

  CanonicalTopic topic3(Topic(3), kTestTaxonomyVersion);
  CanonicalTopic topic4(Topic(4), kTestTaxonomyVersion);

  auto* service = privacy_sandbox_service();
  EXPECT_THAT(service->GetCurrentTopTopics(), testing::IsEmpty());
  EXPECT_THAT(service->GetBlockedTopics(), ElementsAre(topic3, topic4));
}

TEST_F(PrivacySandboxServiceTest, TestFakeTopics) {
  std::vector<base::test::FeatureRefAndParams> test_features = {
      {privacy_sandbox::kPrivacySandboxSettings3,
       {{privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting.name,
         "true"}}},
      {privacy_sandbox::kPrivacySandboxSettings4,
       {{privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting.name,
         "true"}}}};

  // Sample data for current topics is only returned when the pref is on.
  prefs()->SetUserPref(prefs::kPrivacySandboxM1TopicsEnabled,
                       std::make_unique<base::Value>(true));

  for (const auto& feature : test_features) {
    feature_list()->Reset();
    feature_list()->InitWithFeaturesAndParameters({feature}, {});
    CanonicalTopic topic1(Topic(1), kTestTaxonomyVersion);
    CanonicalTopic topic2(Topic(2), kTestTaxonomyVersion);
    CanonicalTopic topic3(Topic(3), kTestTaxonomyVersion);
    CanonicalTopic topic4(Topic(4), kTestTaxonomyVersion);
    // Duplicate a topic to test that it doesn't appear in the results in
    // addition to topic4.
    CanonicalTopic topic4_duplicate(Topic(4), kTestTaxonomyVersion - 1);

    auto* service = privacy_sandbox_service();
    EXPECT_THAT(service->GetCurrentTopTopics(), ElementsAre(topic1, topic2));
    EXPECT_THAT(service->GetBlockedTopics(), ElementsAre(topic3, topic4));

    service->SetTopicAllowed(topic1, false);
    EXPECT_THAT(service->GetCurrentTopTopics(), ElementsAre(topic2));
    EXPECT_THAT(service->GetBlockedTopics(),
                ElementsAre(topic1, topic3, topic4));

    service->SetTopicAllowed(topic4, true);
    service->SetTopicAllowed(topic4_duplicate, true);
    EXPECT_THAT(service->GetCurrentTopTopics(), ElementsAre(topic2, topic4));
    EXPECT_THAT(service->GetBlockedTopics(), ElementsAre(topic1, topic3));

    service->SetTopicAllowed(topic1, true);
    service->SetTopicAllowed(topic4, false);
    service->SetTopicAllowed(topic4_duplicate, false);
    EXPECT_THAT(service->GetCurrentTopTopics(), ElementsAre(topic1, topic2));
    EXPECT_THAT(service->GetBlockedTopics(), ElementsAre(topic3, topic4));
  }
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
  EXPECT_EQ(u"münchen.de",
            privacy_sandbox_service()->GetFirstPartySetOwnerForDisplay(
                GURL("https://muenchen.de")));
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

TEST_F(PrivacySandboxServiceTest, AntiAbuseContentSettingInit) {
  // Check that the init of the Anti-abuse pref occurs correctly.
  ResetAntiAbuseSettings(prefs(), host_content_settings_map());
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));

  // If the user blocks 3PC, and the pref has not been previously init, it
  // should be.
  ResetAntiAbuseSettings(prefs(), host_content_settings_map());
  CreateService();
  EXPECT_EQ(host_content_settings_map()->GetDefaultContentSetting(
                ContentSettingsType::ANTI_ABUSE, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxAntiAbuseInitialized));

  // Once the setting has been init, it should not be re-init, and updated user
  // cookie settings should not impact it.
  ResetAntiAbuseSettings(prefs(), host_content_settings_map());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));

  CreateService();
  EXPECT_EQ(host_content_settings_map()->GetDefaultContentSetting(
                ContentSettingsType::ANTI_ABUSE, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxAntiAbuseInitialized));

  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  EXPECT_EQ(host_content_settings_map()->GetDefaultContentSetting(
                ContentSettingsType::ANTI_ABUSE, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxAntiAbuseInitialized));

  // Blocking all cookies should also init the Anti-abuse setting to blocked.
  ResetAntiAbuseSettings(prefs(), host_content_settings_map());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));

  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CreateService();
  EXPECT_EQ(host_content_settings_map()->GetDefaultContentSetting(
                ContentSettingsType::ANTI_ABUSE, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxAntiAbuseInitialized));
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
  privacy_sandbox_test_util::MockPrivacySandboxSettings*
  privacy_sandbox_settings() {
    return &privacy_sandbox_settings_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::FakeChromeUserManager> user_manager_;
#endif
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  privacy_sandbox_test_util::MockPrivacySandboxSettings
      privacy_sandbox_settings_;
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

class PrivacySandboxServiceM1Test : public PrivacySandboxServiceTest {
 public:
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeature(
        privacy_sandbox::kPrivacySandboxSettings4);
  }

 protected:
  void RunTestCase(const TestState& test_state,
                   const TestInput& test_input,
                   const TestOutput& test_output) {
    auto user_provider = std::make_unique<content_settings::MockProvider>();
    auto* user_provider_raw = user_provider.get();
    auto managed_provider = std::make_unique<content_settings::MockProvider>();
    auto* managed_provider_raw = managed_provider.get();
    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(user_provider),
        HostContentSettingsMap::PREF_PROVIDER);
    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(managed_provider),
        HostContentSettingsMap::POLICY_PROVIDER);
    auto service_wrapper = TestPrivacySandboxService(privacy_sandbox_service());

    privacy_sandbox_test_util::RunTestCase(
        browser_task_environment(), prefs(), host_content_settings_map(),
        mock_delegate(), mock_browsing_topics_service(),
        privacy_sandbox_settings(), &service_wrapper, user_provider_raw,
        managed_provider_raw, TestCase(test_state, test_input, test_output));
  }

  void DisablePrivacySandboxPromptEnabledPolicy() {
    prefs()->SetManagedPref(
        prefs::kPrivacySandboxM1PromptSuppressed,
        base::Value(static_cast<int>(
            PrivacySandboxService::PromptSuppressedReason::kPolicy)));
  }
};

TEST_F(PrivacySandboxServiceM1Test, TopicsConsentDefault) {
  RunTestCase(
      TestState{}, TestInput{},
      TestOutput{{kTopicsConsentGiven, false},
                 {kTopicsConsentLastUpdateReason,
                  privacy_sandbox::TopicsConsentUpdateSource::kDefaultValue},
                 {kTopicsConsentLastUpdateTime, base::Time()},
                 {kTopicsConsentStringIdentifiers, std::vector<int>()}});
}

TEST_F(PrivacySandboxServiceM1Test, TopicsConsentSettings_EnableWithBlocked) {
  // Note that when testing for enabling topics, there can never have been
  // current topics in prod code.
  RunTestCase(
      TestState{{kActiveTopicsConsent, false},
                {kHasCurrentTopics, false},
                {kHasBlockedTopics, true},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, true},
      },
      TestOutput{
          {kTopicsConsentGiven, true},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/true,
                                              /*has_current_topics=*/false,
                                              /*has_blocked_topics=*/true)},
      });
}

TEST_F(PrivacySandboxServiceM1Test, TopicsConsentSettings_EnableNoBlocked) {
  RunTestCase(
      TestState{{kActiveTopicsConsent, false},
                {kHasCurrentTopics, false},
                {kHasBlockedTopics, false},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, true},
      },
      TestOutput{
          {kTopicsConsentGiven, true},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/true,
                                              /*has_current_topics=*/false,
                                              /*has_blocked_topics=*/false)},
      });
}

TEST_F(PrivacySandboxServiceM1Test,
       TopicsConsentSettings_DisableCurrentAndBlocked) {
  RunTestCase(
      TestState{{kActiveTopicsConsent, true},
                {kHasCurrentTopics, true},
                {kHasBlockedTopics, true},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, false},
      },
      TestOutput{
          {kTopicsConsentGiven, false},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/false,
                                              /*has_current_topics=*/true,
                                              /*has_blocked_topics=*/true)},
      });
}

TEST_F(PrivacySandboxServiceM1Test, TopicsConsentSettings_DisableBlockedOnly) {
  RunTestCase(
      TestState{{kActiveTopicsConsent, true},
                {kHasCurrentTopics, false},
                {kHasBlockedTopics, true},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, false},
      },
      TestOutput{
          {kTopicsConsentGiven, false},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/false,
                                              /*has_current_topics=*/false,
                                              /*has_blocked_topics=*/true)},
      });
}

TEST_F(PrivacySandboxServiceM1Test, TopicsConsentSettings_DisableCurrentOnly) {
  RunTestCase(
      TestState{{kActiveTopicsConsent, true},
                {kHasCurrentTopics, true},
                {kHasBlockedTopics, false},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, false},
      },
      TestOutput{
          {kTopicsConsentGiven, false},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/false,
                                              /*has_current_topics=*/true,
                                              /*has_blocked_topics=*/false)},
      });
}

TEST_F(PrivacySandboxServiceM1Test,
       TopicsConsentSettings_DisableNoCurrentNoBlocked) {
  RunTestCase(
      TestState{{kActiveTopicsConsent, true},
                {kHasCurrentTopics, false},
                {kHasBlockedTopics, false},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, false},
      },
      TestOutput{
          {kTopicsConsentGiven, false},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/false,
                                              /*has_current_topics=*/false,
                                              /*has_blocked_topics=*/false)},
      });
}

TEST_F(PrivacySandboxServiceM1Test,
       RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Explicitly) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(
          PrivacySandboxService::PromptSuppressedReason::kRestricted));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxService::PromptStartupState::
                           kPromptNotShownDueToPrivacySandboxRestricted),
      /*expected_count=*/1);

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::
                           kThirdPartyCookiesBlocked));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxService::PromptStartupState::
                           kPromptNotShownDueTo3PCBlocked),
      /*expected_count=*/1);

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::
                           kTrialsConsentDeclined));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxService::PromptStartupState::
                           kPromptNotShownDueToTrialConsentDeclined),
      /*expected_count=*/1);

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::
                           kTrialsDisabledAfterNotice));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxService::PromptStartupState::
                           kPromptNotShownDueToTrialsDisabledAfterNoticeShown),
      /*expected_count=*/1);

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::kPolicy));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxService::PromptStartupState::
                           kPromptNotShownDueToManagedState),
      /*expected_count=*/1);

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::
                           kNoticeShownToGuardian));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxService::PromptStartupState::
                           kRestrictedNoticeNotShownDueToNoticeShownToGuardian),
      /*expected_count=*/1);
}

TEST_F(PrivacySandboxServiceM1Test,
       RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Implicitly) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::kNone));

  // Disable one of the K-APIs.
  prefs()->SetManagedPref(prefs::kPrivacySandboxM1TopicsEnabled,
                          base::Value(false));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxService::PromptStartupState::
                           kPromptNotShownDueToManagedState),
      /*expected_count=*/1);
}

TEST_F(PrivacySandboxServiceM1Test,
       RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_EEA) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::kNone));

  base::test::ScopedFeatureList feature_list_consent_required;
  std::map<std::string, std::string> consent_required_feature_param = {
      {std::string(
           privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName),
       "true"},
      {std::string(privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName),
       "false"}};
  feature_list_consent_required.InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      consent_required_feature_param);
  // Not consented
  prefs()->SetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade, false);

  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(
          PrivacySandboxService::PromptStartupState::kEEAConsentPromptWaiting),
      /*expected_count=*/1);

  // Consent decision made and notice acknowledged.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged, true);

  // With topics enabled.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxService::PromptStartupState::
                           kEEAFlowCompletedWithTopicsAccepted),
      /*expected_count=*/1);

  // With topics disabled.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxService::PromptStartupState::
                           kEEAFlowCompletedWithTopicsDeclined),
      /*expected_count=*/1);

  // Consent decision made but notice was not acknowledged.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged, false);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(
          PrivacySandboxService::PromptStartupState::kEEANoticePromptWaiting),
      /*expected_count=*/1);
}

TEST_F(PrivacySandboxServiceM1Test,
       RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_ROW) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::kNone));

  base::test::ScopedFeatureList feature_list_notice_required;
  std::map<std::string, std::string> notice_required_feature_param = {
      {std::string(
           privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName),
       "false"},
      {std::string(privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName),
       "true"}};
  feature_list_notice_required.InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4, notice_required_feature_param);

  // Notice flow not completed.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged, false);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(
          PrivacySandboxService::PromptStartupState::kROWNoticePromptWaiting),
      /*expected_count=*/1);

  // Notice flow completed.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged, true);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(
          PrivacySandboxService::PromptStartupState::kROWNoticeFlowCompleted),
      /*expected_count=*/1);
}

TEST_F(PrivacySandboxServiceM1Test, RecordPrivacySandbox4StartupMetrics_APIs) {
  // Each test for the APIs are scoped below to ensure we start with a clean
  // HistogramTester as each call to `RecordPrivacySandbox4StartupMetrics` emits
  // histograms for all APIs.

  // Topics
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Topics.Enabled",
                                       static_cast<int>(true),
                                       /*expected_count=*/1);

    prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Topics.Enabled",
                                       static_cast<int>(false),
                                       /*expected_count=*/1);
  }

  // Fledge
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Fledge.Enabled",
                                       static_cast<int>(true),
                                       /*expected_count=*/1);
    prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, false);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Fledge.Enabled",
                                       static_cast<int>(false),
                                       /*expected_count=*/1);
  }

  // Ad measurement
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.AdMeasurement.Enabled", static_cast<int>(true),
        /*expected_count=*/1);
    prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, false);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.AdMeasurement.Enabled",
        static_cast<int>(false),
        /*expected_count=*/1);
  }
}

class PrivacySandboxServiceM1RestrictedNoticeTest
    : public PrivacySandboxServiceM1Test {
 public:
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{"notice-required", "true"}, {"restricted-notice", "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticeTest,
       RestrictedPromptActionsUpdatePrefs) {
  // Prompt acknowledge action should update the prefs accordingly.
  RunTestCase(
      TestState{{StateKey::kM1AdMeasurementEnabledUserPrefValue, false},
                {StateKey::kM1RestrictedNoticeAcknowledged, false}},
      TestInput{{InputKey::kPromptAction,
                 static_cast<int>(PromptAction::kRestrictedNoticeAcknowledge)}},
      TestOutput{{OutputKey::kM1AdMeasurementEnabled, true},
                 {OutputKey::kM1RestrictedNoticeAcknowledged, true}});

  // Open settings action should update the prefs accordingly.
  RunTestCase(TestState{{StateKey::kM1AdMeasurementEnabledUserPrefValue, false},
                        {StateKey::kM1RestrictedNoticeAcknowledged, false}},
              TestInput{{InputKey::kPromptAction,
                         static_cast<int>(
                             PromptAction::kRestrictedNoticeOpenSettings)}},
              TestOutput{{OutputKey::kM1AdMeasurementEnabled, true},
                         {OutputKey::kM1RestrictedNoticeAcknowledged, true}});
}

class PrivacySandboxServiceM1DelayCreation
    : public PrivacySandboxServiceM1Test {
 public:
  void SetUp() override {
    // Prevent service from being created by base class.
  }
};

TEST_F(PrivacySandboxServiceM1DelayCreation,
       UnrestrictedRemainsEnabledWithConsent) {
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven, true);
  prefs()->SetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime,
                   base::Time::Now());
  prefs()->SetInteger(
      prefs::kPrivacySandboxTopicsConsentLastUpdateReason,
      static_cast<int>(
          privacy_sandbox::TopicsConsentUpdateSource::kConfirmation));
  prefs()->SetString(prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate,
                     "foo");

  CreateService();

  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled));
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxTopicsConsentGiven));
  EXPECT_EQ(
      base::Time::Now(),
      prefs()->GetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime));
  EXPECT_EQ(privacy_sandbox::TopicsConsentUpdateSource::kConfirmation,
            static_cast<privacy_sandbox::TopicsConsentUpdateSource>(
                prefs()->GetInteger(
                    prefs::kPrivacySandboxTopicsConsentLastUpdateReason)));
  EXPECT_EQ("foo", prefs()->GetString(
                       prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate));
}

TEST_F(PrivacySandboxServiceM1DelayCreation,
       PromptSuppressReasonClearedWhenRestrictedNoticeEnabled) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{"restricted-notice", "true"}});

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(
          PrivacySandboxService::PromptSuppressedReason::kRestricted));

  CreateService();

  EXPECT_EQ(
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::kNone),
      prefs()->GetValue(prefs::kPrivacySandboxM1PromptSuppressed));
}

TEST_F(PrivacySandboxServiceM1DelayCreation,
       PromptSuppressReasonNotClearedWhenRestrictedNoticeDisabled) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{"restricted-notice", "false"}});

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(
          PrivacySandboxService::PromptSuppressedReason::kRestricted));

  CreateService();

  EXPECT_EQ(static_cast<int>(
                PrivacySandboxService::PromptSuppressedReason::kRestricted),
            prefs()->GetValue(prefs::kPrivacySandboxM1PromptSuppressed));
}

class PrivacySandboxServiceM1DelayCreationRestricted
    : public PrivacySandboxServiceM1DelayCreation {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/true);
    return mock_delegate;
  }
};

TEST_F(PrivacySandboxServiceM1DelayCreationRestricted,
       RestrictedDisablesAndClearsConsent) {
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven, true);
  prefs()->SetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime,
                   base::Time::Now());
  prefs()->SetInteger(
      prefs::kPrivacySandboxTopicsConsentLastUpdateReason,
      static_cast<int>(
          privacy_sandbox::TopicsConsentUpdateSource::kConfirmation));
  prefs()->SetString(prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate,
                     "foo");

  CreateService();

  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled));
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxTopicsConsentGiven));
  EXPECT_EQ(
      base::Time(),
      prefs()->GetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime));
  EXPECT_EQ(privacy_sandbox::TopicsConsentUpdateSource::kDefaultValue,
            static_cast<privacy_sandbox::TopicsConsentUpdateSource>(
                prefs()->GetInteger(
                    prefs::kPrivacySandboxTopicsConsentLastUpdateReason)));
  EXPECT_EQ("", prefs()->GetString(
                    prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate));
}

TEST_F(PrivacySandboxServiceM1DelayCreationRestricted,
       RestrictedEnabledDoesntClearAdMeasurementPref) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{"restricted-notice", "true"}});

  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);

  CreateService();

  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled));
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled));
}

class PrivacySandboxServiceM1PromptTest : public PrivacySandboxServiceM1Test {
 public:
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{"consent-required", "true"}, {"notice-required", "false"}});
  }
};

TEST_F(PrivacySandboxServiceM1PromptTest, PrivacySandboxCorrectPromptVersion) {
  // Depending on the feature enabled, a different prompt type may occur.

  // Trials
  feature_list()->Reset();
  feature_list()->InitWithFeaturesAndParameters(
      {{privacy_sandbox::kPrivacySandboxSettings3,
        {{"force-show-consent-for-testing", "true"}}}},
      {privacy_sandbox::kPrivacySandboxSettings4});
  RunTestCase(TestState({}), TestInput{{InputKey::kForceChromeBuild, true}},
              TestOutput{{OutputKey::kPromptType,
                          static_cast<int>(PromptType::kConsent)},
                         {OutputKey::kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});

  // M1
  feature_list()->Reset();
  feature_list()->InitWithFeaturesAndParameters(
      {{privacy_sandbox::kPrivacySandboxSettings4,
        {{"force-show-consent-for-testing", "true"}}}},
      {privacy_sandbox::kPrivacySandboxSettings3});
  RunTestCase(TestState({}), TestInput{{InputKey::kForceChromeBuild, true}},
              TestOutput{{OutputKey::kPromptType,
                          static_cast<int>(PromptType::kM1Consent)},
                         {OutputKey::kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(PrivacySandboxServiceM1PromptTest, NonChromeBuildPrompt) {
  // A case that will normally show a prompt will not if is a non-Chrome build.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)}},
      TestInput{{InputKey::kForceChromeBuild, false}},
      TestOutput{{OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)}});
}
#endif

TEST_F(PrivacySandboxServiceM1PromptTest, ThirdPartyCookiesBlocked) {
  // If third party cookies are blocked, set the suppressed reason as
  // kThirdPartyCookiesBlocked and return kNone.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kCookieControlsModeUserPrefValue,
                 content_settings::CookieControlsMode::kBlockThirdParty}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{{OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(
                      PromptSuppressedReason::kThirdPartyCookiesBlocked)}});
}

TEST_F(PrivacySandboxServiceM1PromptTest, RestrictedPrompt) {
  // If the Privacy Sandbox is restricted, no prompt is shown.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kIsRestrictedAccount, true}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{{OutputKey::kPromptType,
                  static_cast<int>(PrivacySandboxService::PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kRestricted)}});

  // After being restricted, even if the restriction is removed, no prompt
  // should be shown. No call should even need to be made to see if the
  // sandbox is still restricted.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kRestricted)},
                {StateKey::kIsRestrictedAccount, false}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{{OutputKey::kPromptType,
                  static_cast<int>(PrivacySandboxService::PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kRestricted)}});
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PrivacySandboxServiceM1PromptTest, PromptActionsSentimentService) {
  // Settings both consent and notice to be true so that we can loop through all
  // cases interacting with the sentiment service cleanly, without breaking
  // DCHECKs. Other tests / code paths check that PromptActionOccurred is
  // working correctly based on notice and consent, and assert that only one is
  // enabled.
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{"consent-required", "true"},
       {"notice-required", "true"},
       {"restricted-notice", "true"}});

  std::map<PromptAction, TrustSafetySentimentService::FeatureArea>
      expected_feature_areas;
  expected_feature_areas = {
      {PromptAction::kNoticeOpenSettings,
       TrustSafetySentimentService::FeatureArea::
           kPrivacySandbox4NoticeSettings},
      {PromptAction::kNoticeAcknowledge,
       TrustSafetySentimentService::FeatureArea::kPrivacySandbox4NoticeOk},
      {PromptAction::kConsentAccepted,
       TrustSafetySentimentService::FeatureArea::kPrivacySandbox4ConsentAccept},
      {PromptAction::kConsentDeclined,
       TrustSafetySentimentService::FeatureArea::
           kPrivacySandbox4ConsentDecline}};

  for (int enum_value = 0;
       enum_value <= static_cast<int>(PromptAction::kMaxValue); ++enum_value) {
    auto prompt_action = static_cast<PromptAction>(enum_value);
    if (expected_feature_areas.count(prompt_action)) {
      EXPECT_CALL(
          *mock_sentiment_service(),
          InteractedWithPrivacySandbox4(expected_feature_areas[prompt_action]))
          .Times(1);
    } else {
      EXPECT_CALL(*mock_sentiment_service(),
                  InteractedWithPrivacySandbox4(testing::_))
          .Times(0);
    }
    privacy_sandbox_service()->PromptActionOccurred(prompt_action);
    testing::Mock::VerifyAndClearExpectations(mock_sentiment_service());
  }
}
#endif

class PrivacySandboxServiceM1ConsentPromptTest
    : public PrivacySandboxServiceM1PromptTest {};

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, SuppressedConsent) {
  // A case that will normally show a consent will not if there is any
  // suppression reason.
  for (int suppressed_reason = static_cast<int>(PromptSuppressedReason::kNone);
       suppressed_reason <= static_cast<int>(PromptSuppressedReason::kMaxValue);
       ++suppressed_reason) {
    bool suppressed =
        suppressed_reason != static_cast<int>(PromptSuppressedReason::kNone);
    auto expected_prompt =
        suppressed ? PromptType::kNone : PromptType::kM1Consent;
    RunTestCase(
        TestState{{StateKey::kM1PromptSuppressedReason, suppressed_reason},
                  {StateKey::kIsRestrictedAccount, false}},
        TestInput{{InputKey::kForceChromeBuild, true}},
        TestOutput{{OutputKey::kPromptType, static_cast<int>(expected_prompt)},
                   {OutputKey::kM1PromptSuppressedReason, suppressed_reason}});
  }
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, TrialsConsentDeclined) {
  // If a previous consent decision was made to decline the privacy sandbox
  // (privacy_sandbox.apis_enabled_v2 is false), set kTrialsConsentDeclined
  // as suppressed reason and return kNone.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kTrialsConsentDecisionMade, true}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{
          {OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
          {OutputKey::kM1PromptSuppressedReason,
           static_cast<int>(PromptSuppressedReason::kTrialsConsentDeclined)}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, M1ConsentDecisionNotMade) {
  // If m1 consent required, and decision has not been made, return
  // kM1Consent.
  RunTestCase(TestState{{StateKey::kM1PromptSuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {StateKey::kM1ConsentDecisionMade, false}},
              TestInput{{InputKey::kForceChromeBuild, true}},
              TestOutput{{OutputKey::kPromptType,
                          static_cast<int>(PromptType::kM1Consent)},
                         {OutputKey::kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest,
       M1ConsentDecisionMadeAndEEANoticeNotAcknowledged) {
  // If m1 consent decision has been made and the eea notice has not been
  // acknowledged, return kM1NoticeEEA.
  RunTestCase(TestState{{StateKey::kM1PromptSuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {StateKey::kM1ConsentDecisionMade, true}},
              TestInput{{InputKey::kForceChromeBuild, true}},
              TestOutput{{OutputKey::kPromptType,
                          static_cast<int>(PromptType::kM1NoticeEEA)},
                         {OutputKey::kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest,
       M1ConsentDecisionMadeAndEEANoticeAcknowledged) {
  // If m1 consent decision has been made and the eea notice has been
  // acknowledged, return kNone.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kM1ConsentDecisionMade, true},
                {StateKey::kM1EEANoticeAcknowledged, true}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{{OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, ROWNoticeAckTopicsDisabled) {
  // If the user saw the ROW notice, and then disable Topics from settings, and
  // is now in EEA, they should not see a prompt.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kM1RowNoticeAcknowledged, true},
                {StateKey::kM1TopicsEnabledUserPrefValue, false}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{
          {OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
          {OutputKey::kM1PromptSuppressedReason,
           static_cast<int>(
               PromptSuppressedReason::
                   kROWFlowCompletedAndTopicsDisabledBeforeEEAMigration)}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, PromptAction_ConsentAccepted) {
  // Confirm that when the service is informed that the consent prompt was
  // accepted, it correctly adjusts the Privacy Sandbox prefs.
  RunTestCase(
      TestState{{kActiveTopicsConsent, false},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{{InputKey::kPromptAction,
                 static_cast<int>(PromptAction::kConsentAccepted)}},
      TestOutput{
          {OutputKey::kM1ConsentDecisionMade, true},
          {OutputKey::kM1TopicsEnabled, true},
          {OutputKey::kTopicsConsentGiven, true},
          {OutputKey::kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kConfirmation},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsConfirmationStringIdentifiers()}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, PromptAction_ConsentDeclined) {
  // Confirm that when the service is informed that the consent prompt was
  // declined, it correctly adjusts the Privacy Sandbox prefs.
  RunTestCase(
      TestState{{kActiveTopicsConsent, true},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{{InputKey::kPromptAction,
                 static_cast<int>(PromptAction::kConsentDeclined)}},
      TestOutput{
          {OutputKey::kM1ConsentDecisionMade, true},
          {OutputKey::kM1TopicsEnabled, false},
          {OutputKey::kTopicsConsentGiven, false},
          {OutputKey::kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kConfirmation},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsConfirmationStringIdentifiers()}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest,
       PromptAction_EEANoticeAcknowledged) {
  // Confirm that when the service is informed that the eea notice was
  // acknowledged, it correctly adjusts the Privacy Sandbox prefs.
  RunTestCase(TestState{{StateKey::kM1ConsentDecisionMade, true},
                        {StateKey::kM1EEANoticeAcknowledged, false}},
              TestInput{{InputKey::kPromptAction,
                         static_cast<int>(PromptAction::kNoticeAcknowledge)}},
              TestOutput{{OutputKey::kM1EEANoticeAcknowledged, true},
                         {OutputKey::kM1FledgeEnabled, true},
                         {OutputKey::kM1AdMeasurementEnabled, true}});
  RunTestCase(
      TestState{{StateKey::kM1ConsentDecisionMade, true},
                {StateKey::kM1EEANoticeAcknowledged, false}},
      TestInput{{InputKey::kPromptAction,
                 static_cast<int>(PromptAction::kNoticeOpenSettings)}},
      TestOutput{{OutputKey::kM1EEANoticeAcknowledged, true},
                 {OutputKey::kM1FledgeEnabled, true},
                 {OutputKey::kM1AdMeasurementEnabled, true},
                 {OutputKey::kTopicsConsentGiven, false},
                 {OutputKey::kTopicsConsentLastUpdateReason,
                  privacy_sandbox::TopicsConsentUpdateSource::kDefaultValue}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest,
       PromptAction_EEANoticeAcknowledged_ROWNoticeAcknowledged) {
  // Confirm that if the user has already acknowledged an ROW notice, that the
  // EEA notice does not attempt to re-enable APIs. This is important for the
  // ROW -> EEA upgrade flow, where the user may have already visited settings.
  RunTestCase(TestState{{StateKey::kM1ConsentDecisionMade, true},
                        {StateKey::kM1EEANoticeAcknowledged, false},
                        {StateKey::kM1RowNoticeAcknowledged, true}},
              TestInput{{InputKey::kPromptAction,
                         static_cast<int>(PromptAction::kNoticeAcknowledge)}},
              TestOutput{{OutputKey::kM1EEANoticeAcknowledged, true},
                         {OutputKey::kM1FledgeEnabled, false},
                         {OutputKey::kM1AdMeasurementEnabled, false}});
}

class PrivacySandboxServiceM1NoticePromptTest
    : public PrivacySandboxServiceM1PromptTest {
 public:
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{"consent-required", "false"}, {"notice-required", "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1NoticePromptTest, SuppressedNotice) {
  // A case that will normally show a notice will not if there is any
  // suppression reason.
  for (int suppressed_reason = static_cast<int>(PromptSuppressedReason::kNone);
       suppressed_reason <= static_cast<int>(PromptSuppressedReason::kMaxValue);
       ++suppressed_reason) {
    bool suppressed =
        suppressed_reason != static_cast<int>(PromptSuppressedReason::kNone);
    auto expected_prompt =
        suppressed ? PromptType::kNone : PromptType::kM1NoticeROW;
    RunTestCase(
        TestState{{StateKey::kM1PromptSuppressedReason, suppressed_reason}},
        TestInput{{InputKey::kForceChromeBuild, true}},
        TestOutput{{OutputKey::kPromptType, static_cast<int>(expected_prompt)},
                   {OutputKey::kM1PromptSuppressedReason, suppressed_reason}});
  }
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, TrialsDisabledAfterNotice) {
  // If a previous notice was shown and then the privacy sandbox was disabled
  // after (privacy_sandbox.apis_enabled_v2 is false), set
  // kTrialsDisabledAfterNotice as suppressed reason and return kNone.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kTrialsNoticeDisplayed, true}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{{OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(
                      PromptSuppressedReason::kTrialsDisabledAfterNotice)}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, M1NoticeNotAcknowledged) {
  // If m1 notice required, and the row notice has not been acknowledged, return
  // kM1NoticeROW.
  RunTestCase(TestState{{StateKey::kM1PromptSuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {StateKey::kM1RowNoticeAcknowledged, false}},
              TestInput{{InputKey::kForceChromeBuild, true}},
              TestOutput{{OutputKey::kPromptType,
                          static_cast<int>(PromptType::kM1NoticeROW)},
                         {OutputKey::kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, M1NoticeAcknowledged) {
  // If m1 notice required, and the row notice has been acknowledged, return
  // kNone.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kM1RowNoticeAcknowledged, true}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{{OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, M1EEAFlowInterrupted) {
  // If a user has migrated from EEA to ROW and has already completed the eea
  // consent but not yet acknowledged the notice, return kM1NoticeROW.
  RunTestCase(TestState{{StateKey::kM1PromptSuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {StateKey::kM1ConsentDecisionMade, true},
                        {StateKey::kM1EEANoticeAcknowledged, false}},
              TestInput{{InputKey::kForceChromeBuild, true}},
              TestOutput{{OutputKey::kPromptType,
                          static_cast<int>(PromptType::kM1NoticeROW)},
                         {OutputKey::kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, M1EEAFlowCompleted) {
  // If a user has migrated from EEA to ROW and has already completed the eea
  // flow, set kEEAFlowCompleted as suppressed reason return kNone.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kM1ConsentDecisionMade, true},
                {StateKey::kM1EEANoticeAcknowledged, true}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{
          {OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
          {OutputKey::kM1PromptSuppressedReason,
           static_cast<int>(
               PromptSuppressedReason::kEEAFlowCompletedBeforeRowMigration)}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest,
       PromptAction_RowNoticeAcknowledged) {
  // Confirm that when the service is informed that the row notice was
  // acknowledged, it correctly adjusts the Privacy Sandbox prefs.
  RunTestCase(TestState{},
              TestInput{{InputKey::kPromptAction,
                         static_cast<int>(PromptAction::kNoticeAcknowledge)}},
              TestOutput{{OutputKey::kM1RowNoticeAcknowledged, true},
                         {OutputKey::kM1TopicsEnabled, true},
                         {OutputKey::kM1FledgeEnabled, true},
                         {OutputKey::kM1AdMeasurementEnabled, true},
                         {OutputKey::kTopicsConsentGiven, false}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, PromptAction_OpenSettings) {
  // Confirm that when the service is informed that the row notice was
  // acknowledged, it correctly adjusts the Privacy Sandbox prefs.
  RunTestCase(TestState{},
              TestInput{{InputKey::kPromptAction,
                         static_cast<int>(PromptAction::kNoticeOpenSettings)}},
              TestOutput{{OutputKey::kM1RowNoticeAcknowledged, true},
                         {OutputKey::kM1TopicsEnabled, true},
                         {OutputKey::kM1FledgeEnabled, true},
                         {OutputKey::kM1AdMeasurementEnabled, true},
                         {OutputKey::kTopicsConsentGiven, false}});
}

TEST_F(PrivacySandboxServiceM1Test, DisablePrivacySandboxPromptPolicy) {
  // Disable the prompt via policy and check the returned prompt type is kNone.
  RunTestCase(
      TestState{{StateKey::kM1PromptDisabledByPolicy,
                 static_cast<int>(
                     PrivacySandboxService::PromptSuppressedReason::kPolicy)}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{
          {OutputKey::kPromptType, static_cast<int>(PromptType::kNone)}});
}

TEST_F(PrivacySandboxServiceM1Test, DisablePrivacySandboxTopicsPolicy) {
  // Disable the Topics api via policy and check the returned prompt type is
  // kNone and topics is not allowed.
  RunTestCase(
      TestState{{StateKey::kM1TopicsDisabledByPolicy, true}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{{OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)},
                 {OutputKey::kIsTopicsAllowed, false}});
}

TEST_F(PrivacySandboxServiceM1Test, DisablePrivacySandboxFledgePolicy) {
  // Disable the Fledge api via policy and check the returned prompt type is
  // kNone and fledge is not allowed.
  RunTestCase(
      TestState{{StateKey::kM1FledgeDisabledByPolicy, true}},
      TestInput{
          {InputKey::kForceChromeBuild, true},
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kFledgeAuctionPartyOrigin,
           url::Origin::Create(GURL("https://embedded.com"))}},
      TestOutput{{OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)},
                 {OutputKey::kIsFledgeAllowed, false}});
}

TEST_F(PrivacySandboxServiceM1Test, DisablePrivacySandboxAdMeasurementPolicy) {
  // Disable the ad measurement api via policy and check the returned prompt
  // type is kNone and the api is not allowed.
  RunTestCase(
      TestState{{StateKey::kM1AdMesaurementDisabledByPolicy, true}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kAdMeasurementReportingOrigin,
           url::Origin::Create(GURL("https://embedded.com"))},
          {InputKey::kForceChromeBuild, true}},
      TestOutput{{OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)},
                 {OutputKey::kIsAttributionReportingAllowed, false}});
}

// TODO(crbug.com/1428506): consider parameterizing other tests for the various
// feature flags, particularly `kPrivacySandboxSettings4RestrictedNotice`.
class PrivacySandboxServiceM1RestrictedNoticePromptTest
    : public PrivacySandboxServiceM1PromptTest {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsSubjectToM1NoticeRestrictedResponse(
        /*is_subject_to_restricted_notice=*/true);
    return mock_delegate;
  }
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{"consent-required", "false"},
         {"notice-required", "true"},
         {privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.name,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest, RestrictedNotice) {
  // Ensure that kM1NoticeRestricted is returned when configured to do so.
  RunTestCase(TestState{{StateKey::kM1PromptSuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {StateKey::kTrialsNoticeDisplayed, false}},
              TestInput{{InputKey::kForceChromeBuild, true}},
              TestOutput{{OutputKey::kPromptType,
                          static_cast<int>(PromptType::kM1NoticeRestricted)},
                         {OutputKey::kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest,
       RestrictedNoticeAlreadyAcknowledged) {
  // If the user already acknowledged the notice, don't show it, or the ROW
  // notice, again.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kTrialsNoticeDisplayed, false},
                {StateKey::kM1RestrictedNoticeAcknowledged, true}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{{OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest,
       ROWNoticeAlreadyAcknowledged) {
  // If the user already acknowledged a different notice, don't show it again.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kTrialsNoticeDisplayed, false},
                {StateKey::kM1RowNoticeAcknowledged, true}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{{OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest,
       EEANoticeAlreadyAcknowledged) {
  // If the user already acknowledged a different notice, don't show the
  // restricted notice again. Ensure the existing suppression reason is
  // respected.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kTrialsNoticeDisplayed, false},
                {StateKey::kM1ConsentDecisionMade, true},
                {StateKey::kM1EEANoticeAcknowledged, true}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{
          {OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
          {OutputKey::kM1PromptSuppressedReason,
           static_cast<int>(
               PromptSuppressedReason::kEEAFlowCompletedBeforeRowMigration)}});
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest,
       RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::kNone));

  base::test::ScopedFeatureList feature_list_notice_required;
  std::map<std::string, std::string> notice_required_feature_param = {
      {std::string(
           privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName),
       "false"},
      {std::string(privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName),
       "true"}};
  feature_list_notice_required.InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4, notice_required_feature_param);

  // Notice flow not completed.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                      false);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxService::PromptStartupState::
                           kRestrictedNoticePromptWaiting),
      /*expected_count=*/1);

  // Notice flow completed.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                      true);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxService::PromptStartupState::
                           kRestrictedNoticeFlowCompleted),
      /*expected_count=*/1);

  // ROW flow completed, which implies no restricted prompt.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                      false);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged, true);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(
          PrivacySandboxService::PromptStartupState::
              kRestrictedNoticeNotShownDueToFullNoticeAcknowledged),
      /*expected_count=*/1);

  // EAA flow completed, which implies no restricted prompt.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                      false);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged, true);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(
          PrivacySandboxService::PromptStartupState::
              kRestrictedNoticeNotShownDueToFullNoticeAcknowledged),
      // One when the ROW notice acknowledged pref was set, plus the latest
      // call.
      /*expected_count=*/2);
}

class PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyUnrestricted
    : public PrivacySandboxServiceM1RestrictedNoticePromptTest {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsSubjectToM1NoticeRestrictedResponse(
        /*is_subject_to_restricted_notice=*/true);
    mock_delegate->SetUpIsPrivacySandboxCurrentlyUnrestrictedResponse(
        /*is_unrestricted=*/true);
    return mock_delegate;
  }
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{"consent-required", "false"},
         {"notice-required", "true"},
         {privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.name,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyUnrestricted,
       RecordPrivacySandbox4StartupMetrics_GraduationFlow) {
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::kNone));

  // Restricted Notice flow NOT completed
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                        false);
    // User was reported restricted
    prefs()->SetBoolean(prefs::kPrivacySandboxM1Restricted, true);

    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram,
        static_cast<int>(
            PrivacySandboxService::PromptStartupState::
                kWaitingForGraduationRestrictedNoticeFlowNotCompleted),
        /*expected_count=*/1);
  }

  // Restricted Notice flow completed
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                        true);

    // User was reported restricted
    prefs()->SetBoolean(prefs::kPrivacySandboxM1Restricted, true);

    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram,
        static_cast<int>(
            PrivacySandboxService::PromptStartupState::
                kWaitingForGraduationRestrictedNoticeFlowCompleted),
        /*expected_count=*/1);
  }
}

TEST_F(
    PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyUnrestricted,
    RecordPrivacySandbox4StartupMetrics_GraduationFlowWhenNoticeShownToGuardian) {
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  base::HistogramTester histogram_tester;

  // User was reported restricted
  prefs()->SetBoolean(prefs::kPrivacySandboxM1Restricted, true);

  // Prompt is suppressed because direct notice was shown to guardian
  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::
                           kNoticeShownToGuardian));

  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(
          PrivacySandboxService::PromptStartupState::
              kWaitingForGraduationRestrictedNoticeFlowNotCompleted),
      /*expected_count=*/1);
}

class PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyRestricted
    : public PrivacySandboxServiceM1RestrictedNoticePromptTest {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsSubjectToM1NoticeRestrictedResponse(
        /*is_subject_to_restricted_notice=*/true);
    mock_delegate->SetUpIsPrivacySandboxCurrentlyUnrestrictedResponse(
        /*is_unrestricted=*/false);
    return mock_delegate;
  }
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{"consent-required", "false"},
         {"notice-required", "true"},
         {privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.name,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyRestricted,
       RecordPrivacySandbox4StartupMetrics_GraduationFlow) {
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PrivacySandboxService::PromptSuppressedReason::kNone));

  // Restricted Notice flow completed
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                        true);
    // User was reported restricted
    prefs()->SetBoolean(prefs::kPrivacySandboxM1Restricted, true);

    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram,
        static_cast<int>(PrivacySandboxService::PromptStartupState::
                             kRestrictedNoticeFlowCompleted),
        /*expected_count=*/1);
  }

  // Restricted Notice flow NOT completed
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                        false);
    // User was reported restricted
    prefs()->SetBoolean(prefs::kPrivacySandboxM1Restricted, true);

    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram,
        static_cast<int>(PrivacySandboxService::PromptStartupState::
                             kRestrictedNoticePromptWaiting),
        /*expected_count=*/1);
  }
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest,
       RestrictedNoticeAcknowledged) {
  // Ensure that Ad measurement pref is not re-enabled if user disabled it
  // after acknowledging the restricted notice.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kM1RestrictedNoticeAcknowledged, true},
                {StateKey::kM1AdMeasurementEnabledUserPrefValue, false}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{{OutputKey::kPromptType,
                  static_cast<int>(PrivacySandboxService::PromptType::kNone)},
                 {OutputKey::kM1AdMeasurementEnabled, false},
                 {OutputKey::kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)}});
}

class PrivacySandboxServiceM1RestrictedNoticeShownToGuardianTest
    : public PrivacySandboxServiceM1PromptTest {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/true);
    return mock_delegate;
  }
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{"consent-required", "false"},
         {"notice-required", "true"},
         {privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.name,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticeShownToGuardianTest,
       NotSubjectToNoticeButIsRestricted) {
  // Ensure that kNoticeShownToGuardian, with no prompt, is returned in the
  // event that the user is not subject to the m1 notice restricted prompt.
  // Ensure measurements API is enabled for these users.
  RunTestCase(
      TestState{{StateKey::kM1PromptSuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {StateKey::kTrialsNoticeDisplayed, false}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{
          {OutputKey::kPromptType, static_cast<int>(PromptType::kNone)},
          {OutputKey::kM1PromptSuppressedReason,
           static_cast<int>(PromptSuppressedReason::kNoticeShownToGuardian)},
          {OutputKey::kM1AdMeasurementEnabled, true}});
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticeShownToGuardianTest,
       NotSubjectToNoticeButIsRestrictedWithAdMeasurementDisabled) {
  // Ensure that Ad measurement pref is not re-enabled if user disabled it
  // after the notice was suppressed due to kNoticeShownToGuardian.
  RunTestCase(
      TestState{
          {StateKey::kM1PromptSuppressedReason,
           static_cast<int>(PromptSuppressedReason::kNoticeShownToGuardian)},
          {StateKey::kM1AdMeasurementEnabledUserPrefValue, false}},
      TestInput{{InputKey::kForceChromeBuild, true}},
      TestOutput{
          {OutputKey::kPromptType,
           static_cast<int>(PrivacySandboxService::PromptType::kNone)},
          {OutputKey::kM1AdMeasurementEnabled, false},
          {OutputKey::kM1PromptSuppressedReason,
           static_cast<int>(PromptSuppressedReason::kNoticeShownToGuardian)}});
}

class PrivacySandboxServiceM1RestrictedNoticeEnabledNoRestrictionsTest
    : public PrivacySandboxServiceM1PromptTest {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/false);
    mock_delegate->SetUpIsSubjectToM1NoticeRestrictedResponse(
        /*is_subject_to_restricted_notice=*/false);
    return mock_delegate;
  }
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{"consent-required", "false"},
         {"notice-required", "true"},
         {privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.name,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticeEnabledNoRestrictionsTest,
       VerifyPromptType) {
  // The restricted notice feature is enabled, but the account is not subject to
  // the restrictions, and the privacy sandbox is not otherwise restricted. The
  // ROW notice is still applicable, however.
  RunTestCase(TestState{{StateKey::kM1PromptSuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {StateKey::kTrialsNoticeDisplayed, false}},
              TestInput{{InputKey::kForceChromeBuild, true}},
              TestOutput{{OutputKey::kPromptType,
                          static_cast<int>(PromptType::kM1NoticeROW)},
                         {OutputKey::kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}
