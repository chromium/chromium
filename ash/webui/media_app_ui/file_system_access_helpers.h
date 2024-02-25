// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MEDIA_APP_UI_FILE_SYSTEM_ACCESS_HELPERS_H_
#define ASH_WEBUI_MEDIA_APP_UI_FILE_SYSTEM_ACCESS_HELPERS_H_

#include "base/functional/callback.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"

namespace ash {

// Helper function to run a callback on the file represented by the provided
// transfer token.
void ResolveTransferToken(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    content::WebContents* web_contents,
    base::OnceCallback<void(std::optional<storage::FileSystemURL>)> callback);

}  // namespace ash

#endif  // ASH_WEBUI_MEDIA_APP_UI_FILE_SYSTEM_ACCESS_HELPERS_H_
