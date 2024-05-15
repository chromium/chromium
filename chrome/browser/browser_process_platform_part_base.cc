// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_base.h"

#include "base/notreached.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#endif

BrowserProcessPlatformPartBase::BrowserProcessPlatformPartBase() {
}

BrowserProcessPlatformPartBase::~BrowserProcessPlatformPartBase() {
}

void BrowserProcessPlatformPartBase::StartTearDown() {
}

void BrowserProcessPlatformPartBase::AttemptExit(bool try_to_quit_application) {
// chrome::CloseAllBrowsers() doesn't link on OS_ANDROID, but it overrides this
// method already.
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#else
  // On most platforms, closing all windows causes the application to exit.
  chrome::CloseAllBrowsers();
#endif
}

void BrowserProcessPlatformPartBase::PreMainMessageLoopRun() {
}
