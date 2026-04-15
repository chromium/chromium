// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "extensions/buildflags/buildflags.h"

// The feedbackPrivate API is not supported on desktop Android.
static_assert(BUILDFLAG(ENABLE_EXTENSIONS));

namespace extensions {

using FeedbackPrivateApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(FeedbackPrivateApiTest, Basic) {
  EXPECT_TRUE(RunExtensionTest("feedback_private/basic")) << message_;
}

}  // namespace extensions
