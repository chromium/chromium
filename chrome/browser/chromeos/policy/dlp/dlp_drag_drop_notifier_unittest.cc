// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_drag_drop_notifier.h"

#include "base/optional.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace policy {

namespace {
constexpr char kExampleUrl[] = "https://example.com";

ui::DataTransferEndpoint CreateEndpoint(ui::EndpointType type) {
  if (type == ui::EndpointType::kUrl)
    return ui::DataTransferEndpoint(url::Origin::Create(GURL(kExampleUrl)));
  else
    return ui::DataTransferEndpoint(type);
}

class MockDlpDragDropNotifier : public DlpDragDropNotifier {
 public:
  MockDlpDragDropNotifier() = default;
  MockDlpDragDropNotifier(const MockDlpDragDropNotifier&) = delete;
  MockDlpDragDropNotifier& operator=(const MockDlpDragDropNotifier&) = delete;
  ~MockDlpDragDropNotifier() override = default;

  // DlpDataTransferNotifier:
  MOCK_METHOD1(ShowBlockBubble, void(const std::u16string& text));
};

}  // namespace

class DragDropBubbleTestWithParam
    : public ::testing::TestWithParam<base::Optional<ui::EndpointType>> {
 public:
  DragDropBubbleTestWithParam() = default;
  DragDropBubbleTestWithParam(const DragDropBubbleTestWithParam&) = delete;
  DragDropBubbleTestWithParam& operator=(const DragDropBubbleTestWithParam&) =
      delete;
  ~DragDropBubbleTestWithParam() override = default;
};

TEST_P(DragDropBubbleTestWithParam, NotifyBlocked) {
  ::testing::StrictMock<MockDlpDragDropNotifier> notifier;
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kExampleUrl)));
  base::Optional<ui::DataTransferEndpoint> data_dst;
  auto param = GetParam();
  if (param.has_value())
    data_dst.emplace(CreateEndpoint(param.value()));

  EXPECT_CALL(notifier, ShowBlockBubble);

  notifier.NotifyBlockedAction(&data_src, base::OptionalOrNullptr(data_dst));
}

INSTANTIATE_TEST_SUITE_P(DlpDragDropNotifierTest,
                         DragDropBubbleTestWithParam,
                         ::testing::Values(base::nullopt,
                                           ui::EndpointType::kDefault,
                                           ui::EndpointType::kUnknownVm,
                                           ui::EndpointType::kBorealis,
                                           ui::EndpointType::kUrl,
                                           ui::EndpointType::kCrostini,
                                           ui::EndpointType::kPluginVm,
                                           ui::EndpointType::kArc));

}  // namespace policy
