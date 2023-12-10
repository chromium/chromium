// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/media_app_ui/file_system_access_helpers.h"

#include <utility>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/file_system_access_entry_factory.h"
#include "content/public/browser/storage_partition.h"

namespace ash {

void ResolveTransferToken(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    content::WebContents* web_contents,
    base::OnceCallback<void(std::optional<storage::FileSystemURL>)> callback) {
  web_contents->GetBrowserContext()
      ->GetStoragePartition(web_contents->GetSiteInstance())
      ->GetFileSystemAccessEntryFactory()
      ->ResolveTransferToken(std::move(token), std::move(callback));
}

}  // namespace ash
