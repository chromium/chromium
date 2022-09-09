// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_signin_delegate.h"

#include <memory>

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sync/test/integration/sync_signin_delegate_android.h"
#else
#include "chrome/browser/sync/test/integration/sync_signin_delegate_desktop.h"
#endif

std::unique_ptr<SyncSigninDelegate> CreateSyncSigninDelegate() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<SyncSigninDelegateAndroid>();
#else
  return std::make_unique<SyncSigninDelegateDesktop>();
#endif
}
