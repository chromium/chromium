// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

typedef InProcessBrowserTest PreviewsServiceBrowserTest;

// Verify that the PreviewsService is initialized for a profile.
IN_PROC_BROWSER_TEST_F(PreviewsServiceBrowserTest, VerifyInitialization) {
  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(previews_service->previews_ui_service());
}
