// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_drag_drop_notifier.h"

#include <optional>

#include "base/test/mock_callback.h"
#include "base/types/optional_util.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace policy {

namespace {
constexpr char kExampleUrl[] = "https://example.com";

ui::DataTransferEndpoint CreateEndpoint(ui::EndpointType type) {
  if (type == ui::EndpointType::kUrl)
    return ui::DataTransferEndpoint((GURL(kExampleUrl)));
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
  MOCK_METHOD3(ShowWarningBubble,
               void(const std::u16string& text,
                    base::OnceCallback<void(views::Widget*)> proceed_cb,
                    base::OnceCallback<void(views::Widget*)> cancel_cb));
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

  using DlpDragDropNotifier::CancelPressed;
  using DlpDragDropNotifier::ProceedPressed;

 private:
  base::OnceCallback<void(bool)> paste_cb_;
};

}  // namespace

class DragDropBubbleTestWithParam
    : public ::testing::TestWithParam<std::optional<ui::EndpointType>> {
 public:
  DragDropBubbleTestWithParam() = default;
  DragDropBubbleTestWithParam(const DragDropBubbleTestWithParam&) = delete;
  DragDropBubbleTestWithParam& operator=(const DragDropBubbleTestWithParam&) =
      delete;
  ~DragDropBubbleTestWithParam() override = default;
};

TEST_P(DragDropBubbleTestWithParam, NotifyBlocked) {
  ::testing::StrictMock<MockDlpDragDropNotifier> notifier;
  ui::DataTransferEndpoint data_src((GURL(kExampleUrl)));
  std::optional<ui::DataTransferEndpoint> data_dst;
  auto param = GetParam();
  if (param.has_value())
    data_dst.emplace(CreateEndpoint(param.value()));

  EXPECT_CALL(notifier, ShowBlockBubble);

  notifier.NotifyBlockedAction(&data_src, base::OptionalToPtr(data_dst));
}

TEST_P(DragDropBubbleTestWithParam, ProceedWarnOnDrop) {
  ::testing::StrictMock<MockDlpDragDropNotifier> notifier;
  ui::DataTransferEndpoint data_src((GURL(kExampleUrl)));
  std::optional<ui::DataTransferEndpoint> data_dst;
  auto param = GetParam();
  if (param.has_value())
    data_dst.emplace(CreateEndpoint(param.value()));

  EXPECT_CALL(notifier, CloseWidget(testing::_,
                                    views::Widget::ClosedReason::kUnspecified));
  EXPECT_CALL(notifier, ShowWarningBubble);

  ::testing::StrictMock<base::MockOnceClosure> callback;
  notifier.WarnOnDrop(&data_src, base::OptionalToPtr(data_dst), callback.Get());

  EXPECT_CALL(notifier,
              CloseWidget(testing::_,
                          views::Widget::ClosedReason::kAcceptButtonClicked));

  EXPECT_CALL(callback, Run());
  notifier.ProceedPressed(nullptr);
}

TEST_P(DragDropBubbleTestWithParam, CancelWarnOnDrop) {
  ::testing::StrictMock<MockDlpDragDropNotifier> notifier;
  ui::DataTransferEndpoint data_src((GURL(kExampleUrl)));
  std::optional<ui::DataTransferEndpoint> data_dst;
  auto param = GetParam();
  if (param.has_value())
    data_dst.emplace(CreateEndpoint(param.value()));

  EXPECT_CALL(notifier, CloseWidget(testing::_,
                                    views::Widget::ClosedReason::kUnspecified));
  EXPECT_CALL(notifier, ShowWarningBubble);

  ::testing::StrictMock<base::MockOnceClosure> callback;
  notifier.WarnOnDrop(&data_src, base::OptionalToPtr(data_dst), callback.Get());

  EXPECT_CALL(notifier,
              CloseWidget(testing::_,
                          views::Widget::ClosedReason::kCancelButtonClicked));

  notifier.CancelPressed(nullptr);
}

INSTANTIATE_TEST_SUITE_P(DlpDragDropNotifierTest,
                         DragDropBubbleTestWithParam,
                         ::testing::Values(std::nullopt,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                                           ui::EndpointType::kUnknownVm,
                                           ui::EndpointType::kBorealis,
                                           ui::EndpointType::kCrostini,
                                           ui::EndpointType::kPluginVm,
                                           ui::EndpointType::kArc,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
                                           ui::EndpointType::kDefault,
                                           ui::EndpointType::kUrl));

}  // namespace policy
