// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/crash/core/app/client_upload_info.h"
#include "content/public/test/browser_test.h"

// This file contains tests for client_upload_info.cc within the browser app.
// The tests are not in components/crash because it can have non-browser
// embedders (e.g. installers).

using CrashClientUploadInfoTest = InProcessBrowserTest;

// A smoke test of GetClientCollectStatsConsent() to ensure that the function
// is not crashing.
IN_PROC_BROWSER_TEST_F(CrashClientUploadInfoTest, CollectStatsConsent) {
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::BindLambdaForTesting([&]() {
                               crash_reporter::GetClientCollectStatsConsent();
                               run_loop.Quit();
                             }));
  run_loop.Run();
}

// Tests that product name returned by GetClientProductInfo() is correct.
IN_PROC_BROWSER_TEST_F(CrashClientUploadInfoTest, GetClientProductInfo) {
#if BUILDFLAG(IS_CHROMEOS)
  constexpr char kProductName[] = "Chrome_ChromeOS";
#elif BUILDFLAG(IS_LINUX)
#if defined(ADDRESS_SANITIZER)
  constexpr char kProductName[] = "Chrome_Linux_ASan";
#else
  constexpr char kProductName[] = "Chrome_Linux";
#endif  // defined(ADDRESS_SANITIZER)
#elif BUILDFLAG(IS_MAC)
  constexpr char kProductName[] = "Chrome_Mac";
#elif BUILDFLAG(IS_WIN)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  constexpr char kProductName[] = "Chrome";
#else
  constexpr char kProductName[] = "Chromium";
#endif
#endif

  crash_reporter::ProductInfo product_info;
  crash_reporter::GetClientProductInfo(&product_info);
  EXPECT_EQ(product_info.product_name, kProductName);
}
