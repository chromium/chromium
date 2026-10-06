// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fwupd/fwupd_download_client_impl.h"

#include "base/check_is_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

FwupdDownloadClientImpl::FwupdDownloadClientImpl() = default;
FwupdDownloadClientImpl::~FwupdDownloadClientImpl() = default;

scoped_refptr<network::SharedURLLoaderFactory>
FwupdDownloadClientImpl::GetURLLoaderFactory() {
  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const session_manager::Session* session =
      session_manager::SessionManager::Get()->GetActiveSession();

  content::BrowserContext* browser_context =
      session ? BrowserContextHelper::Get()->GetBrowserContextByAccountId(
                    session->account_id())
              : nullptr;

  // Active user and profile might not be initialized in some tests
  if (!browser_context) {
    CHECK_IS_TEST();
    return nullptr;
  }

  return browser_context->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

}  // namespace ash
