// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_rules_manager_test_utils.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/reporting/client/mock_report_queue.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace policy {

namespace {

constexpr char kClipboardText1[] = "Hello World";
constexpr char16_t kClipboardText116[] = u"Hello World";
constexpr char16_t kClipboardText2[] = u"abcdef";

constexpr char kMailUrl[] = "https://mail.google.com";
constexpr char kDocsUrl[] = "https://docs.google.com";
constexpr char kExampleUrl[] = "https://example.com";

constexpr char kRuleName1[] = "rule #1";
constexpr char kRuleId1[] = "testid1";
const DlpRulesManager::RuleMetadata kRuleMetadata1(kRuleName1, kRuleId1);

constexpr char kRuleName2[] = "rule #2";
constexpr char kRuleId2[] = "testid2";

class FakeClipboardNotifier : public DlpClipboardNotifier {
 public:
  views::Widget* GetWidget() { return widget_.get(); }

  void ProceedPressed(std::unique_ptr<ui::ClipboardData> data,
                      const ui::DataTransferEndpoint& data_dst,
                      base::OnceClosure reporting_cb) {
    DlpClipboardNotifier::ProceedPressed(std::move(data), data_dst,
                                         std::move(reporting_cb), GetWidget());
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
    auto* widget = helper_->GetWidget();
    if (widget) {
      widget->RemoveObserver(this);
    }
  }

  void NotifyBlockedPaste(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst) override {
    helper_->NotifyBlockedAction(data_src, data_dst);
  }

  void WarnOnPaste(base::optional_ref<const ui::DataTransferEndpoint> data_src,
                   base::optional_ref<const ui::DataTransferEndpoint> data_dst,
                   base::OnceClosure reporting_cb) override {
    helper_->WarnOnPaste(data_src, data_dst, std::move(reporting_cb));
  }

  void WarnOnBlinkPaste(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> paste_cb) override {
    blink_data_dst_.emplace(*data_dst);
    helper_->WarnOnBlinkPaste(data_src, data_dst, web_contents,
                              std::move(paste_cb));
  }

  bool ShouldPasteOnWarn(
      base::optional_ref<const ui::DataTransferEndpoint> data_dst) override {
    if (force_paste_on_warn_) {
      return true;
    }
    return helper_->DidUserApproveDst(data_dst);
  }

  bool ObserveWidget() {
    auto* widget = helper_->GetWidget();
    if (widget && !widget->HasObserver(this)) {
      widget->AddObserver(this);
      return true;
    }
    return false;
  }

  void ReportWarningProceededEvent(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      const std::string& src_pattern,
      const std::string& dst_pattern,
      const DlpRulesManager::RuleMetadata& rule_metadata,
      bool is_clipboard_event) {
    DataTransferDlpController::ReportWarningProceededEvent(
        data_src, data_dst, src_pattern, dst_pattern, is_clipboard_event,
        rule_metadata);
  }

  raw_ptr<FakeClipboardNotifier> helper_ = nullptr;
  std::optional<ui::DataTransferEndpoint> blink_data_dst_;
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
  explicit MockDlpRulesManager(PrefService* local_state, Profile* profile)
      : DlpRulesManagerImpl(local_state, profile) {}
  ~MockDlpRulesManager() override = default;

  MOCK_CONST_METHOD0(GetReportingManager,
                     data_controls::DlpReportingManager*());
};

void SetClipboardText(std::u16string text,
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

class DataTransferDlpBrowserTest : public InProcessBrowserTest {
 public:
  DataTransferDlpBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    test_reporting_ = ::reporting::ReportingClient::TestEnvironment::
        CreateWithStorageModule();

    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&DataTransferDlpBrowserTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());

    reporting_manager_ = std::make_unique<data_controls::DlpReportingManager>();
    auto reporting_queue = std::unique_ptr<::reporting::MockReportQueue,
                                           base::OnTaskRunnerDeleter>(
        new ::reporting::MockReportQueue(),
        base::OnTaskRunnerDeleter(
            std::move(base::SequencedTaskRunner::GetCurrentDefault())));
    reporting_queue_ = reporting_queue.get();
    reporting_manager_->SetReportQueueForTest(std::move(reporting_queue));
    ON_CALL(*rules_manager_, GetReportingManager)
        .WillByDefault(::testing::Return(reporting_manager_.get()));

    dlp_controller_ =
        std::make_unique<FakeDlpController>(*rules_manager_, &helper_);
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto mock_rules_manager =
        std::make_unique<testing::NiceMock<MockDlpRulesManager>>(
            g_browser_process->local_state(),
            Profile::FromBrowserContext(context));
    rules_manager_ = mock_rules_manager.get();
    return mock_rules_manager;
  }

  void TearDownOnMainThread() override {
    reporting_queue_ = nullptr;
    dlp_controller_.reset();
    reporting_manager_.reset();
    test_reporting_.reset();
  }

  void SetupTextfield() {
    // Create a widget containing a single, focusable textfield.
    widget_ = std::make_unique<views::Widget>();

    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_->Init(std::move(params));
    textfield_ = widget_->SetContentsView(std::make_unique<views::Textfield>());
    textfield_->GetViewAccessibility().SetName(u"Textfield");
    textfield_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

    // Show the widget.
    widget_->SetBounds(gfx::Rect(0, 0, 100, 100));
    widget_->Show();

    widget_->Show();
    views::test::WaitForWidgetActive(widget_.get(), true);

    ASSERT_TRUE(widget_->IsActive());

    // Focus the textfield and confirm initial state.
    textfield_->RequestFocus();
    ASSERT_TRUE(textfield_->HasFocus());
    ASSERT_TRUE(textfield_->GetText().empty());

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        widget_->GetNativeWindow()->GetRootWindow());
  }

  // Expects `event` to be reported then quits `run_loop`.
  void ExpectEventTobeReported(DlpPolicyEvent expected_event,
                               base::RunLoop& run_loop) {
    EXPECT_CALL(*reporting_queue_, AddRecord)
        .WillOnce([&run_loop, expected_event](
                      std::string_view record, ::reporting::Priority priority,
                      ::reporting::ReportQueue::EnqueueCallback callback) {
          DlpPolicyEvent event;
          ASSERT_TRUE(event.ParseFromString(std::string(record)));
          EXPECT_THAT(event, data_controls::IsDlpPolicyEvent(expected_event));
          std::move(callback).Run(::reporting::Status::StatusOK());
          run_loop.Quit();
        });
  }

  std::unique_ptr<::reporting::ReportingClient::TestEnvironment>
      test_reporting_;
  raw_ptr<MockDlpRulesManager, DanglingUntriaged> rules_manager_;
  std::unique_ptr<data_controls::DlpReportingManager> reporting_manager_;
  raw_ptr<::reporting::MockReportQueue> reporting_queue_;
  FakeClipboardNotifier helper_;
  std::unique_ptr<FakeDlpController> dlp_controller_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::Textfield, DanglingUntriaged> textfield_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, EmptyPolicy) {
  SetClipboardText(kClipboardText116, nullptr);

  ui::DataTransferEndpoint data_dst((GURL("https://google.com")));
  std::u16string result;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst, &result);
  EXPECT_EQ(kClipboardText116, result);
}

IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, BlockDestination) {
  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule1(kRuleName1, "Block Gmail", kRuleId1);
    rule1.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        data_controls::kRestrictionClipboard, data_controls::kLevelBlock);
    dlp_test_util::DlpRule rule2(kRuleName2, "Allow Gmail for work purposes",
                                 kRuleId2);
    rule2.AddSrcUrl(kMailUrl).AddDstUrl(kDocsUrl).AddRestriction(
        data_controls::kRestrictionClipboard, data_controls::kLevelAllow);

    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule1.Create());
    update->Append(rule2.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  ui::DataTransferEndpoint data_dst1((GURL(kMailUrl)));
  std::u16string result1;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result1);
  EXPECT_EQ(kClipboardText116, result1);

  ui::DataTransferEndpoint data_dst2((GURL(kDocsUrl)));
  std::u16string result2;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst2, &result2);
  EXPECT_EQ(kClipboardText116, result2);

  base::RunLoop run_loop;
  ExpectEventTobeReported(
      CreateDlpPolicyEvent(GURL(kMailUrl).spec(), GURL(kExampleUrl).spec(),
                           DlpRulesManager::Restriction::kClipboard, kRuleName1,
                           kRuleId1, DlpRulesManager::Level::kBlock),
      run_loop);
  ui::DataTransferEndpoint data_dst3((GURL(kExampleUrl)));
  std::u16string result3;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst3, &result3);
  EXPECT_EQ(std::u16string(), result3);
  ASSERT_TRUE(dlp_controller_->ObserveWidget());
  run_loop.Run();

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kExampleUrl))));

  ui::DataTransferEndpoint data_dst4((GURL(kMailUrl)));
  std::u16string result4;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result4);
  EXPECT_EQ(kClipboardText116, result4);

  FlushMessageLoop();
}

// TODO(b/278719678): Enable on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS) && !BUILDFLAG(IS_CHROMEOS_DEVICE)
#define MAYBE_WarnDestination DISABLED_WarnDestination
#else
#define MAYBE_WarnDestination WarnDestination
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, MAYBE_WarnDestination) {
  base::WeakPtr<views::Widget> widget;

  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule(kRuleName1, "description", kRuleId1);
    rule.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        data_controls::kRestrictionClipboard, data_controls::kLevelWarn);
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  SetupTextfield();
  // Initiate a paste on textfield_.
  {
    base::RunLoop run_loop;
    ExpectEventTobeReported(
        CreateDlpPolicyEvent(GURL(kMailUrl).spec(), "*",
                             DlpRulesManager::Restriction::kClipboard,
                             kRuleName1, kRuleId1,
                             DlpRulesManager::Level::kWarn),
        run_loop);
    event_generator_->PressAndReleaseKeyAndModifierKeys(ui::VKEY_V,
                                                        ui::EF_CONTROL_DOWN);

    EXPECT_EQ("", base::UTF16ToUTF8(textfield_->GetText()));
    ASSERT_TRUE(dlp_controller_->ObserveWidget());
    widget = helper_.GetWidget()->GetWeakPtr();
    EXPECT_FALSE(widget->IsClosed());

    run_loop.Run();
  }

  // Accept warning.
  {
    base::RunLoop run_loop;
    ExpectEventTobeReported(
        CreateDlpPolicyWarningProceededEvent(
            GURL(kMailUrl).spec(), "*",
            DlpRulesManager::Restriction::kClipboard, kRuleName1, kRuleId1),
        run_loop);
    ui::DataTransferEndpoint default_endpoint(ui::EndpointType::kDefault);
    auto data_src =
        std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl)));
    auto reporting_cb =
        base::BindOnce(&FakeDlpController::ReportWarningProceededEvent,
                       base::Unretained(dlp_controller_.get()), data_src.get(),
                       &default_endpoint, kMailUrl, "*", kRuleMetadata1, true);
    auto data = std::make_unique<ui::ClipboardData>();
    data->set_text(base::UTF16ToUTF8(std::u16string(kClipboardText116)));
    helper_.ProceedPressed(std::move(data), default_endpoint,
                           std::move(reporting_cb));
    EXPECT_TRUE(!widget || widget->IsClosed());
    EXPECT_EQ(kClipboardText116, textfield_->GetText());
    run_loop.Run();
  }

  // Initiate a paste on textfield_.
  {
    base::RunLoop run_loop;
    ExpectEventTobeReported(
        CreateDlpPolicyEvent(GURL(kMailUrl).spec(), "*",
                             DlpRulesManager::Restriction::kClipboard,
                             kRuleName1, kRuleId1,
                             DlpRulesManager::Level::kWarn),
        run_loop);
    SetClipboardText(
        kClipboardText2,
        std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));
    textfield_->SetText(std::u16string());
    textfield_->RequestFocus();
    event_generator_->PressAndReleaseKeyAndModifierKeys(ui::VKEY_V,
                                                        ui::EF_CONTROL_DOWN);
    EXPECT_EQ("", base::UTF16ToUTF8(textfield_->GetText()));
    ASSERT_TRUE(dlp_controller_->ObserveWidget());
    widget = helper_.GetWidget()->GetWeakPtr();
    EXPECT_FALSE(widget->IsClosed());
    run_loop.Run();
  }

  // Initiate a paste on nullptr data_dst.
  {
    std::u16string result;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::ClipboardBuffer::kCopyPaste, nullptr, &result);
    EXPECT_TRUE(!widget || widget->IsClosed());

    EXPECT_EQ(std::u16string(), result);
    ASSERT_TRUE(dlp_controller_->ObserveWidget());
    widget = helper_.GetWidget()->GetWeakPtr();
    EXPECT_FALSE(widget->IsClosed());
  }

  FlushMessageLoop();
}

// TODO(b/300198284): Reenable.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_DataTransferDlpBlinkBrowserTest \
  DISABLED_DataTransferDlpBlinkBrowserTest
#else
#define MAYBE_DataTransferDlpBlinkBrowserTest DataTransferDlpBlinkBrowserTest
#endif
class MAYBE_DataTransferDlpBlinkBrowserTest : public InProcessBrowserTest {
 public:
  MAYBE_DataTransferDlpBlinkBrowserTest() = default;
  MAYBE_DataTransferDlpBlinkBrowserTest(
      const MAYBE_DataTransferDlpBlinkBrowserTest&) = delete;
  MAYBE_DataTransferDlpBlinkBrowserTest& operator=(
      const MAYBE_DataTransferDlpBlinkBrowserTest&) = delete;
  ~MAYBE_DataTransferDlpBlinkBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    TestingProfile::Builder builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_ = builder.Build();

    rules_manager_ = std::make_unique<::testing::NiceMock<MockDlpRulesManager>>(
        g_browser_process->local_state(), profile_.get());

    reporting_manager_ = std::make_unique<data_controls::DlpReportingManager>();
    auto reporting_queue = std::unique_ptr<::reporting::MockReportQueue,
                                           base::OnTaskRunnerDeleter>(
        new ::reporting::MockReportQueue(),
        base::OnTaskRunnerDeleter(
            std::move(base::SequencedTaskRunner::GetCurrentDefault())));
    reporting_queue_ = reporting_queue.get();
    reporting_manager_->SetReportQueueForTest(std::move(reporting_queue));
    ON_CALL(*rules_manager_, GetReportingManager)
        .WillByDefault(::testing::Return(reporting_manager_.get()));

    dlp_controller_ =
        std::make_unique<FakeDlpController>(*rules_manager_, &helper_);
  }

  void TearDownOnMainThread() override {
    reporting_queue_ = nullptr;
    dlp_controller_.reset();
    reporting_manager_.reset();
    rules_manager_.reset();
    profile_.reset();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ::testing::AssertionResult ExecJs(content::WebContents* web_contents,
                                    const std::string& code) {
    return content::ExecJs(web_contents, code,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           /*world_id=*/1);
  }

  content::EvalJsResult EvalJs(content::WebContents* web_contents,
                               const std::string& code) {
    return content::EvalJs(web_contents, code,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           /*world_id=*/1);
  }

  // Expects `event` to be reported then quits `run_loop`.
  void ExpectEventTobeReported(DlpPolicyEvent expected_event,
                               base::RunLoop& run_loop) {
    EXPECT_CALL(*reporting_queue_, AddRecord)
        .WillOnce([&run_loop, expected_event](
                      std::string_view record, ::reporting::Priority priority,
                      ::reporting::ReportQueue::EnqueueCallback callback) {
          DlpPolicyEvent event;
          ASSERT_TRUE(event.ParseFromString(std::string(record)));
          EXPECT_THAT(event, data_controls::IsDlpPolicyEvent(expected_event));
          std::move(callback).Run(::reporting::Status::StatusOK());
          run_loop.Quit();
        });
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<::testing::NiceMock<MockDlpRulesManager>> rules_manager_;
  std::unique_ptr<data_controls::DlpReportingManager> reporting_manager_;
  raw_ptr<::reporting::MockReportQueue> reporting_queue_;
  FakeClipboardNotifier helper_;
  std::unique_ptr<FakeDlpController> dlp_controller_;
};

IN_PROC_BROWSER_TEST_F(MAYBE_DataTransferDlpBlinkBrowserTest, ProceedOnWarn) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule(kRuleName1, "description", kRuleId1);
    rule.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        data_controls::kRestrictionClipboard, data_controls::kLevelWarn);
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));

  content::SimulateMouseClick(GetActiveWebContents(), 0,
                              blink::WebPointerProperties::Button::kLeft);

  // Send paste event and wait till the event is reported.
  {
    base::RunLoop run_loop;
    ExpectEventTobeReported(
        CreateDlpPolicyEvent(
            GURL(kMailUrl).spec(),
            embedded_test_server()->GetURL("/title1.html").spec(),
            DlpRulesManager::Restriction::kClipboard, kRuleName1, kRuleId1,
            DlpRulesManager::Level::kWarn),
        run_loop);
    GetActiveWebContents()->Paste();
    run_loop.Run();

    ASSERT_TRUE(dlp_controller_->ObserveWidget());
    base::WeakPtr<views::Widget> widget = helper_.GetWidget()->GetWeakPtr();
    EXPECT_FALSE(widget->IsClosed());
  }

  // Proceed the warning.
  {
    base::RunLoop run_loop;
    ExpectEventTobeReported(
        CreateDlpPolicyWarningProceededEvent(
            GURL(kMailUrl).spec(),
            embedded_test_server()->GetURL("/title1.html").spec(),
            DlpRulesManager::Restriction::kClipboard, kRuleName1, kRuleId1),
        run_loop);
    ASSERT_TRUE(dlp_controller_->blink_data_dst_.has_value());
    helper_.BlinkProceedPressed(dlp_controller_->blink_data_dst_.value());
    run_loop.Run();

    EXPECT_EQ(kClipboardText1, EvalJs(GetActiveWebContents(), "p"));
    ASSERT_FALSE(helper_.GetWidget());
  }
}

IN_PROC_BROWSER_TEST_F(MAYBE_DataTransferDlpBlinkBrowserTest, CancelWarn) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule(kRuleName1, "description", kRuleId1);
    rule.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        data_controls::kRestrictionClipboard, data_controls::kLevelWarn);
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));
  content::SimulateMouseClick(GetActiveWebContents(), 0,
                              blink::WebPointerProperties::Button::kLeft);

  // Send paste event and wait till the event is reported.
  {
    base::RunLoop run_loop;
    ExpectEventTobeReported(
        CreateDlpPolicyEvent(
            GURL(kMailUrl).spec(),
            embedded_test_server()->GetURL("/title1.html").spec(),
            DlpRulesManager::Restriction::kClipboard, kRuleName1, kRuleId1,
            DlpRulesManager::Level::kWarn),
        run_loop);
    GetActiveWebContents()->Paste();
    run_loop.Run();

    ASSERT_TRUE(dlp_controller_->ObserveWidget());
    base::WeakPtr<views::Widget> widget = helper_.GetWidget()->GetWeakPtr();
    EXPECT_FALSE(widget->IsClosed());
  }

  // Cancel the warning.
  {
    ASSERT_TRUE(dlp_controller_->blink_data_dst_.has_value());
    helper_.CancelWarningPressed(dlp_controller_->blink_data_dst_.value());

    EXPECT_EQ("", EvalJs(GetActiveWebContents(), "p"));
    ASSERT_FALSE(helper_.GetWidget());
  }
}

IN_PROC_BROWSER_TEST_F(MAYBE_DataTransferDlpBlinkBrowserTest,
                       ShouldProceedWarn) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule(kRuleName1, "", kRuleId1);
    rule.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        data_controls::kRestrictionClipboard, data_controls::kLevelWarn);
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));

  content::SimulateMouseClick(GetActiveWebContents(), 0,
                              blink::WebPointerProperties::Button::kLeft);

  // Send paste event and wait till the event is reported.
  {
    base::RunLoop run_loop;
    ExpectEventTobeReported(
        CreateDlpPolicyWarningProceededEvent(
            GURL(kMailUrl).spec(),
            embedded_test_server()->GetURL("/title1.html").spec(),
            DlpRulesManager::Restriction::kClipboard, kRuleName1, kRuleId1),
        run_loop);
    dlp_controller_->force_paste_on_warn_ = true;
    GetActiveWebContents()->Paste();
    run_loop.Run();

    EXPECT_FALSE(dlp_controller_->ObserveWidget());
    EXPECT_FALSE(helper_.GetWidget());
    EXPECT_EQ(kClipboardText1, EvalJs(GetActiveWebContents(), "p"));
  }
}

// Test case for crbug.com/1213143
IN_PROC_BROWSER_TEST_F(MAYBE_DataTransferDlpBlinkBrowserTest, Reporting) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule(kRuleName1, "description", kRuleId1);
    rule.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        data_controls::kRestrictionClipboard, data_controls::kLevelReport);
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));

  content::SimulateMouseClick(GetActiveWebContents(), 0,
                              blink::WebPointerProperties::Button::kLeft);

  // Send paste event and wait till the event is reported.
  {
    base::RunLoop run_loop;
    ExpectEventTobeReported(
        CreateDlpPolicyEvent(
            GURL(kMailUrl).spec(),
            embedded_test_server()->GetURL("/title1.html").spec(),
            DlpRulesManager::Restriction::kClipboard, kRuleName1, kRuleId1,
            DlpRulesManager::Level::kReport),
        run_loop);
    GetActiveWebContents()->Paste();
    run_loop.Run();

    EXPECT_FALSE(dlp_controller_->ObserveWidget());
    EXPECT_EQ(kClipboardText1, EvalJs(GetActiveWebContents(), "p"));
  }
  // TODO(b/259179332): This EXPECT_GE is always true, because it is compared to
  // 0. The histogram sum may not have any samples when the time difference is
  // very small (almost 0), because UmaHistogramTimes requires the time
  // difference to be >= 1.
  EXPECT_GE(histogram_tester.GetTotalSum(
                data_controls::GetDlpHistogramPrefix() +
                data_controls::dlp::kDataTransferReportingTimeDiffUMA),
            0);
}

}  // namespace policy
