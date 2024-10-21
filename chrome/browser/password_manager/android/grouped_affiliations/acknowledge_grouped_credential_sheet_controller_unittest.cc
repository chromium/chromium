// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_controller.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class GroupedCredentialAcknowledgeSheetControllerTest : public testing::Test {
 public:
  AcknowledgeGroupedCredentialSheetController* GetController() {
    return controller_.get();
  }

 private:
  std::unique_ptr<AcknowledgeGroupedCredentialSheetController> controller_ =
      std::make_unique<AcknowledgeGroupedCredentialSheetController>();
};

TEST_F(GroupedCredentialAcknowledgeSheetControllerTest,
       ShowAcknowledgeSheetDeclined) {
  // TODO(crbug.com/372635361): After implementing the bridge, expect the call
  // to show the actual sheet. Now only checks that the callback is called.
  base::MockCallback<base::OnceCallback<void(bool)>> mock_reply;
  EXPECT_CALL(mock_reply, Run(false));
  GetController()->ShowAcknowledgeSheet(mock_reply.Get());
}
