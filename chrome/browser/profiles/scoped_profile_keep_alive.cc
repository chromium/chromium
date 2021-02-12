// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/scoped_profile_keep_alive.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"

ScopedProfileKeepAlive::ScopedProfileKeepAlive(const Profile* profile,
                                               ProfileKeepAliveOrigin origin)
    : profile_(profile), origin_(origin) {
  DCHECK(profile_);
  // |profile_manager| can be nullptr in tests.
  auto* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager->AddKeepAlive(profile_, origin_);
}

ScopedProfileKeepAlive::~ScopedProfileKeepAlive() {
  // |profile_manager| could be nullptr if this is called during shutdown, e.g.
  // for system/guest profiles.
  auto* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager->RemoveKeepAlive(profile_, origin_);
}
