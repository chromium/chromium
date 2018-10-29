// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_context_base.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

const char* const kPermissionsKillSwitchFieldStudy =
    PermissionContextBase::kPermissionsKillSwitchFieldStudy;
const char* const kPermissionsKillSwitchBlockedValue =
    PermissionContextBase::kPermissionsKillSwitchBlockedValue;
const char kPermissionsKillSwitchTestGroup[] = "TestGroup";
const char* const kPromptGroupName = kPermissionsKillSwitchTestGroup;
const char kPromptTrialName[] = "PermissionPromptsUX";

class TestPermissionContext : public PermissionContextBase {
 public:
  TestPermissionContext(Profile* profile,
                        const ContentSettingsType content_settings_type)
      : PermissionContextBase(profile,
                              content_settings_type,
                              blink::mojom::FeaturePolicyFeature::kNotFound),
        tab_context_updated_(false) {}

  ~TestPermissionContext() override {}

  const std::vector<ContentSetting>& decisions() const { return decisions_; }

  bool tab_context_updated() const { return tab_context_updated_; }

  // Once a decision for the requested permission has been made, run the
  // callback.
  void TrackPermissionDecision(ContentSetting content_setting) {
    decisions_.push_back(content_setting);
    // Null check required here as the quit_closure_ can also be run and reset
    // first from within DecidePermission.
    if (quit_closure_) {
      quit_closure_.Run();
      quit_closure_.Reset();
    }
  }

  ContentSetting GetContentSettingFromMap(const GURL& url_a,
                                          const GURL& url_b) {
    auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
    return map->GetContentSetting(url_a.GetOrigin(), url_b.GetOrigin(),
                                  content_settings_type(), std::string());
  }

  void RequestPermission(content::WebContents* web_contents,
                         const PermissionRequestID& id,
                         const GURL& requesting_frame,
                         bool user_gesture,
                         const BrowserPermissionCallback& callback) override {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    PermissionContextBase::RequestPermission(web_contents, id, requesting_frame,
                                             true /* user_gesture */, callback);
    run_loop.Run();
  }

  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin,
                        bool user_gesture,
                        const BrowserPermissionCallback& callback) override {
    PermissionContextBase::DecidePermission(web_contents, id, requesting_origin,
                                            embedding_origin, user_gesture,
                                            callback);
    if (respond_permission_) {
      respond_permission_.Run();
      respond_permission_.Reset();
    } else {
      // Stop the run loop from spinning indefinitely if no response callback
      // has been set, as is the case with TestParallelRequests.
      quit_closure_.Run();
      quit_closure_.Reset();
    }
  }

  // Set the callback to run if the permission is being responded to in the
  // test. This is left empty where no response is needed, such as in parallel
  // requests, invalid origin, and killswitch.
  void SetRespondPermissionCallback(base::Closure callback) {
    respond_permission_ = callback;
  }

 protected:
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        bool allowed) override {
    tab_context_updated_ = true;
  }

  bool IsRestrictedToSecureOrigins() const override { return false; }

 private:
  std::vector<ContentSetting> decisions_;
  bool tab_context_updated_;
  base::Closure quit_closure_;
  // Callback for responding to a permission once the request has been completed
  // (valid URL, kill switch disabled)
  base::Closure respond_permission_;
  DISALLOW_COPY_AND_ASSIGN(TestPermissionContext);
};

class TestKillSwitchPermissionContext : public TestPermissionContext {
 public:
  TestKillSwitchPermissionContext(
      Profile* profile,
      const ContentSettingsType content_settings_type)
      : TestPermissionContext(profile, content_settings_type),
        field_trial_list_(std::make_unique<base::FieldTrialList>(
            std::make_unique<base::MockEntropyProvider>())) {}

  void ResetFieldTrialList() {
    // Destroy the existing FieldTrialList before creating a new one to avoid
    // a DCHECK.
    field_trial_list_.reset();
    field_trial_list_ = std::make_unique<base::FieldTrialList>(
        std::make_unique<base::MockEntropyProvider>());
    variations::testing::ClearAllVariationParams();
  }

 private:
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(TestKillSwitchPermissionContext);
};

class PermissionContextBaseTests : public ChromeRenderViewHostTestHarness {
 protected:
  PermissionContextBaseTests() {}
  ~PermissionContextBaseTests() override {}

  // Accept or dismiss the permission prompt.
  void RespondToPermission(TestPermissionContext* context,
                           const PermissionRequestID& id,
                           const GURL& url,
                           ContentSetting response) {
    DCHECK(response == CONTENT_SETTING_ALLOW ||
           response == CONTENT_SETTING_BLOCK ||
           response == CONTENT_SETTING_ASK);
    using AutoResponseType = PermissionRequestManager::AutoResponseType;
    AutoResponseType decision = AutoResponseType::DISMISS;
    if (response == CONTENT_SETTING_ALLOW)
      decision = AutoResponseType::ACCEPT_ALL;
    else if (response == CONTENT_SETTING_BLOCK)
      decision = AutoResponseType::DENY_ALL;
    prompt_factory_->set_response_type(decision);
  }

  void TestAskAndDecide_TestContent(ContentSettingsType content_settings_type,
                                    ContentSetting decision) {
    TestPermissionContext permission_context(profile(), content_settings_type);
    GURL url("https://www.google.com");
    SetUpUrl(url);
    base::HistogramTester histograms;

    const PermissionRequestID id(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), -1);
    permission_context.SetRespondPermissionCallback(base::Bind(
        &PermissionContextBaseTests::RespondToPermission,
        base::Unretained(this), &permission_context, id, url, decision));
    permission_context.RequestPermission(
        web_contents(), id, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));
    ASSERT_EQ(1u, permission_context.decisions().size());
    EXPECT_EQ(decision, permission_context.decisions()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());

    std::string decision_string;
    if (decision == CONTENT_SETTING_ALLOW)
      decision_string = "Accepted";
    else if (decision == CONTENT_SETTING_BLOCK)
      decision_string = "Denied";
    else if (decision == CONTENT_SETTING_ASK)
      decision_string = "Dismissed";

    if (decision_string.size()) {
      histograms.ExpectUniqueSample(
          "Permissions.Prompt." + decision_string + ".PriorDismissCount." +
              PermissionUtil::GetPermissionString(content_settings_type),
          0, 1);
      histograms.ExpectUniqueSample(
          "Permissions.Prompt." + decision_string + ".PriorIgnoreCount." +
              PermissionUtil::GetPermissionString(content_settings_type),
          0, 1);
    }

    EXPECT_EQ(decision, permission_context.GetContentSettingFromMap(url, url));

    histograms.ExpectUniqueSample(
        "Permissions.AutoBlocker.EmbargoPromptSuppression",
        static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), 1);
    histograms.ExpectUniqueSample(
        "Permissions.AutoBlocker.EmbargoStatus",
        static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), 1);
  }

  void DismissMultipleTimesAndExpectBlock(
      const GURL& url,
      ContentSettingsType content_settings_type,
      uint32_t iterations) {
    base::HistogramTester histograms;

    // Dismiss |iterations| times. The final dismiss should change the decision
    // from dismiss to block, and hence change the persisted content setting.
    for (uint32_t i = 0; i < iterations; ++i) {
      TestPermissionContext permission_context(profile(),
                                               content_settings_type);
      const PermissionRequestID id(
          web_contents()->GetMainFrame()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(), i);

      permission_context.SetRespondPermissionCallback(
          base::Bind(&PermissionContextBaseTests::RespondToPermission,
                     base::Unretained(this), &permission_context, id, url,
                     CONTENT_SETTING_ASK));

      permission_context.RequestPermission(
          web_contents(), id, url, true /* user_gesture */,
          base::Bind(&TestPermissionContext::TrackPermissionDecision,
                     base::Unretained(&permission_context)));
      histograms.ExpectTotalCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount." +
              PermissionUtil::GetPermissionString(content_settings_type),
          i + 1);
      histograms.ExpectBucketCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount." +
              PermissionUtil::GetPermissionString(content_settings_type),
          i, 1);

      histograms.ExpectTotalCount("Permissions.AutoBlocker.EmbargoStatus",
                                  i + 1);

      PermissionResult result = permission_context.GetPermissionStatus(
          nullptr /* render_frame_host */, url, url);

      histograms.ExpectUniqueSample(
          "Permissions.AutoBlocker.EmbargoPromptSuppression",
          static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      if (i < 2) {
        EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
        EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
        histograms.ExpectUniqueSample(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      } else {
        EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
        EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
        histograms.ExpectBucketCount(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::REPEATED_DISMISSALS), 1);
      }

      ASSERT_EQ(1u, permission_context.decisions().size());
      EXPECT_EQ(CONTENT_SETTING_ASK, permission_context.decisions()[0]);
      EXPECT_TRUE(permission_context.tab_context_updated());
    }

    TestPermissionContext permission_context(profile(), content_settings_type);
    const PermissionRequestID id(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), -1);

    permission_context.SetRespondPermissionCallback(
        base::Bind(&PermissionContextBaseTests::RespondToPermission,
                   base::Unretained(this), &permission_context, id, url,
                   CONTENT_SETTING_ASK));

    permission_context.RequestPermission(
        web_contents(), id, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    PermissionResult result = permission_context.GetPermissionStatus(
        nullptr /* render_frame_host */, url, url);
    EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
    EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
    histograms.ExpectBucketCount(
        "Permissions.AutoBlocker.EmbargoPromptSuppression",
        static_cast<int>(PermissionEmbargoStatus::REPEATED_DISMISSALS), 1);
  }

  void TestBlockOnSeveralDismissals_TestContent() {
    GURL url("https://www.google.com");
    SetUpUrl(url);
    base::HistogramTester histograms;

    {
      // Ensure that > 3 dismissals behaves correctly when the
      // BlockPromptsIfDismissedOften feature is off.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndDisableFeature(
          features::kBlockPromptsIfDismissedOften);

      for (uint32_t i = 0; i < 4; ++i) {
        TestPermissionContext permission_context(
            profile(), CONTENT_SETTINGS_TYPE_GEOLOCATION);

        const PermissionRequestID id(
            web_contents()->GetMainFrame()->GetProcess()->GetID(),
            web_contents()->GetMainFrame()->GetRoutingID(), i);

        permission_context.SetRespondPermissionCallback(
            base::Bind(&PermissionContextBaseTests::RespondToPermission,
                       base::Unretained(this), &permission_context, id, url,
                       CONTENT_SETTING_ASK));
        permission_context.RequestPermission(
            web_contents(), id, url, true /* user_gesture */,
            base::Bind(&TestPermissionContext::TrackPermissionDecision,
                       base::Unretained(&permission_context)));
        histograms.ExpectTotalCount(
            "Permissions.Prompt.Dismissed.PriorDismissCount.Geolocation",
            i + 1);
        histograms.ExpectBucketCount(
            "Permissions.Prompt.Dismissed.PriorDismissCount.Geolocation", i, 1);
        histograms.ExpectUniqueSample(
            "Permissions.AutoBlocker.EmbargoPromptSuppression",
            static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
        histograms.ExpectUniqueSample(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);

        ASSERT_EQ(1u, permission_context.decisions().size());
        EXPECT_EQ(CONTENT_SETTING_ASK, permission_context.decisions()[0]);
        EXPECT_TRUE(permission_context.tab_context_updated());
        EXPECT_EQ(CONTENT_SETTING_ASK,
                  permission_context.GetContentSettingFromMap(url, url));
      }

      // Flush the dismissal counts.
      auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
      map->ClearSettingsForOneType(
          CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA);
    }

    EXPECT_TRUE(
        base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften));

    // Sanity check independence per permission type by checking two of them.
    DismissMultipleTimesAndExpectBlock(url, CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                       3);
    DismissMultipleTimesAndExpectBlock(url, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                       3);
  }

  void TestVariationBlockOnSeveralDismissals_TestContent() {
    GURL url("https://www.google.com");
    SetUpUrl(url);
    base::HistogramTester histograms;

    // Set up the custom parameter and custom value.
    base::FieldTrialList field_trials(nullptr);
    base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
        kPromptTrialName, kPromptGroupName);
    std::map<std::string, std::string> params;
    params[PermissionDecisionAutoBlocker::kPromptDismissCountKey] = "5";
    ASSERT_TRUE(variations::AssociateVariationParams(kPromptTrialName,
                                                     kPromptGroupName, params));

    std::unique_ptr<base::FeatureList> feature_list =
        std::make_unique<base::FeatureList>();
    feature_list->RegisterFieldTrialOverride(
        features::kBlockPromptsIfDismissedOften.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_EQ(base::FeatureList::GetFieldTrial(
                  features::kBlockPromptsIfDismissedOften),
              trial);

    {
      std::map<std::string, std::string> actual_params;
      EXPECT_TRUE(variations::GetVariationParamsByFeature(
          features::kBlockPromptsIfDismissedOften, &actual_params));
      EXPECT_EQ(params, actual_params);
    }

    for (uint32_t i = 0; i < 5; ++i) {
      TestPermissionContext permission_context(
          profile(), CONTENT_SETTINGS_TYPE_MIDI_SYSEX);

      const PermissionRequestID id(
          web_contents()->GetMainFrame()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(), i);
      permission_context.SetRespondPermissionCallback(
          base::Bind(&PermissionContextBaseTests::RespondToPermission,
                     base::Unretained(this), &permission_context, id, url,
                     CONTENT_SETTING_ASK));
      permission_context.RequestPermission(
          web_contents(), id, url, true /* user_gesture */,
          base::Bind(&TestPermissionContext::TrackPermissionDecision,
                     base::Unretained(&permission_context)));

      EXPECT_EQ(1u, permission_context.decisions().size());
      ASSERT_EQ(CONTENT_SETTING_ASK, permission_context.decisions()[0]);
      EXPECT_TRUE(permission_context.tab_context_updated());
      PermissionResult result = permission_context.GetPermissionStatus(
          nullptr /* render_frame_host */, url, url);

      histograms.ExpectTotalCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount.MidiSysEx", i + 1);
      histograms.ExpectBucketCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount.MidiSysEx", i, 1);
      histograms.ExpectUniqueSample(
          "Permissions.AutoBlocker.EmbargoPromptSuppression",
          static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      histograms.ExpectTotalCount("Permissions.AutoBlocker.EmbargoStatus",
                                  i + 1);
      if (i < 4) {
        EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
        EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
        histograms.ExpectUniqueSample(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      } else {
        EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
        EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
        histograms.ExpectBucketCount(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::REPEATED_DISMISSALS), 1);
      }
    }

    // Ensure that we finish in the block state.
    TestPermissionContext permission_context(profile(),
                                             CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
    PermissionResult result = permission_context.GetPermissionStatus(
        nullptr /* render_frame_host */, url, url);
    EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
    EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
    variations::testing::ClearAllVariationParams();
  }

  void TestRequestPermissionInvalidUrl(
      ContentSettingsType content_settings_type) {
    base::HistogramTester histograms;
    TestPermissionContext permission_context(profile(), content_settings_type);
    GURL url;
    ASSERT_FALSE(url.is_valid());
    controller().LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
                         std::string());

    const PermissionRequestID id(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), -1);
    permission_context.RequestPermission(
        web_contents(), id, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    ASSERT_EQ(1u, permission_context.decisions().size());
    EXPECT_EQ(CONTENT_SETTING_BLOCK, permission_context.decisions()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());
    EXPECT_EQ(CONTENT_SETTING_ASK,
              permission_context.GetContentSettingFromMap(url, url));
    histograms.ExpectTotalCount(
        "Permissions.AutoBlocker.EmbargoPromptSuppression", 0);
  }

  void TestGrantAndRevoke_TestContent(ContentSettingsType content_settings_type,
                                      ContentSetting expected_default) {
    TestPermissionContext permission_context(profile(), content_settings_type);
    GURL url("https://www.google.com");
    SetUpUrl(url);

    const PermissionRequestID id(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), -1);
    permission_context.SetRespondPermissionCallback(
        base::Bind(&PermissionContextBaseTests::RespondToPermission,
                   base::Unretained(this), &permission_context, id, url,
                   CONTENT_SETTING_ALLOW));

    permission_context.RequestPermission(
        web_contents(), id, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    ASSERT_EQ(1u, permission_context.decisions().size());
    EXPECT_EQ(CONTENT_SETTING_ALLOW, permission_context.decisions()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              permission_context.GetContentSettingFromMap(url, url));

    // Try to reset permission.
    permission_context.ResetPermission(url.GetOrigin(), url.GetOrigin());
    ContentSetting setting_after_reset =
        permission_context.GetContentSettingFromMap(url, url);
    ContentSetting default_setting =
        HostContentSettingsMapFactory::GetForProfile(profile())
            ->GetDefaultContentSetting(content_settings_type, nullptr);
    EXPECT_EQ(default_setting, setting_after_reset);
  }

  void TestGlobalPermissionsKillSwitch(
      ContentSettingsType content_settings_type) {
    TestKillSwitchPermissionContext permission_context(profile(),
                                                       content_settings_type);
    permission_context.ResetFieldTrialList();

    EXPECT_FALSE(permission_context.IsPermissionKillSwitchOn());
    std::map<std::string, std::string> params;
    params[PermissionUtil::GetPermissionString(content_settings_type)] =
        kPermissionsKillSwitchBlockedValue;
    variations::AssociateVariationParams(kPermissionsKillSwitchFieldStudy,
                                         kPermissionsKillSwitchTestGroup,
                                         params);
    base::FieldTrialList::CreateFieldTrial(kPermissionsKillSwitchFieldStudy,
                                           kPermissionsKillSwitchTestGroup);
    EXPECT_TRUE(permission_context.IsPermissionKillSwitchOn());
  }

  // Don't call this more than once in the same test, as it persists data to
  // HostContentSettingsMap.
  void TestParallelRequests(ContentSetting response) {
    TestPermissionContext permission_context(
        profile(), CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
    GURL url("http://www.google.com");
    SetUpUrl(url);

    const PermissionRequestID id0(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), 0);
    const PermissionRequestID id1(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), 1);

    // Request a permission without setting the callback to DecidePermission.
    permission_context.RequestPermission(
        web_contents(), id0, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    EXPECT_EQ(0u, permission_context.decisions().size());

    // Set the callback, and make a second permission request.
    permission_context.SetRespondPermissionCallback(base::Bind(
        &PermissionContextBaseTests::RespondToPermission,
        base::Unretained(this), &permission_context, id0, url, response));
    permission_context.RequestPermission(
        web_contents(), id1, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    ASSERT_EQ(2u, permission_context.decisions().size());
    EXPECT_EQ(response, permission_context.decisions()[0]);
    EXPECT_EQ(response, permission_context.decisions()[1]);
    EXPECT_TRUE(permission_context.tab_context_updated());

    EXPECT_EQ(response, permission_context.GetContentSettingFromMap(url, url));
  }

  void TestVirtualURL(const GURL& loaded_url,
                      const GURL& virtual_url,
                      const ContentSetting want_response,
                      const PermissionStatusSource& want_source) {
    TestPermissionContext permission_context(
        profile(), CONTENT_SETTINGS_TYPE_NOTIFICATIONS);

    NavigateAndCommit(loaded_url);
    web_contents()->GetController().GetVisibleEntry()->SetVirtualURL(
        virtual_url);

    PermissionResult result = permission_context.GetPermissionStatus(
        web_contents()->GetMainFrame(), virtual_url, virtual_url);
    EXPECT_EQ(result.content_setting, want_response);
    EXPECT_EQ(result.source, want_source);
  }

  void SetUpUrl(const GURL& url) {
    NavigateAndCommit(url);
    prompt_factory_->DocumentOnLoadCompletedInMainFrame();
  }

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PermissionRequestManager::CreateForWebContents(web_contents());
    PermissionRequestManager* manager =
        PermissionRequestManager::FromWebContents(web_contents());
    prompt_factory_.reset(new MockPermissionPromptFactory(manager));
  }

  void TearDown() override {
    prompt_factory_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionContextBaseTests);
};

// Simulates clicking Accept. The permission should be granted and
// saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndGrant) {
  TestAskAndDecide_TestContent(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                               CONTENT_SETTING_ALLOW);
}

// Simulates clicking Block. The permission should be denied and
// saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndBlock) {
  TestAskAndDecide_TestContent(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                               CONTENT_SETTING_BLOCK);
}

// Simulates clicking Dismiss (X) in the prompt.
// The permission should be denied but not saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndDismiss) {
  TestAskAndDecide_TestContent(CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                               CONTENT_SETTING_ASK);
}

// Simulates clicking Dismiss (X) in the prompt with the block on too
// many dismissals feature active. The permission should be blocked after
// several dismissals.
TEST_F(PermissionContextBaseTests, TestDismissUntilBlocked) {
  TestBlockOnSeveralDismissals_TestContent();
}

// Test setting a custom number of dismissals before block via variations.
TEST_F(PermissionContextBaseTests, TestDismissVariations) {
  TestVariationBlockOnSeveralDismissals_TestContent();
}

// Simulates non-valid requesting URL.
// The permission should be denied but not saved for future use.
TEST_F(PermissionContextBaseTests, TestNonValidRequestingUrl) {
  TestRequestPermissionInvalidUrl(CONTENT_SETTINGS_TYPE_GEOLOCATION);
  TestRequestPermissionInvalidUrl(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  TestRequestPermissionInvalidUrl(CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
#if defined(OS_CHROMEOS)
  TestRequestPermissionInvalidUrl(
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
#endif
}

// Simulates granting and revoking of permissions.
TEST_F(PermissionContextBaseTests, TestGrantAndRevoke) {
  TestGrantAndRevoke_TestContent(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                 CONTENT_SETTING_ASK);
  TestGrantAndRevoke_TestContent(CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                                 CONTENT_SETTING_ASK);
#if defined(OS_ANDROID)
  TestGrantAndRevoke_TestContent(
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, CONTENT_SETTING_ASK);
  // TODO(timvolodine): currently no test for
  // CONTENT_SETTINGS_TYPE_NOTIFICATIONS because notification permissions work
  // differently with infobars as compared to bubbles (crbug.com/453784).
#else
  TestGrantAndRevoke_TestContent(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                 CONTENT_SETTING_ASK);
#endif
}

// Tests the global kill switch by enabling/disabling the Field Trials.
TEST_F(PermissionContextBaseTests, TestGlobalKillSwitch) {
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_GEOLOCATION);
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_DURABLE_STORAGE);
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  TestGlobalPermissionsKillSwitch(
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
#endif
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsAllowed) {
  TestParallelRequests(CONTENT_SETTING_ALLOW);
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsBlocked) {
  TestParallelRequests(CONTENT_SETTING_BLOCK);
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsDismissed) {
  TestParallelRequests(CONTENT_SETTING_ASK);
}

TEST_F(PermissionContextBaseTests, TestVirtualURLDifferentOrigin) {
  TestVirtualURL(GURL("http://www.google.com"), GURL("http://foo.com"),
                 CONTENT_SETTING_BLOCK,
                 PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN);
}

TEST_F(PermissionContextBaseTests, TestVirtualURLNotHTTP) {
  TestVirtualURL(GURL("chrome://foo"), GURL("chrome://newtab"),
                 CONTENT_SETTING_ASK, PermissionStatusSource::UNSPECIFIED);
}

TEST_F(PermissionContextBaseTests, TestVirtualURLSameOrigin) {
  TestVirtualURL(GURL("http://www.google.com"),
                 GURL("http://www.google.com/foo"), CONTENT_SETTING_ASK,
                 PermissionStatusSource::UNSPECIFIED);
}
