// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_data_transfer_notifier.h"

#include "ash/test/ash_test_base.h"
#include "base/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace policy {

namespace {

class MockDlpDataTransferNotifier : public DlpDataTransferNotifier {
 public:
  MockDlpDataTransferNotifier() = default;
  MockDlpDataTransferNotifier(const MockDlpDataTransferNotifier&) = delete;
  MockDlpDataTransferNotifier& operator=(const MockDlpDataTransferNotifier&) =
      delete;
  ~MockDlpDataTransferNotifier() override = default;

  // DlpDataTransferNotifier:
  void NotifyBlockedAction(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst) override {}

  using DlpDataTransferNotifier::CloseWidget;
  using DlpDataTransferNotifier::ShowBlockBubble;
  using DlpDataTransferNotifier::ShowWarningBubble;
  using DlpDataTransferNotifier::widget_;
};

}  // namespace

class DlpDataTransferNotifierTest : public ash::AshTestBase {
 public:
  DlpDataTransferNotifierTest() = default;
  ~DlpDataTransferNotifierTest() override = default;

  DlpDataTransferNotifierTest(const DlpDataTransferNotifierTest&) = delete;
  DlpDataTransferNotifierTest& operator=(const DlpDataTransferNotifierTest&) =
      delete;

 protected:
  MockDlpDataTransferNotifier notifier_;
};

TEST_F(DlpDataTransferNotifierTest, ShowBlockBubble) {
  EXPECT_FALSE(notifier_.widget_.get());
  notifier_.ShowBlockBubble(std::u16string());

  EXPECT_TRUE(notifier_.widget_.get());
  EXPECT_TRUE(notifier_.widget_->IsVisible());
  EXPECT_TRUE(notifier_.widget_->IsActive());

  notifier_.CloseWidget(notifier_.widget_.get(),
                        views::Widget::ClosedReason::kCloseButtonClicked);

  EXPECT_FALSE(notifier_.widget_.get());
}

TEST_F(DlpDataTransferNotifierTest, ShowWarningBubble) {
  EXPECT_FALSE(notifier_.widget_.get());

  notifier_.ShowWarningBubble(std::u16string(), base::DoNothing(),
                              base::DoNothing());

  EXPECT_TRUE(notifier_.widget_.get());
  EXPECT_TRUE(notifier_.widget_->IsVisible());
  EXPECT_TRUE(notifier_.widget_->IsActive());

  notifier_.CloseWidget(notifier_.widget_.get(),
                        views::Widget::ClosedReason::kAcceptButtonClicked);

  EXPECT_FALSE(notifier_.widget_.get());
}

}  // namespace policy
