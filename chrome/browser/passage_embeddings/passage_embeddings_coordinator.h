// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_COORDINATOR_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_COORDINATOR_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/passage_embeddings/omnibox_focus_change_listener.h"
#include "chrome/browser/passage_embeddings/web_contents_passage_embedder.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/passage_embeddings/passage_embeddings_types.h"

namespace content {
class WebContents;
}

namespace page_content_annotations {
class PageContentExtractionService;
}

namespace passage_embeddings {

class PassageEmbeddingsCoordinator
    : public KeyedService,
      public page_content_annotations::PageContentExtractionService::Observer,
      public WebContentsPassageEmbedder::Delegate {
 public:
  explicit PassageEmbeddingsCoordinator(
      page_content_annotations::PageContentExtractionService*
          page_content_extraction_service);
  ~PassageEmbeddingsCoordinator() override;

  // PageContentExtractionService:
  void OnPageContentExtracted(
      content::Page& page,
      const optimization_guide::proto::AnnotatedPageContent& page_content)
      override;

  // WebContentsPassageEmbedder::Delegate:
  Embedder::TaskId ComputePassagesEmbeddings(
      std::vector<std::string> passages,
      Embedder::ComputePassagesEmbeddingsCallback callback) override;
  bool TryCancel(Embedder::TaskId task_id) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;

 private:
  void OnOmniboxFocusChanged(bool is_focused);

  base::ScopedObservation<
      page_content_annotations::PageContentExtractionService,
      PassageEmbeddingsCoordinator>
      page_content_extraction_observation_{this};

  std::map<content::WebContents*, std::unique_ptr<WebContentsPassageEmbedder>>
      web_contents_passage_embedders_;

  OmniboxFocusChangedListener omnibox_focus_changed_listener_;

  PassagePriority current_priority_ = PassagePriority::kPassive;

  base::WeakPtrFactory<PassageEmbeddingsCoordinator> weak_ptr_factory_{this};
};

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_COORDINATOR_H_
