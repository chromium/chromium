// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/diagnostics/diagnostics_browser_delegate_impl.h"

#include "base/files/file_path.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace diagnostics {

base::FilePath DiagnosticsBrowserDelegateImpl::GetActiveUserProfileDir() {
  const session_manager::Session* session =
      session_manager::SessionManager::Get()->GetActiveSession();
  // Handle no user logged in.
  if (!session) {
    return base::FilePath();
  }

  content::BrowserContext* browser_context =
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          session->account_id());

  // Profile may be null if called before profile load is complete.
  if (!browser_context) {
    return base::FilePath();
  }

  return browser_context->GetPath();
}

}  // namespace diagnostics
}  // namespace ash
