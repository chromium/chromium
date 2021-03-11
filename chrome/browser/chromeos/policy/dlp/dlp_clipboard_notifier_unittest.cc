// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_notifier.h"

#include <string>

#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/policy/dlp/clipboard_bubble.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_bubble_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace policy {

namespace {

constexpr char kUrl[] = "https://example.com";

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

}  // namespace

class MockDlpClipboardNotifier : public DlpClipboardNotifier {
 public:
  MOCK_METHOD1(ShowBlockBubble, void(const std::u16string& text));
  MOCK_METHOD3(ShowWarningBubble,
               void(const std::u16string& text,
                    base::RepeatingCallback<void(views::Widget*)> proceed_cb,
                    base::RepeatingCallback<void(views::Widget*)> cancel_cb));
  MOCK_CONST_METHOD2(ShowToast,
                     void(const std::string& id, const std::u16string& text));

  using DlpClipboardNotifier::BlinkProceedPressed;
  using DlpClipboardNotifier::CancelWarningPressed;
  using DlpClipboardNotifier::ProceedPressed;
  using DlpClipboardNotifier::ResetUserWarnSelection;
};

class DlpClipboardBubbleTest
    : public ::testing::TestWithParam<base::Optional<ui::EndpointType>> {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(DlpClipboardBubbleTest, BlockBubble) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  url::Origin origin = url::Origin::Create(GURL(kUrl));
  ui::DataTransferEndpoint data_src(origin);
  base::Optional<ui::DataTransferEndpoint> data_dst;
  auto param = GetParam();
  if (param.has_value()) {
    if (param.value() == ui::EndpointType::kUrl)
      data_dst.emplace(url::Origin::Create(GURL(kUrl)));
    else
      data_dst.emplace(param.value());
  }

  EXPECT_CALL(notifier, ShowBlockBubble);

  notifier.NotifyBlockedAction(&data_src, base::OptionalOrNullptr(data_dst));
}

TEST_P(DlpClipboardBubbleTest, WarnBubble) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  url::Origin origin = url::Origin::Create(GURL(kUrl));
  ui::DataTransferEndpoint data_src(origin);
  base::Optional<ui::DataTransferEndpoint> data_dst;
  auto param = GetParam();
  if (param.has_value() && param.value() == ui::EndpointType::kUrl)
    return;

  if (param.has_value())
    data_dst.emplace(param.value());

  EXPECT_CALL(notifier, ShowWarningBubble);

  const ui::DataTransferEndpoint* dst_ptr = base::OptionalOrNullptr(data_dst);
  notifier.WarnOnPaste(&data_src, dst_ptr);
}

TEST_P(DlpClipboardBubbleTest, ProceedPressed) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  base::Optional<ui::DataTransferEndpoint> data_dst;
  auto param = GetParam();
  // ProceedOnWarn gets called with const reference to DataTransferEndpoint.
  if (!param.has_value())
    return;

  if (param.value() == ui::EndpointType::kUrl)
    data_dst.emplace(url::Origin::Create(GURL(kUrl)));
  else
    data_dst.emplace(param.value());

  const ui::DataTransferEndpoint* dst_ptr = base::OptionalOrNullptr(data_dst);
  notifier.ProceedPressed(*dst_ptr, nullptr);
  EXPECT_TRUE(notifier.DidUserApproveDst(dst_ptr));
}

INSTANTIATE_TEST_SUITE_P(DlpClipboard,
                         DlpClipboardBubbleTest,
                         ::testing::Values(base::nullopt,
                                           ui::EndpointType::kDefault,
                                           ui::EndpointType::kUnknownVm,
                                           ui::EndpointType::kBorealis,
                                           ui::EndpointType::kUrl));

TEST_F(DlpClipboardBubbleTest, BlinkWarn) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  url::Origin origin = url::Origin::Create(GURL(kUrl));
  ui::DataTransferEndpoint data_src(origin);
  ui::DataTransferEndpoint data_dst(origin);

  EXPECT_CALL(notifier, ShowWarningBubble);

  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();
  auto web_contents = CreateTestWebContents(testing_profile.get());
  ::testing::StrictMock<base::MockOnceCallback<void(bool)>> callback;

  notifier.WarnOnBlinkPaste(&data_src, &data_dst, web_contents.get(),
                            callback.Get());

  EXPECT_CALL(callback, Run(true));
  notifier.BlinkProceedPressed(data_dst, nullptr);

  EXPECT_TRUE(notifier.DidUserApproveDst(&data_dst));
}

TEST_F(DlpClipboardBubbleTest, ProceedSavedHistory) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  const ui::DataTransferEndpoint url_dst(url::Origin::Create(GURL(kUrl)));
  const ui::DataTransferEndpoint default_dst(ui::EndpointType::kDefault);
  const ui::DataTransferEndpoint arc_dst(ui::EndpointType::kArc);
  const ui::DataTransferEndpoint crostini_dst(ui::EndpointType::kCrostini);

  notifier.ProceedPressed(url_dst, nullptr);
  notifier.ProceedPressed(default_dst, nullptr);
  notifier.ProceedPressed(arc_dst, nullptr);
  notifier.ProceedPressed(crostini_dst, nullptr);

  EXPECT_TRUE(notifier.DidUserApproveDst(&url_dst));
  EXPECT_TRUE(notifier.DidUserApproveDst(&default_dst));
  EXPECT_TRUE(notifier.DidUserApproveDst(&arc_dst));
  EXPECT_TRUE(notifier.DidUserApproveDst(&crostini_dst));

  notifier.ResetUserWarnSelection();

  EXPECT_FALSE(notifier.DidUserApproveDst(&url_dst));
  EXPECT_FALSE(notifier.DidUserApproveDst(&default_dst));
  EXPECT_FALSE(notifier.DidUserApproveDst(&arc_dst));
  EXPECT_FALSE(notifier.DidUserApproveDst(&crostini_dst));
}

TEST_F(DlpClipboardBubbleTest, CancelSavedHistory) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  const ui::DataTransferEndpoint url_dst(url::Origin::Create(GURL(kUrl)));
  const ui::DataTransferEndpoint default_dst(ui::EndpointType::kDefault);
  const ui::DataTransferEndpoint arc_dst(ui::EndpointType::kArc);
  const ui::DataTransferEndpoint crostini_dst(ui::EndpointType::kCrostini);

  notifier.CancelWarningPressed(url_dst, nullptr);
  notifier.CancelWarningPressed(default_dst, nullptr);
  notifier.CancelWarningPressed(arc_dst, nullptr);
  notifier.CancelWarningPressed(crostini_dst, nullptr);

  EXPECT_TRUE(notifier.DidUserCancelDst(&url_dst));
  EXPECT_TRUE(notifier.DidUserCancelDst(&default_dst));
  EXPECT_TRUE(notifier.DidUserCancelDst(&arc_dst));
  EXPECT_TRUE(notifier.DidUserCancelDst(&crostini_dst));

  notifier.ResetUserWarnSelection();

  EXPECT_FALSE(notifier.DidUserCancelDst(&url_dst));
  EXPECT_FALSE(notifier.DidUserCancelDst(&default_dst));
  EXPECT_FALSE(notifier.DidUserCancelDst(&arc_dst));
  EXPECT_FALSE(notifier.DidUserCancelDst(&crostini_dst));
}

class DlpClipboardToastTest : public ::testing::TestWithParam<ToastTest> {};

TEST_P(DlpClipboardToastTest, BlockToast) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  url::Origin origin = url::Origin::Create(GURL(kUrl));
  ui::DataTransferEndpoint data_src(origin);
  ui::DataTransferEndpoint data_dst(GetParam().dst_type);

  std::u16string expected_toast_str = l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM,
      base::UTF8ToUTF16(origin.host()),
      l10n_util::GetStringUTF16(GetParam().expected_dst_name_id));

  EXPECT_CALL(notifier, ShowToast(testing::_, expected_toast_str));

  notifier.NotifyBlockedAction(&data_src, &data_dst);
}

TEST_P(DlpClipboardToastTest, WarnToast) {
  ::testing::StrictMock<MockDlpClipboardNotifier> notifier;
  url::Origin origin = url::Origin::Create(GURL(kUrl));
  ui::DataTransferEndpoint data_src(origin);
  ui::DataTransferEndpoint data_dst(GetParam().dst_type);

  std::u16string expected_toast_str = l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_WARN_ON_COPY_VM,
      l10n_util::GetStringUTF16(GetParam().expected_dst_name_id));

  EXPECT_CALL(notifier, ShowToast(testing::_, expected_toast_str));

  notifier.WarnOnPaste(&data_src, &data_dst);
}

INSTANTIATE_TEST_SUITE_P(
    DlpClipboard,
    DlpClipboardToastTest,
    ::testing::Values(
        ToastTest(ui::EndpointType::kCrostini, IDS_CROSTINI_LINUX),
        ToastTest(ui::EndpointType::kPluginVm, IDS_PLUGIN_VM_APP_NAME),
        ToastTest(ui::EndpointType::kArc, IDS_POLICY_DLP_ANDROID_APPS)));

}  // namespace policy
