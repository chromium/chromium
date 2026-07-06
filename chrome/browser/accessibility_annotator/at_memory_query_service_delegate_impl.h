// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_AT_MEMORY_QUERY_SERVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_AT_MEMORY_QUERY_SERVICE_DELEGATE_IMPL_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service_delegate.h"

class Profile;

namespace page_content_annotations {
class PageContentExtractionService;
class PageEmbeddingsService;
}  // namespace page_content_annotations

namespace passage_embeddings {
class Embedder;
}  // namespace passage_embeddings

namespace accessibility_annotator {

class LiveTabRetriever;

class AtMemoryQueryServiceDelegateImpl
    : public autofill::AtMemoryQueryServiceDelegate {
 public:
  explicit AtMemoryQueryServiceDelegateImpl(Profile* profile);
  AtMemoryQueryServiceDelegateImpl(
      Profile* profile,
      page_content_annotations::PageContentExtractionService*
          extraction_service,
      page_content_annotations::PageEmbeddingsService* embeddings_service,
      passage_embeddings::Embedder* embedder);
  AtMemoryQueryServiceDelegateImpl(const AtMemoryQueryServiceDelegateImpl&) =
      delete;
  AtMemoryQueryServiceDelegateImpl& operator=(
      const AtMemoryQueryServiceDelegateImpl&) = delete;
  ~AtMemoryQueryServiceDelegateImpl() override;

  // AtMemoryQueryServiceDelegate:
  void RetrieveLiveTabContext(
      autofill::LiveTabContextQuery query,
      base::OnceCallback<void(autofill::LiveTabContextResponse)> callback)
      override;

 private:
  const raw_ptr<Profile> profile_;

  std::unique_ptr<LiveTabRetriever> live_tab_retriever_;

  base::WeakPtrFactory<AtMemoryQueryServiceDelegateImpl> weak_ptr_factory_{
      this};
};

}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_AT_MEMORY_QUERY_SERVICE_DELEGATE_IMPL_H_
