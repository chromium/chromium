// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PROFILES_BROWSER_CONTEXT_HELPER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_PROFILES_BROWSER_CONTEXT_HELPER_DELEGATE_IMPL_H_

#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"

namespace ash {

// Injects chrome/browser dependency to BrowserContextHelper.
// TODO(crbug.com/40225390): Remove g_browser_process dependency from this
// implementation, which requires to change the lifetime of the instance.
class BrowserContextHelperDelegateImpl : public BrowserContextHelper::Delegate {
 public:
  BrowserContextHelperDelegateImpl();
  ~BrowserContextHelperDelegateImpl() override;

  // BrowserContextHelper::Delegate overrides
  content::BrowserContext* GetBrowserContextByPath(
      const base::FilePath& path) override;
  content::BrowserContext* GetBrowserContextByAccountId(
      const AccountId& account_id) override;
  content::BrowserContext* DeprecatedGetBrowserContext(
      const base::FilePath& path) override;
  content::BrowserContext* GetOrCreatePrimaryOTRBrowserContext(
      content::BrowserContext* browser_context) override;
  content::BrowserContext* GetOriginalBrowserContext(
      content::BrowserContext* browser_context) override;
  const base::FilePath* GetUserDataDir() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PROFILES_BROWSER_CONTEXT_HELPER_DELEGATE_IMPL_H_
