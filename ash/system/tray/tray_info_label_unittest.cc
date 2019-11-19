// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_info_label.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

class TestClickEvent : public ui::Event {
 public:
  TestClickEvent() : ui::Event(ui::ET_MOUSE_PRESSED, base::TimeTicks(), 0) {}
};

class TestDelegate : public TrayInfoLabel::Delegate {
 public:
  TestDelegate() = default;

  const std::vector<int>& clicked_message_ids() { return clicked_message_ids_; }

  void AddClickableMessageId(int message_id) {
    clickable_message_ids_.insert(message_id);
  }

  // TrayInfoLabel::Delegate:
  void OnLabelClicked(int message_id) override {
    clicked_message_ids_.push_back(message_id);
  }

  bool IsLabelClickable(int message_id) const override {
    return clickable_message_ids_.find(message_id) !=
           clickable_message_ids_.end();
  }

 private:
  std::unordered_set<int> clickable_message_ids_;
  std::vector<int> clicked_message_ids_;
};

}  // namespace

class TrayInfoLabelTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    delegate_ = std::make_unique<TestDelegate>();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    label_.reset();
    delegate_.reset();
  }

  void CreateLabel(bool use_delegate, int message_id) {
    label_ = std::make_unique<TrayInfoLabel>(
        use_delegate ? delegate_.get() : nullptr, message_id);
    use_delegate_ = use_delegate;
  }

  void ClickOnLabel(bool expect_click_was_handled) {
    bool click_was_handled = label_->PerformAction(TestClickEvent());
    EXPECT_EQ(expect_click_was_handled, click_was_handled);
  }

  void VerifyClickability(bool expected_clickable) {
    EXPECT_EQ(expected_clickable, label_->IsClickable());
    EXPECT_EQ(expected_clickable ? views::View::FocusBehavior::ALWAYS
                                 : views::View::FocusBehavior::NEVER,
              label_->GetFocusBehavior());

    ui::AXNodeData node_data;
    label_->GetAccessibleNodeData(&node_data);

    if (expected_clickable)
      EXPECT_EQ(ax::mojom::Role::kButton, node_data.role);
    else
      EXPECT_EQ(ax::mojom::Role::kLabelText, node_data.role);
  }

  void VerifyClicks(const std::vector<int>& expected_clicked_message_ids) {
    if (!use_delegate_) {
      EXPECT_TRUE(expected_clicked_message_ids.empty());
      return;
    }

    EXPECT_EQ(expected_clicked_message_ids, delegate_->clicked_message_ids());
  }

  void VerifyNoClicks() { VerifyClicks(std::vector<int>()); }

 protected:
  std::unique_ptr<TrayInfoLabel> label_;
  std::unique_ptr<TestDelegate> delegate_;
  bool use_delegate_;
};

TEST_F(TrayInfoLabelTest, NoDelegate) {
  CreateLabel(false /* use_delegate */, IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED),
            label_->GetAccessibleName());
  VerifyClickability(false /* expected_clickable */);

  label_->Update(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED),
            label_->GetAccessibleName());
  VerifyClickability(false /* expected_clickable */);

  label_->Update(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERING);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERING),
      label_->GetAccessibleName());
  VerifyClickability(false /* expected_clickable */);
}

TEST_F(TrayInfoLabelTest, PerformAction) {
  const int kClickableMessageId1 = IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED;
  const int kClickableMessageId2 = IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED;
  const int kNonClickableMessageId = IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERING;

  delegate_->AddClickableMessageId(kClickableMessageId1);
  delegate_->AddClickableMessageId(kClickableMessageId2);

  CreateLabel(true /* use_delegate */, kClickableMessageId1);
  VerifyNoClicks();

  EXPECT_EQ(l10n_util::GetStringUTF16(kClickableMessageId1),
            label_->GetAccessibleName());
  VerifyClickability(true /* expected_clickable */);
  ClickOnLabel(true /* expect_click_was_handled */);
  VerifyClicks(std::vector<int>{kClickableMessageId1});

  ClickOnLabel(true /* expect_click_was_handled */);
  VerifyClicks(std::vector<int>{kClickableMessageId1, kClickableMessageId1});

  label_->Update(kNonClickableMessageId);
  EXPECT_EQ(l10n_util::GetStringUTF16(kNonClickableMessageId),
            label_->GetAccessibleName());
  VerifyClickability(false /* expected_clickable */);
  ClickOnLabel(false /* expect_click_was_handled */);
  VerifyClicks(std::vector<int>{kClickableMessageId1, kClickableMessageId1});

  label_->Update(kClickableMessageId2);
  EXPECT_EQ(l10n_util::GetStringUTF16(kClickableMessageId2),
            label_->GetAccessibleName());
  VerifyClickability(true /* expected_clickable */);
  ClickOnLabel(true /* expect_click_was_handled */);
  VerifyClicks(std::vector<int>{kClickableMessageId1, kClickableMessageId1,
                                kClickableMessageId2});
}

}  // namespace ash
