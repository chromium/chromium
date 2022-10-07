// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_dialog.h"

#include <string>

#include "hats_dialog.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(HatsDialogTest, HandleClientTriggeredAction) {
  // Client asks to close the window
  EXPECT_TRUE(HatsDialog::HandleClientTriggeredAction("close"));
  // There was an unhandled error, close the window
  EXPECT_TRUE(
      HatsDialog::HandleClientTriggeredAction("survey-loading-error-12345"));
  // Client sent an invalid action, ignore it
  EXPECT_FALSE(HatsDialog::HandleClientTriggeredAction("Invalid"));

  // Client sent a valid action
  EXPECT_FALSE(HatsDialog::HandleClientTriggeredAction("smiley-selected-4"));
}

}  // namespace ash
