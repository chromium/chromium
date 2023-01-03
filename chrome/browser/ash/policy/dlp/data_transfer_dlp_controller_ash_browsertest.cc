// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/ash/policy/login/login_policy_test_base.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
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
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace policy {

namespace {

constexpr char16_t kClipboardText116[] = u"Hello World";

constexpr char kMailUrl[] = "https://mail.google.com";

class FakeClipboardNotifier : public DlpClipboardNotifier {
 public:
  views::Widget* GetWidget() { return widget_.get(); }

  void ProceedPressed(const ui::DataTransferEndpoint& data_dst,
                      base::RepeatingCallback<void()> reporting_cb) {
    DlpClipboardNotifier::ProceedPressed(data_dst, std::move(reporting_cb),
                                         GetWidget());
  }

  void BlinkProceedPressed(const ui::DataTransferEndpoint& data_dst) {
    DlpClipboardNotifier::BlinkProceedPressed(data_dst, GetWidget());
  }

  void CancelWarningPressed(const ui::DataTransferEndpoint& data_dst) {
    DlpClipboardNotifier::CancelWarningPressed(data_dst, GetWidget());
  }
};

class FakeDlpController : public DataTransferDlpController,
                          public views::WidgetObserver {
 public:
  FakeDlpController(const DlpRulesManager& dlp_rules_manager,
                    FakeClipboardNotifier* helper)
      : DataTransferDlpController(dlp_rules_manager), helper_(helper) {
    DCHECK(helper);
  }

  ~FakeDlpController() override {
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
                   const ui::DataTransferEndpoint* const data_dst,
                   base::RepeatingCallback<void()> reporting_cb) override {
    helper_->WarnOnPaste(data_src, data_dst, std::move(reporting_cb));
  }

  void SetBlinkQuitCallback(base::RepeatingClosure cb) {
    blink_quit_cb_ = std::move(cb);
  }

  void WarnOnBlinkPaste(const ui::DataTransferEndpoint* const data_src,
                        const ui::DataTransferEndpoint* const data_dst,
                        content::WebContents* web_contents,
                        base::OnceCallback<void(bool)> paste_cb) override {
    blink_data_dst_.emplace(*data_dst);
    helper_->WarnOnBlinkPaste(data_src, data_dst, web_contents,
                              std::move(paste_cb));
    std::move(blink_quit_cb_).Run();
  }

  bool ShouldPasteOnWarn(
      const ui::DataTransferEndpoint* const data_dst) override {
    if (force_paste_on_warn_)
      return true;
    return helper_->DidUserApproveDst(data_dst);
  }

  bool ObserveWidget() {
    widget_ = helper_->GetWidget();
    if (widget_ && !widget_->HasObserver(this)) {
      widget_->AddObserver(this);
      return true;
    }
    return false;
  }

  views::Widget* widget_ = nullptr;
  FakeClipboardNotifier* helper_ = nullptr;
  absl::optional<ui::DataTransferEndpoint> blink_data_dst_;
  base::RepeatingClosure blink_quit_cb_ = base::DoNothing();
  bool force_paste_on_warn_ = false;

 protected:
  base::TimeDelta GetSkipReportingTimeout() override {
    // Override with a very high value to ensure that tests are passing on slow
    // debug builds.
    return base::Milliseconds(1000);
  }
};

class MockDlpRulesManager : public DlpRulesManagerImpl {
 public:
  explicit MockDlpRulesManager(PrefService* local_state)
      : DlpRulesManagerImpl(local_state) {}
  ~MockDlpRulesManager() override = default;

  MOCK_CONST_METHOD0(GetReportingManager, DlpReportingManager*());
  MOCK_CONST_METHOD0(GetDlpFilesController, DlpFilesController*());
};

}  // namespace

class DataTransferDlpAshBrowserTest : public InProcessBrowserTest {
 public:
  DataTransferDlpAshBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&DataTransferDlpAshBrowserTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());

    reporting_manager_ = std::make_unique<DlpReportingManager>();
    SetReportQueueForReportingManager(
        reporting_manager_.get(), events,
        base::SequencedTaskRunner::GetCurrentDefault());
    ON_CALL(*rules_manager_, GetReportingManager)
        .WillByDefault(::testing::Return(reporting_manager_.get()));

    files_controller_ = std::make_unique<DlpFilesController>(*rules_manager_);
    ON_CALL(*rules_manager_, GetDlpFilesController)
        .WillByDefault(::testing::Return(files_controller_.get()));

    dlp_controller_ =
        std::make_unique<FakeDlpController>(*rules_manager_, &helper_);
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto mock_rules_manager =
        std::make_unique<testing::NiceMock<MockDlpRulesManager>>(
            g_browser_process->local_state());
    rules_manager_ = mock_rules_manager.get();
    return mock_rules_manager;
  }

  void TearDownOnMainThread() override {
    dlp_controller_.reset();
    reporting_manager_.reset();
    files_controller_.reset();
  }

  void SetupCrostini() {
    crostini::FakeCrostiniFeatures crostini_features;
    crostini_features.set_is_allowed_now(true);
    crostini_features.set_enabled(true);

    // Setup CrostiniManager for testing.
    crostini::CrostiniManager* crostini_manager =
        crostini::CrostiniManager::GetForProfile(browser()->profile());
    crostini_manager->set_skip_restart_for_testing();
    crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
    crostini_manager->AddRunningContainerForTesting(
        crostini::kCrostiniDefaultVmName,
        crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                                "testuser", "/home/testuser",
                                "PLACEHOLDER_IP"));
  }

  MockDlpRulesManager* rules_manager_;
  std::unique_ptr<DlpReportingManager> reporting_manager_;
  std::vector<DlpPolicyEvent> events;
  FakeClipboardNotifier helper_;
  std::unique_ptr<FakeDlpController> dlp_controller_;
  std::unique_ptr<DlpFilesController> files_controller_;
};

// Flaky on MSan bots: http://crbug.com/1178328
#if defined(MEMORY_SANITIZER)
#define MAYBE_BlockComponent DISABLED_BlockComponent
#else
#define MAYBE_BlockComponent BlockComponent
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpAshBrowserTest, MAYBE_BlockComponent) {
  SetupCrostini();
  {
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);

    base::Value::List src_urls;
    src_urls.Append(kMailUrl);
    base::Value::List dst_components;
    dst_components.Append(dlp::kArc);
    dst_components.Append(dlp::kCrostini);
    base::Value::List restrictions;
    restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
        dlp::kClipboardRestriction, dlp::kBlockLevel));
    update->Append(dlp_test_util::CreateRule(
        "rule #1", "Block Gmail", std::move(src_urls),
        /*dst_urls=*/base::Value::List(), std::move(dst_components),
        std::move(restrictions)));
  }

  {
    ui::ScopedClipboardWriter writer(
        ui::ClipboardBuffer::kCopyPaste,
        std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));
    writer.WriteText(kClipboardText116);
  }
  ui::DataTransferEndpoint data_dst1(ui::EndpointType::kDefault);
  std::u16string result1;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result1);
  EXPECT_EQ(kClipboardText116, result1);

  ui::DataTransferEndpoint data_dst2(ui::EndpointType::kArc);
  std::u16string result2;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst2, &result2);
  EXPECT_EQ(std::u16string(), result2);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_THAT(events[0], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                             kMailUrl, DlpRulesManager::Component::kArc,
                             DlpRulesManager::Restriction::kClipboard,
                             DlpRulesManager::Level::kBlock)));

  ui::DataTransferEndpoint data_dst3(ui::EndpointType::kCrostini);
  std::u16string result3;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst3, &result3);
  EXPECT_EQ(std::u16string(), result3);
  ASSERT_EQ(events.size(), 2u);
  EXPECT_THAT(events[1], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                             kMailUrl, DlpRulesManager::Component::kCrostini,
                             DlpRulesManager::Restriction::kClipboard,
                             DlpRulesManager::Level::kBlock)));
}

// Flaky on MSan bots: http://crbug.com/1178328
#if defined(MEMORY_SANITIZER)
#define MAYBE_WarnComponent DISABLED_WarnComponent
#else
#define MAYBE_WarnComponent WarnComponent
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpAshBrowserTest, MAYBE_WarnComponent) {
  SetupCrostini();

  {
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    base::Value::Dict rule;
    base::Value::Dict src_urls;
    base::Value::List src_urls_list;
    src_urls_list.Append(kMailUrl);
    src_urls.Set("urls", std::move(src_urls_list));
    rule.Set("sources", std::move(src_urls));

    base::Value::Dict dst_components;
    base::Value::List dst_components_list;
    dst_components_list.Append("ARC");
    dst_components_list.Append("CROSTINI");
    dst_components_list.Append("PLUGIN_VM");
    dst_components.Set("components", std::move(dst_components_list));
    rule.Set("destinations", std::move(dst_components));

    base::Value::Dict restrictions;
    base::Value::List restrictions_list;
    base::Value::Dict class_level_dict;
    class_level_dict.Set("class", "CLIPBOARD");
    class_level_dict.Set("level", "WARN");
    restrictions_list.Append(std::move(class_level_dict));
    rule.Set("restrictions", std::move(restrictions_list));

    update->Append(std::move(rule));
  }

  {
    ui::ScopedClipboardWriter writer(
        ui::ClipboardBuffer::kCopyPaste,
        std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));
    writer.WriteText(kClipboardText116);
  }

  ui::DataTransferEndpoint arc_endpoint(ui::EndpointType::kArc);
  std::u16string result;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &arc_endpoint, &result);
  EXPECT_EQ(kClipboardText116, result);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_THAT(events[0], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                             kMailUrl, DlpRulesManager::Component::kArc,
                             DlpRulesManager::Restriction::kClipboard,
                             DlpRulesManager::Level::kWarn)));

  ui::DataTransferEndpoint crostini_endpoint(ui::EndpointType::kCrostini);
  result.clear();
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &crostini_endpoint, &result);
  EXPECT_EQ(kClipboardText116, result);
  ASSERT_EQ(events.size(), 2u);
  EXPECT_THAT(events[1], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                             kMailUrl, DlpRulesManager::Component::kCrostini,
                             DlpRulesManager::Restriction::kClipboard,
                             DlpRulesManager::Level::kWarn)));
}

}  // namespace policy
