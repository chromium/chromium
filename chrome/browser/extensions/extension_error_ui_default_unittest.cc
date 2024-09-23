// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_ui_default.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/mock_extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class TestErrorUIDelegate : public extensions::ExtensionErrorUI::Delegate {
 public:
  // extensions::ExtensionErrorUI::Delegate:
  content::BrowserContext* GetContext() override { return &profile_; }
  const extensions::ExtensionSet& GetBlocklistedExtensions() override {
    return forbidden_;
  }
  void OnAlertDetails() override {}
  void OnAlertAccept() override {}
  void OnAlertClosed() override {}

  void InsertForbidden(scoped_refptr<const extensions::Extension> ext) {
    forbidden_.Insert(ext);
  }

 private:
  content::BrowserTaskEnvironment environment_;
  TestingProfile profile_;
  extensions::ExtensionSet forbidden_;
};

class ManagementPolicyMock : public extensions::ManagementPolicy::Provider {
 public:
  ManagementPolicyMock(const extensions::Extension* extension, bool may_load)
      : extension_(extension), may_load_(may_load) {}

  std::string GetDebugPolicyProviderName() const override {
    return "ManagementPolicyMock";
  }

  bool UserMayLoad(const extensions::Extension* extension,
                   std::u16string* error) const override {
    EXPECT_EQ(extension_, extension);
    return may_load_;
  }

 private:
  raw_ptr<const extensions::Extension> extension_;
  bool may_load_;
};

}  // namespace

TEST(ExtensionErrorUIDefaultTest, BubbleTitleAndMessageMentionsExtension) {
  TestErrorUIDelegate delegate;

  delegate.InsertForbidden(extensions::ExtensionBuilder("Bar").Build());
  delegate.InsertForbidden(extensions::ExtensionBuilder("Baz").Build());

  extensions::ExtensionErrorUIDefault ui(&delegate);
  GlobalErrorWithStandardBubble* bubble = ui.GetErrorForTesting();

  std::u16string title = bubble->GetBubbleViewTitle();
  EXPECT_THAT(title,
              l10n_util::GetPluralStringFUTF16(IDS_EXTENSION_ALERT_TITLE, 2));

  std::vector<std::u16string> messages = bubble->GetBubbleViewMessages();

  EXPECT_THAT(messages,
              testing::ElementsAre(
                  l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_ALERT_ITEM_BLOCKLISTED_MALWARE_TITLE),
                  l10n_util::GetStringFUTF16(
                      IDS_BLOCKLISTED_EXTENSIONS_ALERT_ITEM, u"Bar"),
                  l10n_util::GetStringFUTF16(
                      IDS_BLOCKLISTED_EXTENSIONS_ALERT_ITEM, u"Baz")));
}

// Test that unusually long extension names in the bubble message are truncated.
TEST(ExtensionErrorUIDefaultTest, BubbleMessageWithLongNameExtension) {
  std::string long_name_a(128, 'a');
  std::string long_name_b(128, 'b');

  TestErrorUIDelegate delegate;
  delegate.InsertForbidden(extensions::ExtensionBuilder(long_name_a).Build());
  delegate.InsertForbidden(extensions::ExtensionBuilder(long_name_b).Build());

  extensions::ExtensionErrorUIDefault ui(&delegate);
  GlobalErrorWithStandardBubble* bubble = ui.GetErrorForTesting();

  std::u16string title = bubble->GetBubbleViewTitle();
  EXPECT_THAT(title,
              l10n_util::GetPluralStringFUTF16(IDS_EXTENSION_ALERT_TITLE, 2));

  std::vector<std::u16string> messages = bubble->GetBubbleViewMessages();

  // Expected values of extension names in the returned messages.
  std::u16string truncated_name_a =
      extensions::util::GetFixupExtensionNameForUIDisplay(long_name_a);
  std::u16string truncated_name_b =
      extensions::util::GetFixupExtensionNameForUIDisplay(long_name_b);

  ASSERT_LT(truncated_name_a.size(), long_name_a.size());
  ASSERT_LT(truncated_name_b.size(), long_name_b.size());

  EXPECT_THAT(
      messages,
      testing::ElementsAre(
          l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_ALERT_ITEM_BLOCKLISTED_MALWARE_TITLE),
          l10n_util::GetStringFUTF16(IDS_BLOCKLISTED_EXTENSIONS_ALERT_ITEM,
                                     truncated_name_a),
          l10n_util::GetStringFUTF16(IDS_BLOCKLISTED_EXTENSIONS_ALERT_ITEM,
                                     truncated_name_b)));
}

TEST(ExtensionErrorUIDefaultTest, BubbleTitleAndMessageMentionsApp) {
  TestErrorUIDelegate delegate;

  delegate.InsertForbidden(
      extensions::ExtensionBuilder(
          "Bar", extensions::ExtensionBuilder::Type::PLATFORM_APP)
          .Build());

  extensions::ExtensionErrorUIDefault ui(&delegate);
  GlobalErrorWithStandardBubble* bubble = ui.GetErrorForTesting();

  std::u16string title = bubble->GetBubbleViewTitle();
  EXPECT_THAT(title, l10n_util::GetPluralStringFUTF16(IDS_APP_ALERT_TITLE, 1));

  std::vector<std::u16string> messages = bubble->GetBubbleViewMessages();

  EXPECT_THAT(messages,
              testing::ElementsAre(l10n_util::GetStringFUTF16(
                  IDS_EXTENSION_ALERT_ITEM_BLOCKLISTED_MALWARE, u"Bar")));
}

TEST(ExtensionErrorUIDefaultTest, BubbleMessageMentionsMalware) {
  TestErrorUIDelegate delegate;
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(
          "Bar", extensions::ExtensionBuilder::Type::PLATFORM_APP)
          .Build();
  extensions::blocklist_prefs::AddOmahaBlocklistState(
      extension->id(), extensions::BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extensions::ExtensionPrefs::Get(delegate.GetContext()));
  delegate.InsertForbidden(extension);

  extensions::ExtensionErrorUIDefault ui(&delegate);
  GlobalErrorWithStandardBubble* bubble = ui.GetErrorForTesting();

  std::u16string title = bubble->GetBubbleViewTitle();
  EXPECT_THAT(title, l10n_util::GetPluralStringFUTF16(IDS_APP_ALERT_TITLE, 1));

  std::vector<std::u16string> messages = bubble->GetBubbleViewMessages();

  EXPECT_THAT(messages, testing::ElementsAre(l10n_util::GetStringFUTF16(
                            IDS_EXTENSION_ALERT_ITEM_BLOCKLISTED_MALWARE,
                            base::UTF8ToUTF16(extension->name()))));
}

TEST(ExtensionErrorUIDefaultTest, BubbleTitleForEnterpriseBlockedExtensions) {
  TestErrorUIDelegate delegate;

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Bar").Build();
  delegate.InsertForbidden(extension);

  extensions::ExtensionErrorUIDefault ui(&delegate);
  ManagementPolicyMock provider(extension.get(), false);
  std::unique_ptr<extensions::ManagementPolicy> management_policy =
      std::make_unique<extensions::ManagementPolicy>();
  management_policy->RegisterProvider(&provider);
  ui.SetManagementPolicyForTesting(management_policy.get());
  GlobalErrorWithStandardBubble* bubble = ui.GetErrorForTesting();
  std::u16string title = bubble->GetBubbleViewTitle();
  EXPECT_THAT(title, l10n_util::GetPluralStringFUTF16(
                         IDS_POLICY_BLOCKED_EXTENSION_ALERT_TITLE, 1));

  std::vector<std::u16string> messages = bubble->GetBubbleViewMessages();

  EXPECT_THAT(messages,
              testing::ElementsAre(l10n_util::GetStringFUTF16(
                  IDS_POLICY_BLOCKED_EXTENSION_ALERT_ITEM_DETAIL, u"Bar")));
}
