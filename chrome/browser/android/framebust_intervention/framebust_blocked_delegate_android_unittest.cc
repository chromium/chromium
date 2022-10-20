// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/framebust_intervention/framebust_blocked_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace blocked_content {
namespace {
constexpr char kPageUrl[] = "http://example_page.test";
}  // namespace

class FramebustBlockedMessageDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  FramebustBlockedMessageDelegateTest() = default;
  ~FramebustBlockedMessageDelegateTest() override;

  // ChromeRenderViewHostTestHarness:
  void SetUp() override;
  void TearDown() override;

  HostContentSettingsMap* settings_map() { return settings_map_.get(); }

  bool EnqueueMessage(GURL url);

  messages::MessageWrapper* GetMessageWrapper();
  void TriggerMessageDismissedCallback(messages::DismissReason dismiss_reason);
  void TriggerActionClick();
  void TriggerSecondaryActionClick();

  FramebustBlockedMessageDelegate* GetDelegate() {
    return framebust_blocked_message_delegate_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  raw_ptr<FramebustBlockedMessageDelegate> framebust_blocked_message_delegate_;
};

FramebustBlockedMessageDelegateTest::~FramebustBlockedMessageDelegateTest() {
  settings_map_->ShutdownOnUIThread();
}

void FramebustBlockedMessageDelegateTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();

  // Make sure the SafeBrowsingTriggeredPopupBlocker is not created.
  feature_list_.InitAndDisableFeature(kAbusiveExperienceEnforce);

  HostContentSettingsMap::RegisterProfilePrefs(pref_service_.registry());
  settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
      &pref_service_, false /* is_off_the_record */,
      false /* store_last_modified */, false /* restore_session*/,
      false /* should_record_metrics */);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<
          content_settings::TestPageSpecificContentSettingsDelegate>(
          /*prefs=*/nullptr, settings_map_.get()));

  PopupBlockerTabHelper::CreateForWebContents(web_contents());

  FramebustBlockedMessageDelegate::CreateForWebContents(web_contents());
  framebust_blocked_message_delegate_ =
      FramebustBlockedMessageDelegate::FromWebContents(web_contents());
  NavigateAndCommit(GURL(kPageUrl));
  message_dispatcher_bridge_.SetMessagesEnabledForEmbedder(true);
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void FramebustBlockedMessageDelegateTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

messages::MessageWrapper*
FramebustBlockedMessageDelegateTest::GetMessageWrapper() {
  return framebust_blocked_message_delegate_->message_for_testing();
}

bool FramebustBlockedMessageDelegateTest::EnqueueMessage(GURL url) {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(testing::Return(true));
  auto intervention_outcome =
      [](FramebustBlockedMessageDelegate::InterventionOutcome outcome) {};
  return GetDelegate()->ShowMessage(url, settings_map(),
                                    base::BindOnce(intervention_outcome));
}

void FramebustBlockedMessageDelegateTest::TriggerActionClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
}

void FramebustBlockedMessageDelegateTest::TriggerSecondaryActionClick() {
  GetMessageWrapper()->HandleSecondaryActionClick(
      base::android::AttachCurrentThread());
}

void FramebustBlockedMessageDelegateTest::TriggerMessageDismissedCallback(
    messages::DismissReason dismiss_reason) {
  GetMessageWrapper()->HandleDismissCallback(
      base::android::AttachCurrentThread(), static_cast<int>(dismiss_reason));
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that message properties (title, description, icon, button text) are
// set correctly.
TEST_F(FramebustBlockedMessageDelegateTest, MessagePropertyValues) {
  EnqueueMessage(GURL("a.test"));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_REDIRECT_BLOCKED_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ALWAYS_ALLOW_REDIRECTS),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(
      url_formatter::FormatUrlForSecurityDisplay(
          GURL("a.test"), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
      GetMessageWrapper()->GetDescription());

  // The description should be updated after ShowMessage is called with a
  // new blocked URL.
  // #EnqueueMessage ensure message is enqueued only once.
  GetDelegate()->ShowMessage(GURL("b.test"), settings_map(),
                             base::NullCallback());
  EXPECT_EQ(
      url_formatter::FormatUrlForSecurityDisplay(
          GURL("b.test"), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
      GetMessageWrapper()->GetDescription());
  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

// Tests that the settings are properly updated when a user decides to always
// allow redirects on a page.
TEST_F(FramebustBlockedMessageDelegateTest, OverrideIntervention) {
  EXPECT_EQ(settings_map()->GetContentSetting(GURL(kPageUrl), GURL(kPageUrl),
                                              ContentSettingsType::POPUPS),
            CONTENT_SETTING_BLOCK);
  bool result = EnqueueMessage(GURL("a.test"));
  EXPECT_TRUE(result);
  TriggerActionClick();
  EXPECT_EQ(settings_map()->GetContentSetting(GURL(kPageUrl), GURL(kPageUrl),
                                              ContentSettingsType::POPUPS),
            CONTENT_SETTING_ALLOW);
}

// Tests that the settings are not updated when a user dismisses a redirect
// blocked message.
TEST_F(FramebustBlockedMessageDelegateTest, AcceptIntervention) {
  EXPECT_EQ(settings_map()->GetContentSetting(GURL(kPageUrl), GURL(kPageUrl),
                                              ContentSettingsType::POPUPS),
            CONTENT_SETTING_BLOCK);
  bool result = EnqueueMessage(GURL("a.test"));
  EXPECT_TRUE(result);
  TriggerMessageDismissedCallback(messages::DismissReason::GESTURE);
  EXPECT_EQ(settings_map()->GetContentSetting(GURL(kPageUrl), GURL(kPageUrl),
                                              ContentSettingsType::POPUPS),
            CONTENT_SETTING_BLOCK);
}

// Tests that the settings are not updated when a user allows a redirect
// once.
TEST_F(FramebustBlockedMessageDelegateTest, AllowOnce) {
  EXPECT_EQ(settings_map()->GetContentSetting(GURL(kPageUrl), GURL(kPageUrl),
                                              ContentSettingsType::POPUPS),
            CONTENT_SETTING_BLOCK);
  bool result = EnqueueMessage(GURL("a.test"));
  EXPECT_TRUE(result);
  TriggerSecondaryActionClick();
  EXPECT_EQ(settings_map()->GetContentSetting(GURL(kPageUrl), GURL(kPageUrl),
                                              ContentSettingsType::POPUPS),
            CONTENT_SETTING_BLOCK);
}

}  // namespace blocked_content
