// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/keep_alive/profile_keep_alive_waiter.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"

ProfileKeepAliveAddedWaiter::ProfileKeepAliveAddedWaiter(
    Profile* observed_profile,
    ProfileKeepAliveOrigin observed_origin)
    : observed_profile_(observed_profile), observed_origin_(observed_origin) {
  g_browser_process->profile_manager()->AddObserver(this);
}

ProfileKeepAliveAddedWaiter::~ProfileKeepAliveAddedWaiter() {
  g_browser_process->profile_manager()->RemoveObserver(this);
}

void ProfileKeepAliveAddedWaiter::Wait() {
  run_loop_.Run();
}

void ProfileKeepAliveAddedWaiter::OnKeepAliveAdded(
    const Profile* profile,
    ProfileKeepAliveOrigin origin) {
  if (profile == observed_profile_ && origin == observed_origin_) {
    run_loop_.Quit();
  }
}
