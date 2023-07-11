// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {

namespace {

using ReadingListApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(ReadingListApiTest, TestReadingListWorks) {
  ASSERT_TRUE(RunExtensionTest("reading_list")) << message_;
}

}  // namespace

}  // namespace extensions
