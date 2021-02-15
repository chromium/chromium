// // Copyright 2020 The Chromium Authors. All rights reserved.
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/shell.h"
#include "base/json/json_writer.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/origin.h"

namespace policy {

namespace {

constexpr char kClipboardText1[] = "Hello World";
constexpr char kClipboardText2[] = "abcdef";

constexpr char kMailUrl[] = "https://mail.google.com";
constexpr char kDocsUrl[] = "https://docs.google.com";
constexpr char kExampleUrl[] = "https://example.com";

class FakeClipboardNotifier : public DlpClipboardNotifier {
 public:
  views::Widget* GetWidget() { return GetWidgetForTesting(); }

  void ProceedOnWarn(const ui::DataTransferEndpoint& data_dst) {
    DlpClipboardNotifier::ProceedOnWarn(data_dst, GetWidget());
  }
};

class FakeDlpController : public DataTransferDlpController,
                          public views::WidgetObserver {
 public:
  explicit FakeDlpController(FakeClipboardNotifier* helper)
      : DataTransferDlpController(
            *DlpRulesManagerFactory::GetForPrimaryProfile()),
        helper_(helper) {
    DCHECK(helper);
  }

  ~FakeDlpController() {
    if (widget_ && widget_->HasObserver(this)) {
      widget_->RemoveObserver(this);
    }
  }

  void NotifyBlockedPaste(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst) override {
    helper_->NotifyBlockedAction(data_src, data_dst);
  }

  void WarnOnPaste(const ui::DataTransferEndpoint* const data_src,
                   const ui::DataTransferEndpoint* const data_dst) override {
    helper_->WarnOnAction(data_src, data_dst);
  }

  bool ShouldProceedOnWarn(
      const ui::DataTransferEndpoint* const data_dst) override {
    return helper_->DidUserProceedOnWarn(data_dst);
  }

  bool ObserveWidget() {
    widget_ = helper_->GetWidget();
    if (widget_ && !widget_->HasObserver(this)) {
      widget_->AddObserver(this);
      return true;
    }
    return false;
  }

  MOCK_METHOD1(OnWidgetClosing, void(views::Widget* widget));
  views::Widget* widget_;
  FakeClipboardNotifier* helper_;
};

void SetClipboardText(base::string16 text,
                      std::unique_ptr<ui::DataTransferEndpoint> source) {
  ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste,
                                   source ? std::move(source) : nullptr);
  writer.WriteText(text);
}

// On Widget Closing, a task for NativeWidgetAura::CloseNow() gets posted. This
// task runs after the widget is destroyed which leads to a crash, that's why
// we need to flush the message loop.
void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();
}

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

  void SetupTextfield() {
    // Create a widget containing a single, focusable textfield.
    widget_ = std::make_unique<views::Widget>();

    views::Widget::InitParams params;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    widget_->Init(std::move(params));
    textfield_ = widget_->SetContentsView(std::make_unique<views::Textfield>());
    textfield_->SetAccessibleName(base::UTF8ToUTF16("Textfield"));
    textfield_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

    // Show the widget.
    widget_->SetBounds(gfx::Rect(0, 0, 100, 100));
    widget_->Show();
    ASSERT_TRUE(widget_->IsActive());

    // Focus the textfield and confirm initial state.
    textfield_->RequestFocus();
    ASSERT_TRUE(textfield_->HasFocus());
    ASSERT_TRUE(textfield_->GetText().empty());

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());
  }

  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<views::Widget> widget_;
  views::Textfield* textfield_ = nullptr;
};

// Flaky on MSan bots: http://crbug.com/1178328
#if defined(MEMORY_SANITIZER)
#define MAYBE_EmptyPolicy \
  DISABLED_EmptyPolicy
#else
#define MAYBE_EmptyPolicy \
  EmptyPolicy
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, MAYBE_EmptyPolicy) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  SetClipboardText(base::UTF8ToUTF16(kClipboardText1), nullptr);

  ui::DataTransferEndpoint data_dst(
      url::Origin::Create(GURL("https://google.com")));
  base::string16 result;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst, &result);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText1), result);
}

// Flaky on MSan bots: http://crbug.com/1178328
#if defined(MEMORY_SANITIZER)
#define MAYBE_BlockDestination \
  DISABLED_BlockDestination
#else
#define MAYBE_BlockDestination \
  BlockDestination
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, MAYBE_BlockDestination) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  std::unique_ptr<FakeClipboardNotifier> helper =
      std::make_unique<FakeClipboardNotifier>();
  FakeDlpController dlp_controller(helper.get());

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls1(base::Value::Type::LIST);
  src_urls1.Append(kMailUrl);
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
  src_urls2.Append(kMailUrl);
  base::Value dst_urls2(base::Value::Type::LIST);
  dst_urls2.Append(kDocsUrl);
  base::Value restrictions2(base::Value::Type::LIST);
  restrictions2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kAllowLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Allow Gmail for work purposes", std::move(src_urls2),
      std::move(dst_urls2),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions2)));

  SetDlpRulesPolicy(std::move(rules));

  SetClipboardText(base::UTF8ToUTF16(kClipboardText1),
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kMailUrl))));

  ui::DataTransferEndpoint data_dst1(url::Origin::Create(GURL(kMailUrl)));
  base::string16 result1;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result1);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText1), result1);

  ui::DataTransferEndpoint data_dst2(url::Origin::Create(GURL(kDocsUrl)));
  base::string16 result2;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst2, &result2);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText1), result2);

  ui::DataTransferEndpoint data_dst3(url::Origin::Create(GURL(kExampleUrl)));
  base::string16 result3;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst3, &result3);
  EXPECT_EQ(base::string16(), result3);
  ASSERT_TRUE(dlp_controller.ObserveWidget());

  EXPECT_CALL(dlp_controller, OnWidgetClosing);

  SetClipboardText(base::UTF8ToUTF16(kClipboardText1),
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kExampleUrl))));
  testing::Mock::VerifyAndClearExpectations(helper.get());

  ui::DataTransferEndpoint data_dst4(url::Origin::Create(GURL(kMailUrl)));
  base::string16 result4;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result4);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText1), result4);

  FlushMessageLoop();
}

// Flaky on MSan bots: http://crbug.com/1178328
#if defined(MEMORY_SANITIZER)
#define MAYBE_BlockComponent \
  DISABLED_BlockComponent
#else
#define MAYBE_BlockComponent \
  BlockComponent
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, MAYBE_BlockComponent) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  SetupCrostini();

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kMailUrl);
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
                                         url::Origin::Create(GURL(kMailUrl))));
    writer.WriteText(base::UTF8ToUTF16(kClipboardText1));
  }
  ui::DataTransferEndpoint data_dst1(ui::EndpointType::kDefault);
  base::string16 result1;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result1);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText1), result1);

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

// Flaky on MSan bots: http://crbug.com/1178328
#if defined(MEMORY_SANITIZER)
#define MAYBE_WarnDestination \
  DISABLED_WarnDestination
#else
#define MAYBE_WarnDestination \
  WarnDestination
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, MAYBE_WarnDestination) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  std::unique_ptr<FakeClipboardNotifier> helper =
      std::make_unique<FakeClipboardNotifier>();
  FakeDlpController dlp_controller(helper.get());

  {
    ListPrefUpdate update(g_browser_process->local_state(),
                          policy_prefs::kDlpRulesList);
    base::Value rule(base::Value::Type::DICTIONARY);
    base::Value src_urls(base::Value::Type::DICTIONARY);
    base::Value src_urls_list(base::Value::Type::LIST);
    src_urls_list.Append(base::Value(kMailUrl));
    src_urls.SetKey("urls", std::move(src_urls_list));
    rule.SetKey("sources", std::move(src_urls));

    base::Value dst_urls(base::Value::Type::DICTIONARY);
    base::Value dst_urls_list(base::Value::Type::LIST);
    dst_urls_list.Append(base::Value("*"));
    dst_urls.SetKey("urls", std::move(dst_urls_list));
    rule.SetKey("destinations", std::move(dst_urls));

    base::Value restrictions(base::Value::Type::DICTIONARY);
    base::Value restrictions_list(base::Value::Type::LIST);
    base::Value class_level_dict(base::Value::Type::DICTIONARY);
    class_level_dict.SetKey("class", base::Value("CLIPBOARD"));
    class_level_dict.SetKey("level", base::Value("WARN"));
    restrictions_list.Append(std::move(class_level_dict));
    rule.SetKey("restrictions", std::move(restrictions_list));

    update->Append(std::move(rule));
  }

  SetClipboardText(base::UTF8ToUTF16(kClipboardText1),
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kMailUrl))));

  SetupTextfield();
  // Initiate a paste on textfield_.
  event_generator_->PressKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  event_generator_->ReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);

  EXPECT_EQ("", base::UTF16ToUTF8(textfield_->GetText()));
  ASSERT_TRUE(dlp_controller.ObserveWidget());

  // Accept warning.
  EXPECT_CALL(dlp_controller, OnWidgetClosing);
  ui::DataTransferEndpoint default_endpoint(ui::EndpointType::kDefault);
  helper->ProceedOnWarn(default_endpoint);
  testing::Mock::VerifyAndClearExpectations(&dlp_controller);

  EXPECT_EQ(kClipboardText1, base::UTF16ToUTF8(textfield_->GetText()));

  // Initiate a paste on example url.
  ui::DataTransferEndpoint url_endpoint1(
      url::Origin::Create(GURL(kExampleUrl)));
  base::string16 result;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &url_endpoint1, &result);
  EXPECT_EQ(base::string16(), result);
  ASSERT_TRUE(dlp_controller.ObserveWidget());

  // Accept warning.
  EXPECT_CALL(dlp_controller, OnWidgetClosing);
  helper->ProceedOnWarn(url_endpoint1);
  testing::Mock::VerifyAndClearExpectations(&dlp_controller);

  // Initiate a paste on example url.
  result.clear();
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &url_endpoint1, &result);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText1), result);
  ASSERT_FALSE(dlp_controller.ObserveWidget());

  // Initiate a paste on docs url.
  ui::DataTransferEndpoint url_endpoint2(url::Origin::Create(GURL(kDocsUrl)));
  result.clear();
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &url_endpoint2, &result);
  EXPECT_EQ(base::string16(), result);
  ASSERT_TRUE(dlp_controller.ObserveWidget());

  // Accept warning.
  EXPECT_CALL(dlp_controller, OnWidgetClosing);
  helper->ProceedOnWarn(url_endpoint2);
  testing::Mock::VerifyAndClearExpectations(&dlp_controller);

  // Initiate a paste on docs url.
  result.clear();
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &url_endpoint2, &result);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText1), result);
  ASSERT_FALSE(dlp_controller.ObserveWidget());

  SetClipboardText(base::UTF8ToUTF16(kClipboardText2),
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kMailUrl))));

  // Initiate a paste on example url with notify_if_restricted set to false.
  ui::DataTransferEndpoint url_endpoint3(url::Origin::Create(GURL(kExampleUrl)),
                                         /*notify_if_restricted=*/false);
  result.clear();
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &url_endpoint3, &result);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText2), result);
  ASSERT_FALSE(dlp_controller.ObserveWidget());

  // Initiate a paste on example url with notify_if_restricted set to true.
  result.clear();
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &url_endpoint1, &result);
  EXPECT_EQ(base::string16(), result);
  ASSERT_TRUE(dlp_controller.ObserveWidget());

  // Initiate a paste on textfield_.
  textfield_->SetText(base::string16());
  EXPECT_CALL(dlp_controller, OnWidgetClosing);
  textfield_->RequestFocus();
  event_generator_->PressKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  event_generator_->ReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(&dlp_controller);

  EXPECT_EQ("", base::UTF16ToUTF8(textfield_->GetText()));
  ASSERT_TRUE(dlp_controller.ObserveWidget());

  // Initiate a paste on nullptr data_dst.
  result.clear();
  EXPECT_CALL(dlp_controller, OnWidgetClosing);
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, nullptr, &result);
  testing::Mock::VerifyAndClearExpectations(&dlp_controller);

  EXPECT_EQ(base::string16(), result);
  ASSERT_TRUE(dlp_controller.ObserveWidget());

  EXPECT_CALL(dlp_controller, OnWidgetClosing);
  SetClipboardText(base::UTF8ToUTF16(kClipboardText2),
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kDocsUrl))));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller);

  FlushMessageLoop();
}

// Flaky on MSan bots: http://crbug.com/1178328
#if defined(MEMORY_SANITIZER)
#define MAYBE_WarnComponent \
  DISABLED_WarnComponent
#else
#define MAYBE_WarnComponent \
  WarnComponent
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, MAYBE_WarnComponent) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  SetupCrostini();

  {
    ListPrefUpdate update(g_browser_process->local_state(),
                          policy_prefs::kDlpRulesList);
    base::Value rule(base::Value::Type::DICTIONARY);
    base::Value src_urls(base::Value::Type::DICTIONARY);
    base::Value src_urls_list(base::Value::Type::LIST);
    src_urls_list.Append(base::Value(kMailUrl));
    src_urls.SetKey("urls", std::move(src_urls_list));
    rule.SetKey("sources", std::move(src_urls));

    base::Value dst_components(base::Value::Type::DICTIONARY);
    base::Value dst_components_list(base::Value::Type::LIST);
    dst_components_list.Append(base::Value("ARC"));
    dst_components_list.Append(base::Value("CROSTINI"));
    dst_components_list.Append(base::Value("PLUGIN_VM"));
    dst_components.SetKey("components", std::move(dst_components_list));
    rule.SetKey("destinations", std::move(dst_components));

    base::Value restrictions(base::Value::Type::DICTIONARY);
    base::Value restrictions_list(base::Value::Type::LIST);
    base::Value class_level_dict(base::Value::Type::DICTIONARY);
    class_level_dict.SetKey("class", base::Value("CLIPBOARD"));
    class_level_dict.SetKey("level", base::Value("WARN"));
    restrictions_list.Append(std::move(class_level_dict));
    rule.SetKey("restrictions", std::move(restrictions_list));

    update->Append(std::move(rule));
  }

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste,
                                     std::make_unique<ui::DataTransferEndpoint>(
                                         url::Origin::Create(GURL(kMailUrl))));
    writer.WriteText(base::UTF8ToUTF16(kClipboardText1));
  }

  ui::DataTransferEndpoint arc_endpoint(ui::EndpointType::kArc);
  base::string16 result;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &arc_endpoint, &result);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText1), result);

  ui::DataTransferEndpoint crostini_endpoint(ui::EndpointType::kCrostini);
  result.clear();
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &crostini_endpoint, &result);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardText1), result);
}

}  // namespace policy
