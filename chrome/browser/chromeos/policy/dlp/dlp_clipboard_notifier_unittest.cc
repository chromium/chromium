// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_notifier.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/types/optional_util.h"
#include "chrome/browser/chromeos/policy/dlp/clipboard_bubble.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_bubble_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace policy {

namespace {

constexpr char kExampleUrl[] = "https://example.com";
constexpr char kExample2Url[] = "https://example2.com";
constexpr char kExample3Url[] = "https://example3.com";

struct ToastTest {
  ToastTest(ui::EndpointType dst_type, int dst_name_id)
      : dst_type(dst_type), expected_dst_name_id(dst_name_id) {}

  const ui::EndpointType dst_type;
  const int expected_dst_name_id;
};

std::unique_ptr<content::WebContents> CreateTestWebContents(
    content::BrowserContext* browser_context) {
  auto site_instance = content::SiteInstance::Create(browser_context);
  return content::WebContentsTester::CreateTestWebContents(
      browser_context, std::move(site_instance));
}

ui::DataTransferEndpoint CreateEndpoint(ui::EndpointType type) {
  if (type == ui::EndpointType::kUrl)
    return ui::DataTransferEndpoint((GURL(kExampleUrl)));
  else
    return ui::DataTransferEndpoint(type);
}

class MockDlpClipboardNotifier : public DlpClipboardNotifier {
 public:
  MockDlpClipboardNotifier() = default;
  MockDlpClipboardNotifier(const MockDlpClipboardNotifier&) = delete;
  MockDlpClipboardNotifier& operator=(const MockDlpClipboardNotifier&) = delete;
  ~MockDlpClipboardNotifier() override = default;

  // DlpDataTransferNotifier:
  MOCK_METHOD1(ShowBlockBubble, void(const std::u16string& text));
  MOCK_METHOD3(ShowWarningBubble,
               void(const std::u16string& text,
                    base::OnceCallback<void(views::Widget*)> proceed_cb,
                    base::OnceCallback<void(views::Widget*)> cancel_cb));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_CONST_METHOD3(ShowToast,
                     void(const std::string& id,
                          ash::ToastCatalogName catalog_name,
                          const std::u16string& text));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD2(CloseWidget,
               void(MayBeDangling<views::Widget> widget,
                    views::Widget::ClosedReason reason));

  void SetPasteCallback(base::OnceCallback<void(bool)> paste_cb) override {
    paste_cb_ = std::move(paste_cb);
  }

  void RunPasteCallback() override {
    DCHECK(paste_cb_);
    std::move(paste_cb_).Run(true);
  }

  using DlpClipboardNotifier::BlinkProceedPressed;
  using DlpClipboardNotifier::CancelWarningPressed;
  using DlpClipboardNotifier::ProceedPressed;
  using DlpClipboardNotifier::ResetUserWarnSelection;

 private:
  base::OnceCallback<void(bool)> paste_cb_;
};

}  // namespace

class ClipboardBubbleTestWithParam
    : public ::testing::TestWithParam<std::optional<ui::EndpointType>> {
 public:
  ClipboardBubbleTestWithParam() = default;
  ClipboardBubbleTestWithParam(const ClipboardBubbleTestWithParam&) = delete;
  ClipboardBubbleTestWithParam& operator=(const ClipboardBubbleTestWithParam&) =
      delete;
  ~ClipboardBubbleTestWithParam() override = default;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT};
};

TEST_P(ClipboardBubbleTestWithParam, BlockBubble) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  ui::DataTransferEndpoint data_src((GURL(kExampleUrl)));
  std::optional<ui::DataTransferEndpoint> data_dst;
  auto param = GetParam();
  if (param.has_value())
    data_dst.emplace(CreateEndpoint(param.value()));

  EXPECT_CALL(notifier, ShowBlockBubble);

  notifier.NotifyBlockedAction(&data_src, base::OptionalToPtr(data_dst));
}

TEST_P(ClipboardBubbleTestWithParam, WarnBubble) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  ui::DataTransferEndpoint data_src((GURL(kExampleUrl)));
  std::optional<ui::DataTransferEndpoint> data_dst;
  auto param = GetParam();
  if (param.has_value())
    data_dst.emplace(CreateEndpoint(param.value()));

  EXPECT_CALL(notifier, CloseWidget(testing::_,
                                    views::Widget::ClosedReason::kUnspecified));
  EXPECT_CALL(notifier, ShowWarningBubble);

  notifier.WarnOnPaste(data_src, data_dst, base::DoNothing());
}

INSTANTIATE_TEST_SUITE_P(DlpClipboardNotifierTest,
                         ClipboardBubbleTestWithParam,
                         ::testing::Values(std::nullopt,
                                           ui::EndpointType::kDefault,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                                           ui::EndpointType::kUnknownVm,
                                           ui::EndpointType::kBorealis,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
                                           ui::EndpointType::kUrl));

class ClipboardBubbleButtonsTestWithParam
    : public ::testing::TestWithParam<ui::EndpointType> {
 public:
  ClipboardBubbleButtonsTestWithParam() = default;
  ClipboardBubbleButtonsTestWithParam(
      const ClipboardBubbleButtonsTestWithParam&) = delete;
  ClipboardBubbleButtonsTestWithParam& operator=(
      const ClipboardBubbleButtonsTestWithParam&) = delete;
  ~ClipboardBubbleButtonsTestWithParam() override = default;
};

TEST_P(ClipboardBubbleButtonsTestWithParam, ProceedPressed) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  ui::DataTransferEndpoint data_dst(CreateEndpoint(GetParam()));

  EXPECT_CALL(notifier,
              CloseWidget(testing::_,
                          views::Widget::ClosedReason::kAcceptButtonClicked));

  notifier.ProceedPressed(std::make_unique<ui::ClipboardData>(), data_dst,
                          base::DoNothing(), nullptr);

  EXPECT_TRUE(notifier.DidUserApproveDst(&data_dst));
}

TEST_P(ClipboardBubbleButtonsTestWithParam, CancelPressed) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  ui::DataTransferEndpoint data_dst(CreateEndpoint(GetParam()));

  EXPECT_CALL(notifier,
              CloseWidget(testing::_,
                          views::Widget::ClosedReason::kCancelButtonClicked));

  notifier.CancelWarningPressed(data_dst, nullptr);

  EXPECT_TRUE(notifier.DidUserCancelDst(&data_dst));
}

INSTANTIATE_TEST_SUITE_P(DlpClipboardNotifierTest,
                         ClipboardBubbleButtonsTestWithParam,
                         ::testing::Values(ui::EndpointType::kDefault,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                                           ui::EndpointType::kUnknownVm,
                                           ui::EndpointType::kBorealis,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
                                           ui::EndpointType::kUrl));

class DlpClipboardNotifierTest : public testing::Test {
 public:
  DlpClipboardNotifierTest() = default;
  DlpClipboardNotifierTest(const DlpClipboardNotifierTest&) = delete;
  DlpClipboardNotifierTest& operator=(const DlpClipboardNotifierTest&) = delete;
  ~DlpClipboardNotifierTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
};

TEST_F(DlpClipboardNotifierTest, BlinkWarn) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  GURL url = GURL(kExampleUrl);
  ui::DataTransferEndpoint data_src(url);
  ui::DataTransferEndpoint data_dst(url);

  EXPECT_CALL(notifier, CloseWidget(testing::_,
                                    views::Widget::ClosedReason::kUnspecified));
  EXPECT_CALL(notifier, ShowWarningBubble);

  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContents(testing_profile.get());
  ::testing::StrictMock<base::MockOnceCallback<void(bool)>> callback;

  notifier.WarnOnBlinkPaste(&data_src, &data_dst, web_contents.get(),
                            callback.Get());

  testing::Mock::VerifyAndClearExpectations(&notifier);

  EXPECT_CALL(notifier, CloseWidget(testing::_,
                                    views::Widget::ClosedReason::kUnspecified));
  web_contents.reset();
}

TEST_F(DlpClipboardNotifierTest, BlinkProceedSavedHistory) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  const ui::DataTransferEndpoint url_dst1((GURL(kExampleUrl)));
  const ui::DataTransferEndpoint url_dst2((GURL(kExample2Url)));
  const ui::DataTransferEndpoint url_dst3((GURL(kExample3Url)));

  ::testing::StrictMock<base::MockOnceCallback<void(bool)>> callback;

  EXPECT_CALL(notifier,
              CloseWidget(testing::_,
                          views::Widget::ClosedReason::kAcceptButtonClicked))
      .Times(3);

  notifier.SetPasteCallback(callback.Get());
  EXPECT_CALL(callback, Run(true));
  notifier.BlinkProceedPressed(url_dst1, nullptr);

  notifier.SetPasteCallback(callback.Get());
  EXPECT_CALL(callback, Run(true));
  notifier.BlinkProceedPressed(url_dst2, nullptr);

  notifier.SetPasteCallback(callback.Get());
  EXPECT_CALL(callback, Run(true));
  notifier.BlinkProceedPressed(url_dst3, nullptr);

  testing::Mock::VerifyAndClearExpectations(&notifier);

  EXPECT_TRUE(notifier.DidUserApproveDst(&url_dst1));
  EXPECT_TRUE(notifier.DidUserApproveDst(&url_dst2));
  EXPECT_TRUE(notifier.DidUserApproveDst(&url_dst3));

  notifier.ResetUserWarnSelection();

  EXPECT_FALSE(notifier.DidUserApproveDst(&url_dst1));
  EXPECT_FALSE(notifier.DidUserApproveDst(&url_dst2));
  EXPECT_FALSE(notifier.DidUserApproveDst(&url_dst3));
}

TEST_F(DlpClipboardNotifierTest, ProceedSavedHistory) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  const ui::DataTransferEndpoint url_dst((GURL(kExampleUrl)));
  const ui::DataTransferEndpoint default_dst(ui::EndpointType::kDefault);

  EXPECT_CALL(notifier,
              CloseWidget(testing::_,
                          views::Widget::ClosedReason::kAcceptButtonClicked))
      .Times(2);

  notifier.ProceedPressed(std::make_unique<ui::ClipboardData>(), url_dst,
                          base::DoNothing(), nullptr);
  notifier.ProceedPressed(std::make_unique<ui::ClipboardData>(), default_dst,
                          base::DoNothing(), nullptr);

  EXPECT_TRUE(notifier.DidUserApproveDst(&url_dst));
  EXPECT_TRUE(notifier.DidUserApproveDst(&default_dst));

  notifier.ResetUserWarnSelection();

  EXPECT_FALSE(notifier.DidUserApproveDst(&url_dst));
  EXPECT_FALSE(notifier.DidUserApproveDst(&default_dst));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(DlpClipboardNotifierTest, ProceedSavedHistoryVMs) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  const ui::DataTransferEndpoint arc_dst(ui::EndpointType::kArc);
  const ui::DataTransferEndpoint crostini_dst(ui::EndpointType::kCrostini);

  EXPECT_CALL(notifier,
              CloseWidget(testing::_,
                          views::Widget::ClosedReason::kAcceptButtonClicked))
      .Times(2);

  notifier.ProceedPressed(std::make_unique<ui::ClipboardData>(), arc_dst,
                          base::DoNothing(), nullptr);
  notifier.ProceedPressed(std::make_unique<ui::ClipboardData>(), crostini_dst,
                          base::DoNothing(), nullptr);

  EXPECT_TRUE(notifier.DidUserApproveDst(&arc_dst));
  EXPECT_TRUE(notifier.DidUserApproveDst(&crostini_dst));

  notifier.ResetUserWarnSelection();

  EXPECT_FALSE(notifier.DidUserApproveDst(&arc_dst));
  EXPECT_FALSE(notifier.DidUserApproveDst(&crostini_dst));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(DlpClipboardNotifierTest, CancelSavedHistory) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  const ui::DataTransferEndpoint url_dst((GURL(kExampleUrl)));
  const ui::DataTransferEndpoint default_dst(ui::EndpointType::kDefault);

  EXPECT_CALL(notifier,
              CloseWidget(testing::_,
                          views::Widget::ClosedReason::kCancelButtonClicked))
      .Times(2);

  notifier.CancelWarningPressed(url_dst, nullptr);
  notifier.CancelWarningPressed(default_dst, nullptr);

  EXPECT_TRUE(notifier.DidUserCancelDst(&url_dst));
  EXPECT_TRUE(notifier.DidUserCancelDst(&default_dst));

  notifier.ResetUserWarnSelection();

  EXPECT_FALSE(notifier.DidUserCancelDst(&url_dst));
  EXPECT_FALSE(notifier.DidUserCancelDst(&default_dst));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(DlpClipboardNotifierTest, CancelSavedHistoryVMs) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  const ui::DataTransferEndpoint arc_dst(ui::EndpointType::kArc);
  const ui::DataTransferEndpoint crostini_dst(ui::EndpointType::kCrostini);

  EXPECT_CALL(notifier,
              CloseWidget(testing::_,
                          views::Widget::ClosedReason::kCancelButtonClicked))
      .Times(2);

  notifier.CancelWarningPressed(arc_dst, nullptr);
  notifier.CancelWarningPressed(crostini_dst, nullptr);

  EXPECT_TRUE(notifier.DidUserCancelDst(&arc_dst));
  EXPECT_TRUE(notifier.DidUserCancelDst(&crostini_dst));

  notifier.ResetUserWarnSelection();

  EXPECT_FALSE(notifier.DidUserCancelDst(&arc_dst));
  EXPECT_FALSE(notifier.DidUserCancelDst(&crostini_dst));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ToastTestWithParam : public ::testing::TestWithParam<ToastTest> {
 public:
  ToastTestWithParam() = default;
  ToastTestWithParam(const ToastTestWithParam&) = delete;
  ToastTestWithParam& operator=(const ToastTestWithParam&) = delete;
  ~ToastTestWithParam() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(ToastTestWithParam, BlockToast) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  GURL url = GURL(kExampleUrl);
  ui::DataTransferEndpoint data_src(url);
  ui::DataTransferEndpoint data_dst(GetParam().dst_type);

  std::u16string expected_toast_str = l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM,
      base::UTF8ToUTF16(url.host()),
      l10n_util::GetStringUTF16(GetParam().expected_dst_name_id));

  EXPECT_CALL(
      notifier,
      ShowToast(testing::_, ash::ToastCatalogName::kClipboardBlockedAction,
                expected_toast_str));

  notifier.NotifyBlockedAction(&data_src, &data_dst);
}

TEST_P(ToastTestWithParam, WarnToast) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  GURL url = GURL(kExampleUrl);
  ui::DataTransferEndpoint data_src(url);
  ui::DataTransferEndpoint data_dst(GetParam().dst_type);

  std::u16string expected_toast_str = l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_WARN_ON_COPY_VM,
      l10n_util::GetStringUTF16(GetParam().expected_dst_name_id));

  EXPECT_CALL(notifier, ShowToast(testing::_,
                                  ash::ToastCatalogName::kClipboardWarnOnPaste,
                                  expected_toast_str));

  EXPECT_CALL(notifier, CloseWidget(testing::_,
                                    views::Widget::ClosedReason::kUnspecified));
  notifier.WarnOnPaste(data_src, data_dst, base::DoNothing());
}

INSTANTIATE_TEST_SUITE_P(
    DlpClipboardNotifierTest,
    ToastTestWithParam,
    ::testing::Values(
        ToastTest(ui::EndpointType::kCrostini, IDS_CROSTINI_LINUX),
        ToastTest(ui::EndpointType::kPluginVm, IDS_PLUGIN_VM_APP_NAME),
        ToastTest(ui::EndpointType::kArc, IDS_POLICY_DLP_ANDROID_APPS)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace policy
