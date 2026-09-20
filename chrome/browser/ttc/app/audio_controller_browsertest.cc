// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/audio_controller.h"

#include "chrome/browser/ttc/app/app_browser_test_base.h"
#include "chrome/browser/ttc/app/conversation_impl.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

class AudioControllerBrowserTest : public AppBrowserTestBase {
 public:
  AudioControllerBrowserTest() = default;
  ~AudioControllerBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(AudioControllerBrowserTest, CreatedOnSessionStart) {
  ASSERT_EQ(conversation(), nullptr);

  ttc_service().StartSession();

  ASSERT_NE(conversation(), nullptr);
  EXPECT_NE(conversation()->audio_controller(), nullptr);
}

}  // namespace ttc
