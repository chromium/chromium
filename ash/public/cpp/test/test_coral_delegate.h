// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_CORAL_DELEGATE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_CORAL_DELEGATE_H_

#include "ash/public/cpp/coral_delegate.h"

namespace ash {

// A `CoralDelegate` that does nothing. Used by `TestShellDelegate`.
class TestCoralDelegate : public CoralDelegate {
 public:
  TestCoralDelegate();
  TestCoralDelegate(const TestCoralDelegate&) = delete;
  TestCoralDelegate& operator=(const TestCoralDelegate&) = delete;
  ~TestCoralDelegate() override;

  // CoralDelegate:
  void LaunchPostLoginGroup(coral::mojom::GroupPtr group) override;
  void MoveTabsInGroupToNewDesk(const std::vector<coral::mojom::Tab>& tabs,
                                size_t src_desk_index) override;
  int GetChromeDefaultRestoreId() override;
  void OpenFeedbackDialog(
      const std::string& group_description,
      ScannerDelegate::SendFeedbackCallback send_feedback_callback) override;
  void CheckGenAIAgeAvailability(GenAIInquiryCallback callback) override;
  bool GetGenAILocationAvailability() override;
  std::string GetSystemLanguage() override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_CORAL_DELEGATE_H_
