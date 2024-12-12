// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDER_TAB_OBSERVER_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDER_TAB_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/content_extraction/inner_text.mojom.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WeakDocumentPtr;
class WebContents;
}  // namespace content

namespace passage_embeddings {

// Extracts passages from the page and runs the passage embedder.
class EmbedderTabObserver : public content::WebContentsObserver {
 public:
  explicit EmbedderTabObserver(content::WebContents* web_contents);

  EmbedderTabObserver(const EmbedderTabObserver&) = delete;
  EmbedderTabObserver& operator=(const EmbedderTabObserver&) = delete;

  ~EmbedderTabObserver() override;

 private:
  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // Utility method to delay passage extraction until tabs are done loading.
  // Does nothing and returns false if RenderFrameHost was deleted or navigated
  // to a different document.
  bool ScheduleExtraction(content::WeakDocumentPtr weak_render_frame_host);

  // This is called some time after `DidFinishLoad`. Calls `ExtractPassages` if
  // tabs are done loading or `ScheduleExtraction` otherwise.
  void MaybeExtractPassages(content::WeakDocumentPtr weak_render_frame_host);

  // Initiates async passage extraction from the given host's main frame if
  // RenderFrameHost was not deleted nor navigated to a different document and
  // RenderFrame in the renderer process has been created and has a connection.
  void ExtractPassages(content::WeakDocumentPtr weak_render_frame_host);

  // Initiates async passage embeddings computation once passage extraction
  // completes.
  void OnGotPassages(mojo::Remote<blink::mojom::InnerTextAgent> remote,
                     base::ElapsedTimer passage_extraction_timer,
                     blink::mojom::InnerTextFramePtr mojo_frame);

  Profile* GetProfile();

  const raw_ptr<content::WebContents> web_contents_;

  // Used to cancel scheduled passage extraction.
  base::WeakPtrFactory<EmbedderTabObserver> weak_ptr_factory_{this};
};

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDER_TAB_OBSERVER_H_
