// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_id_generator.h"

#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace {

using SessionIdGeneratorBrowserTest = InProcessBrowserTest;

// Verify the SessionIdGenerator is Initialized.
IN_PROC_BROWSER_TEST_F(SessionIdGeneratorBrowserTest,
                       VerifySessionIdGeneratorInitialized) {
  sessions::SessionIdGenerator* generator =
      sessions::SessionIdGenerator::GetInstance();
  EXPECT_TRUE(generator->IsInitializedForTest());
}

}  // namespace