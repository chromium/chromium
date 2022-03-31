// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/signin_confirmation_modal.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/test_layout_provider.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace enterprise_connectors {

const std::u16string kTestTitle = base::UTF8ToUTF16(std::string("title"));
const std::u16string kTestMessage = base::UTF8ToUTF16(std::string("message"));
const std::u16string kTestCancel = base::UTF8ToUTF16(std::string("cancel"));
const std::u16string kTestAccept = base::UTF8ToUTF16(std::string("accept"));

class FileSystemConfirmationModalTest : public testing::Test {
 public:
  void SetUp() override {
    modal = std::unique_ptr<FileSystemConfirmationModal>(
        new FileSystemConfirmationModal(
            kTestTitle, kTestMessage, kTestCancel, kTestAccept,
            base::BindOnce(&FileSystemConfirmationModalTest::OnUserClick,
                           base::Unretained(this))));
  }

  std::unique_ptr<FileSystemConfirmationModal> modal;
  bool callback_ran = false;
  bool callback_user_accepted = false;

 private:
  void OnUserClick(bool user_accepted) {
    callback_ran = true;
    callback_user_accepted = user_accepted;
  }

  // For GetContentsView() which needs views::layout::GetFont().
  views::test::TestLayoutProvider layout_provider;
};

TEST_F(FileSystemConfirmationModalTest, GetUIProperties) {
  ASSERT_TRUE(modal->ShouldShowWindowTitle());
  ASSERT_TRUE(modal->ShouldShowWindowIcon());
  ASSERT_FALSE(modal->ShouldShowCloseButton());

  ASSERT_EQ(modal->GetWindowTitle(), kTestTitle);
  ASSERT_EQ(modal->GetAccessibleWindowTitle(), kTestTitle);
  ASSERT_EQ(modal->GetModalType(), ui::MODAL_TYPE_WINDOW);

  auto icon = modal->GetWindowIcon();
  ASSERT_FALSE(icon.IsEmpty());

  views::View* message_view = modal->GetContentsView();
  ASSERT_NE(message_view, nullptr);
  // Check on the message text.
  views::Label* message_label = static_cast<views::Label*>(message_view);
  ASSERT_EQ(message_label->GetText(), kTestMessage);
}

TEST_F(FileSystemConfirmationModalTest, ClickAccept) {
  modal->Accept();
  ASSERT_TRUE(callback_ran);
  ASSERT_TRUE(callback_user_accepted);
}

TEST_F(FileSystemConfirmationModalTest, ClickCancel) {
  modal->Cancel();
  ASSERT_TRUE(callback_ran);
  ASSERT_FALSE(callback_user_accepted);
}

}  // namespace enterprise_connectors
