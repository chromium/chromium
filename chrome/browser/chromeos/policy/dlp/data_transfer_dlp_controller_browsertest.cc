// // Copyright 2020 The Chromium Authors. All rights reserved.
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.

#include <memory>

#include "base/json/json_writer.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/origin.h"

namespace policy {

namespace {

constexpr char kClipboardText[] = "Hello World";

}  // namespace

class DataTransferDlpBrowserTest : public LoginPolicyTestBase {
 public:
  DataTransferDlpBrowserTest() = default;

  void SetDlpRulesPolicy(const base::Value& rules) {
    std::string json;
    base::JSONWriter::Write(rules, &json);

    base::DictionaryValue policy;
    policy.SetKey(key::kDataLeakPreventionRulesList, base::Value(json));
    user_policy_helper()->SetPolicyAndWait(
        policy, /*recommended=*/base::DictionaryValue(),
        ProfileManager::GetActiveUserProfile());
  }

  void SetupCrostini() {
    crostini::FakeCrostiniFeatures crostini_features;
    crostini_features.set_is_allowed_now(true);
    crostini_features.set_enabled(true);

    // Setup CrostiniManager for testing.
    crostini::CrostiniManager* crostini_manager =
        crostini::CrostiniManager::GetForProfile(GetProfileForActiveUser());
    crostini_manager->set_skip_restart_for_testing();
    crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
    crostini_manager->AddRunningContainerForTesting(
        crostini::kCrostiniDefaultVmName,
        crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                                "testuser", "/home/testuser",
                                "PLACEHOLDER_IP"));
  }
};

IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, EmptyPolicy) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(base::UTF8ToUTF16(kClipboardText));
  }
  ui::DataTransferEndpoint data_dst(
      url::Origin::Create(GURL("https://google.com")));
  base::string16 result;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst, &result);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText), result);
}

IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, RestrictedUrl) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  const std::string kUrl1 = "https://mail.google.com";
  const std::string kUrl2 = "https://docs.google.com";
  const std::string kUrl3 = "https://example.com";

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls1(base::Value::Type::LIST);
  src_urls1.Append(kUrl1);
  base::Value dst_urls1(base::Value::Type::LIST);
  dst_urls1.Append("*");
  base::Value restrictions1(base::Value::Type::LIST);
  restrictions1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block Gmail", std::move(src_urls1), std::move(dst_urls1),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions1)));

  base::Value src_urls2(base::Value::Type::LIST);
  src_urls2.Append(kUrl1);
  base::Value dst_urls2(base::Value::Type::LIST);
  dst_urls2.Append(kUrl2);
  base::Value restrictions2(base::Value::Type::LIST);
  restrictions2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kAllowLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Allow Gmail for work purposes", std::move(src_urls2),
      std::move(dst_urls2),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions2)));

  SetDlpRulesPolicy(rules);

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste,
                                     std::make_unique<ui::DataTransferEndpoint>(
                                         url::Origin::Create(GURL(kUrl1))));
    writer.WriteText(base::UTF8ToUTF16(kClipboardText));
  }
  ui::DataTransferEndpoint data_dst1(url::Origin::Create(GURL(kUrl1)));
  base::string16 result1;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result1);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText), result1);

  ui::DataTransferEndpoint data_dst2(url::Origin::Create(GURL(kUrl2)));
  base::string16 result2;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst2, &result2);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText), result2);

  ui::DataTransferEndpoint data_dst3(url::Origin::Create(GURL(kUrl3)));
  base::string16 result3;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst3, &result3);
  EXPECT_EQ(base::string16(), result3);

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste,
                                     std::make_unique<ui::DataTransferEndpoint>(
                                         url::Origin::Create(GURL(kUrl3))));
    writer.WriteText(base::UTF8ToUTF16(kClipboardText));
  }
  ui::DataTransferEndpoint data_dst4(url::Origin::Create(GURL(kUrl1)));
  base::string16 result4;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result4);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText), result4);
}

IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, RestrictedComponent) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  SetupCrostini();

  const std::string kUrl1 = "https://mail.google.com";

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kUrl1);
  base::Value dst_components(base::Value::Type::LIST);
  dst_components.Append(dlp::kArc);
  dst_components.Append(dlp::kCrostini);
  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block Gmail", std::move(src_urls),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      std::move(dst_components), std::move(restrictions)));

  SetDlpRulesPolicy(rules);

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste,
                                     std::make_unique<ui::DataTransferEndpoint>(
                                         url::Origin::Create(GURL(kUrl1))));
    writer.WriteText(base::UTF8ToUTF16(kClipboardText));
  }
  ui::DataTransferEndpoint data_dst1(ui::EndpointType::kDefault);
  base::string16 result1;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result1);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText), result1);

  ui::DataTransferEndpoint data_dst2(ui::EndpointType::kArc);
  base::string16 result2;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst2, &result2);
  EXPECT_EQ(base::string16(), result2);

  ui::DataTransferEndpoint data_dst3(ui::EndpointType::kCrostini);
  base::string16 result3;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst3, &result3);
  EXPECT_EQ(base::string16(), result3);
}

// TODO(crbug.com/1139884): Add browsertests for the clipboard notifications.

}  // namespace policy
