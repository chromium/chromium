// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/test_browser_context.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"

namespace arc {

TestBrowserContext::TestBrowserContext()
    : browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()) {
  user_prefs::UserPrefs::Set(this, &prefs_);
}

TestBrowserContext::~TestBrowserContext() {
  browser_context_dependency_manager_->DestroyBrowserContextServices(this);
}

}  // namespace arc
