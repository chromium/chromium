// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_DOCUMENT_ICON_FETCHER_TASK_H_
#define CHROME_BROWSER_SHORTCUTS_DOCUMENT_ICON_FETCHER_TASK_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/webapps/common/web_page_metadata.mojom-forward.h"
#include "components/webapps/common/web_page_metadata_agent.mojom-forward.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class WebContents;
}  // namespace content

class GURL;
class SkBitmap;

namespace gfx {
class Size;
}

namespace shortcuts {

enum class FetchIconsForDocumentError { kTaskDestroyed, kMetadataFetchFailed };

using FetchIconsFromDocumentResult =
    base::expected<std::vector<SkBitmap>, FetchIconsForDocumentError>;

using FetchIconsFromDocumentCallback =
    base::OnceCallback<void(FetchIconsFromDocumentResult)>;

// This object is responsible for fetching all available icons from a given
// document.
class DocumentIconFetcherTask {
 public:
  // Creates a task that fetches all icons for the top level primary frame of
  // the given web contents. `callback` will always be called (even on document
  // destruction), and always called asynchronously. If the callback is not
  // called with an error, it is guaranteed to include at least one icon (i.e.
  // it is not possible for fetching to succeed but not return any icons. If no
  // icons were found, a fallback icon is generated).
  DocumentIconFetcherTask(content::WebContents& web_contents,
                          FetchIconsFromDocumentCallback callback);
  ~DocumentIconFetcherTask();

  void StartIconFetching();

 private:
  void OnMetadataFetchError();

  void OnWebPageMetadataObtained(
      mojo::AssociatedRemote<webapps::mojom::WebPageMetadataAgent>
          metadata_agent,
      webapps::mojom::WebPageMetadataPtr web_page_metadata);

  void DidDownloadFavicon(int id,
                          int http_status_code,
                          const GURL& image_url,
                          const std::vector<SkBitmap>& bitmaps,
                          const std::vector<gfx::Size>& sizes);

  void MaybeCompleteImageDownloadAndSelfDestruct();

  void OnIconFetchingCompleteSelfDestruct(FetchIconsFromDocumentResult result);

  base::WeakPtr<content::WebContents> web_contents_;
  char32_t fallback_letter_;
  FetchIconsFromDocumentCallback final_callback_;
  bool metadata_fetch_complete_ = false;
  int num_pending_image_requests_ = 0;
  std::vector<SkBitmap> icons_;

  base::WeakPtrFactory<DocumentIconFetcherTask> weak_factory_{this};
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_DOCUMENT_ICON_FETCHER_TASK_H_
