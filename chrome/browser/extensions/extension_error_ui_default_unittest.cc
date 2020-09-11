// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_ui_default.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
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

}  // namespace

TEST(ExtensionErrorUIDefaultTest, BubbleTitleAndMessageMentionsExtension) {
  TestErrorUIDelegate delegate;

  delegate.InsertForbidden(extensions::ExtensionBuilder("Bar").Build());
  delegate.InsertForbidden(extensions::ExtensionBuilder("Baz").Build());

  extensions::ExtensionErrorUIDefault ui(&delegate);
  GlobalErrorWithStandardBubble* bubble = ui.GetErrorForTesting();

  base::string16 title = bubble->GetBubbleViewTitle();
  EXPECT_THAT(title,
              l10n_util::GetPluralStringFUTF16(IDS_EXTENSION_ALERT_TITLE, 2));

  std::vector<base::string16> messages = bubble->GetBubbleViewMessages();

  EXPECT_THAT(
      messages,
      testing::ElementsAre(
          l10n_util::GetStringFUTF16(IDS_EXTENSION_ALERT_ITEM_BLOCKLISTED_OTHER,
                                     base::UTF8ToUTF16("Bar")),
          l10n_util::GetStringFUTF16(IDS_EXTENSION_ALERT_ITEM_BLOCKLISTED_OTHER,
                                     base::UTF8ToUTF16("Baz"))));
}

TEST(ExtensionErrorUIDefaultTest, BubbleTitleAndMessageMentionsApp) {
  TestErrorUIDelegate delegate;

  delegate.InsertForbidden(
      extensions::ExtensionBuilder(
          "Bar", extensions::ExtensionBuilder::Type::PLATFORM_APP)
          .Build());

  extensions::ExtensionErrorUIDefault ui(&delegate);
  GlobalErrorWithStandardBubble* bubble = ui.GetErrorForTesting();

  base::string16 title = bubble->GetBubbleViewTitle();
  EXPECT_THAT(title, l10n_util::GetPluralStringFUTF16(IDS_APP_ALERT_TITLE, 1));

  std::vector<base::string16> messages = bubble->GetBubbleViewMessages();

  EXPECT_THAT(messages, testing::ElementsAre(l10n_util::GetStringFUTF16(
                            IDS_APP_ALERT_ITEM_BLOCKLISTED_OTHER,
                            base::UTF8ToUTF16("Bar"))));
}

TEST(ExtensionErrorUIDefaultTest, BubbleMessageMentionsMalware) {
  TestErrorUIDelegate delegate;
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(
          "Bar", extensions::ExtensionBuilder::Type::PLATFORM_APP)
          .Build();
  extensions::ExtensionPrefs::Get(delegate.GetContext())
      ->AddDisableReason(
          extension->id(),
          extensions::disable_reason::DISABLE_REMOTELY_FOR_MALWARE);
  delegate.InsertForbidden(extension);

  extensions::ExtensionErrorUIDefault ui(&delegate);
  GlobalErrorWithStandardBubble* bubble = ui.GetErrorForTesting();

  base::string16 title = bubble->GetBubbleViewTitle();
  EXPECT_THAT(title, l10n_util::GetPluralStringFUTF16(IDS_APP_ALERT_TITLE, 1));

  std::vector<base::string16> messages = bubble->GetBubbleViewMessages();

  EXPECT_THAT(messages, testing::ElementsAre(l10n_util::GetStringFUTF16(
                            IDS_EXTENSION_ALERT_ITEM_BLOCKLISTED_MALWARE,
                            base::UTF8ToUTF16(extension->name()))));
}
