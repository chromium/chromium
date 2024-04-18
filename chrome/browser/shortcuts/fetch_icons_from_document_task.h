// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_FETCH_ICONS_FROM_DOCUMENT_TASK_H_
#define CHROME_BROWSER_SHORTCUTS_FETCH_ICONS_FROM_DOCUMENT_TASK_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "components/webapps/common/web_page_metadata.mojom-forward.h"
#include "components/webapps/common/web_page_metadata_agent.mojom-forward.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

class SkBitmap;
class GURL;

namespace base {
class Location;
}

namespace content {
class RenderFrameHost;
}

namespace gfx {
class Size;
}

namespace shortcuts {
class DocumentIconFetcher;

enum class FetchIconsForDocumentError {
  kDocumentDestroyed,
  kMetadataFetchFailed
};
using FetchIconsFromDocumentResult =
    base::expected<std::vector<SkBitmap>, FetchIconsForDocumentError>;
using FetchIconsFromDocumentCallback =
    base::OnceCallback<void(FetchIconsFromDocumentResult)>;

// Fetch all icons for the current RenderFrameHost document.
// Invariants:
// - The `frame_host_` provided to this class must be alive for the lifetime of
//   this class.
// - The `callback` may be called synchronously.
// - The `callback` will be NOT be called on destruction. To handle that, the
// user
//   of this class must use the `TakeCallback()` method to extract the callback.
class FetchIconsFromDocumentTask {
 public:
  using Result = FetchIconsFromDocumentResult;

  // `rfh` must be the primary main frame.
  FetchIconsFromDocumentTask(base::PassKey<DocumentIconFetcher>,
                             content::RenderFrameHost& rfh);
  ~FetchIconsFromDocumentTask();

  // The `callback` may be called synchronously.
  void Start(FetchIconsFromDocumentCallback callback);

  FetchIconsFromDocumentCallback TakeCallback();

 private:
  void OnWebPageMetadataObtained(
      mojo::AssociatedRemote<webapps::mojom::WebPageMetadataAgent>
          metadata_agent,
      webapps::mojom::WebPageMetadataPtr web_page_metadata);

  void DownloadIcon(const GURL& url);

  void DidDownloadFavicon(int id,
                          int http_status_code,
                          const GURL& image_url,
                          const std::vector<SkBitmap>& bitmaps,
                          const std::vector<gfx::Size>& sizes);

  void MaybeCompleteImageDownloadAndSelfDestruct();

  void OnCompleteSelfDestruct(Result result, base::Location here = FROM_HERE);

  void OnMetadataFetchError();

  base::raw_ref<content::RenderFrameHost> frame_host_;
  FetchIconsFromDocumentCallback callback_;

  bool metadata_fetch_complete_ = false;
  int num_pending_image_requests_ = 0;
  std::vector<SkBitmap> icons_;

  base::WeakPtrFactory<FetchIconsFromDocumentTask> weak_factory_{this};
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_FETCH_ICONS_FROM_DOCUMENT_TASK_H_
