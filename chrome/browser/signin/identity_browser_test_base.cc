// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_browser_test_base.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"  // nogncheck
#include "content/public/browser/browser_main_parts.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class IdentityExtraSetUp : public ChromeBrowserMainExtraParts {
 public:
  void PreProfileInit() override {
    // Create and initialize Ash AccountManager.
    CreateAshAccountManagerForTests();
    auto* account_manager = MaybeGetAshAccountManagerForTests();
    CHECK(account_manager);
    account_manager->InitializeInEphemeralMode(
        g_browser_process->system_network_context_manager()
            ->GetSharedURLLoaderFactory());
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

void IdentityBrowserTestBase::CreatedBrowserMainParts(
    content::BrowserMainParts* parts) {
  InProcessBrowserTest::CreatedBrowserMainParts(parts);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
      std::make_unique<IdentityExtraSetUp>());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}
