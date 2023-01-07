// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using webrtc_event_logging::WebRtcEventLogManager;

namespace {
constexpr size_t kWebAppId = 42;
}  // namespace

class WebRtcEventLogCollectionAllowedPolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<policy::PolicyTest::BooleanPolicy> {
 public:
  ~WebRtcEventLogCollectionAllowedPolicyTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;

    const BooleanPolicy policy = GetParam();
    if (policy == BooleanPolicy::kFalse || policy == BooleanPolicy::kTrue) {
      const bool policy_bool = (policy == BooleanPolicy::kTrue);
      policies.Set(policy::key::kWebRtcEventLogCollectionAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                   base::Value(policy_bool), nullptr);
    }

    provider_.UpdateChromePolicy(policies);
  }

  const PrefService::Preference* GetPreference() const {
    auto* service = user_prefs::UserPrefs::Get(browser()->profile());
    return service->FindPreference(prefs::kWebRtcEventLogCollectionAllowed);
  }

  base::OnceCallback<void(bool)> BlockingBoolExpectingReply(
      base::RunLoop* run_loop,
      bool expected_value) {
    return base::BindOnce(
        [](base::RunLoop* run_loop, bool expected_value, bool value) {
          EXPECT_EQ(expected_value, value);
          run_loop->Quit();
        },
        run_loop, expected_value);
  }

  // The "extras" in question are the ID and error (only one of which may
  // be non-null), which this test ignores (tested elsewhere).
  base::OnceCallback<void(bool, const std::string&, const std::string&)>
  BlockingBoolExpectingReplyWithExtras(base::RunLoop* run_loop,
                                       bool expected_value) {
    return base::BindOnce(
        [](base::RunLoop* run_loop, bool expected_value, bool value,
           const std::string& ignored_log_id,
           const std::string& ignored_error) {
          EXPECT_EQ(expected_value, value);
          run_loop->Quit();
        },
        run_loop, expected_value);
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcEventLogCollectionAllowedPolicyTest, RunTest) {
  const PrefService::Preference* const pref = GetPreference();
  const bool remote_logging_allowed = (GetParam() == BooleanPolicy::kTrue);
  ASSERT_EQ(pref->GetValue()->GetBool(), remote_logging_allowed);

  auto* webrtc_event_log_manager = WebRtcEventLogManager::GetInstance();
  ASSERT_TRUE(webrtc_event_log_manager);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::GlobalRenderFrameHostId frame_id =
      web_contents->GetPrimaryMainFrame()->GetGlobalId();

  constexpr int kLid = 123;
  const std::string kSessionId = "id";

  {
    base::RunLoop run_loop;
    webrtc_event_log_manager->OnPeerConnectionAdded(
        frame_id, kLid, BlockingBoolExpectingReply(&run_loop, true));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    webrtc_event_log_manager->OnPeerConnectionSessionIdSet(
        frame_id, kLid, kSessionId,
        BlockingBoolExpectingReply(&run_loop, true));
    run_loop.Run();
  }

  {
    constexpr size_t kMaxFileSizeBytes = 1000 * 1000;
    constexpr int kOutputPeriodMs = 1000;

    base::RunLoop run_loop;

    // Test focus - remote-bound logging allowed if and only if the policy
    // is configured to allow it.
    webrtc_event_log_manager->StartRemoteLogging(
        frame_id.child_id, kSessionId, kMaxFileSizeBytes, kOutputPeriodMs,
        kWebAppId,
        BlockingBoolExpectingReplyWithExtras(&run_loop,
                                             remote_logging_allowed));
    run_loop.Run();
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebRtcEventLogCollectionAllowedPolicyTest,
    ::testing::Values(policy::PolicyTest::BooleanPolicy::kNotConfigured,
                      policy::PolicyTest::BooleanPolicy::kFalse,
                      policy::PolicyTest::BooleanPolicy::kTrue));
