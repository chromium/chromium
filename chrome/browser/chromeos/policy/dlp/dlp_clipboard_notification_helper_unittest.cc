// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_notification_helper.h"

#include <string>

#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace policy {

namespace {

constexpr char kUrl[] = "https://example.com";

struct ToastTest {
  ToastTest(ui::EndpointType dst_type,
            const std::string& toast_id,
            int dst_name_id)
      : dst_type(dst_type),
        expected_toast_id(toast_id),
        expected_dst_name_id(dst_name_id) {}

  const ui::EndpointType dst_type;
  const std::string expected_toast_id;
  const int expected_dst_name_id;
};

}  // namespace

class MockDlpClipboardNotificationHelper
    : public DlpClipboardNotificationHelper {
 public:
  MOCK_METHOD1(ShowClipboardBlockBubble, void(const base::string16& text));
  MOCK_METHOD2(ShowClipboardBlockToast,
               void(const std::string& id, const base::string16& text));
};

class DlpClipboardBubbleTest
    : public ::testing::TestWithParam<base::Optional<ui::EndpointType>> {};

TEST_P(DlpClipboardBubbleTest, BlockBubble) {
  ::testing::StrictMock<MockDlpClipboardNotificationHelper> notification_helper;
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

  base::string16 expected_bubble_str =
      l10n_util::GetStringFUTF16(IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_PASTE,
                                 base::UTF8ToUTF16(origin.host()));
  EXPECT_CALL(notification_helper,
              ShowClipboardBlockBubble(expected_bubble_str));

  notification_helper.NotifyBlockedPaste(&data_src,
                                         base::OptionalOrNullptr(data_dst));
}

INSTANTIATE_TEST_SUITE_P(DlpClipboard,
                         DlpClipboardBubbleTest,
                         ::testing::Values(base::nullopt,
                                           ui::EndpointType::kDefault,
                                           ui::EndpointType::kUnknownVm,
                                           ui::EndpointType::kBorealis,
                                           ui::EndpointType::kUrl));

class DlpClipboardToastTest : public ::testing::TestWithParam<ToastTest> {};

TEST_P(DlpClipboardToastTest, BlockToast) {
  ::testing::StrictMock<MockDlpClipboardNotificationHelper> notification_helper;
  url::Origin origin = url::Origin::Create(GURL(kUrl));
  ui::DataTransferEndpoint data_src(origin);
  ui::DataTransferEndpoint data_dst(GetParam().dst_type);

  base::string16 expected_toast_str = l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM,
      base::UTF8ToUTF16(origin.host()),
      l10n_util::GetStringUTF16(GetParam().expected_dst_name_id));
  EXPECT_CALL(notification_helper,
              ShowClipboardBlockToast(GetParam().expected_toast_id,
                                      expected_toast_str));

  notification_helper.NotifyBlockedPaste(&data_src, &data_dst);
}

INSTANTIATE_TEST_SUITE_P(
    DlpClipboard,
    DlpClipboardToastTest,
    ::testing::Values(ToastTest(ui::EndpointType::kCrostini,
                                "clipboard_dlp_block_crostini",
                                IDS_CROSTINI_LINUX),
                      ToastTest(ui::EndpointType::kPluginVm,
                                "clipboard_dlp_block_plugin_vm",
                                IDS_PLUGIN_VM_APP_NAME),
                      ToastTest(ui::EndpointType::kArc,
                                "clipboard_dlp_block_arc",
                                IDS_POLICY_DLP_ANDROID_APPS)));

}  // namespace policy
