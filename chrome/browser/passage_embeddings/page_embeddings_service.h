// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_PAGE_EMBEDDINGS_SERVICE_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_PAGE_EMBEDDINGS_SERVICE_H_

#include <map>
#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace page_content_annotations {
class PageContentExtractionService;
}

namespace passage_embeddings {

class WebContentsDestructionObserver : public content::WebContentsObserver {
 public:
  WebContentsDestructionObserver(
      content::WebContents* web_contents,
      base::OnceCallback<void(content::WebContents*)> destroyed_callback);
  ~WebContentsDestructionObserver() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  base::OnceCallback<void(content::WebContents*)> destroyed_callback_;
};

// A passage from a page along with its computed embedding.
struct PassageEmbedding {
  std::string passage;
  passage_embeddings::Embedding embedding;
};

class PageEmbeddingsService
    : public KeyedService,
      public page_content_annotations::PageContentExtractionService::Observer {
 public:
  // The priority to use when computing embeddings. Higher priorities imply more
  // performance overhead.
  enum Priority {
    kUserBlocking,
    kUrgent,
    kDefault,
    kBackground,
  };

  class Observer : public base::CheckedObserver {
   public:
    // Gets the default priority to use for computing embeddings.
    // Implementations are expected to return the same value over the entire
    // lifetime of the observer.
    virtual Priority GetDefaultPriority() const = 0;

    // Invoked when embeddings become available or are updated for the
    // web_contents. The embeddings then can be queried via GetEmbeddings().
    virtual void OnPageEmbeddingsAvailable(content::WebContents* web_contents) {
    }
  };

  // ScopedPriority allows observers to temporarily raise the priority of the
  // embeddings computation for the lifetime of the object. This can be useful,
  // for example, if embeddings are anticipated to be needed urgently to drive
  // UI features.
  class ScopedPriority {
   public:
    ScopedPriority(PageEmbeddingsService* service,
                   Observer* observer,
                   Priority priority);
    ~ScopedPriority();

    ScopedPriority(ScopedPriority& other) = delete;
    ScopedPriority& operator=(ScopedPriority& other) = delete;

    ScopedPriority(ScopedPriority&& other);
    ScopedPriority& operator=(ScopedPriority&& other);

   private:
    raw_ptr<PageEmbeddingsService> service_;
    raw_ptr<PageEmbeddingsService::Observer> observer_;
  };

  // A callback to produce the passages for a page for which to generate
  // embeddings. This is responsible for generating chunked passages from the
  // AnnotatedPageContent and filtering to the top passages_to_generate most
  // useful passages.
  using EmbeddingCandidatesGenerator =
      base::RepeatingCallback<std::vector<std::string>(
          const optimization_guide::proto::AnnotatedPageContent&,
          int passages_to_generate)>;

  PageEmbeddingsService(EmbeddingCandidatesGenerator candidates_generator,
                        page_content_annotations::PageContentExtractionService*
                            page_content_extraction_service,
                        passage_embeddings::Embedder* embedder);
  ~PageEmbeddingsService() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  ScopedPriority RaisePriority(Observer* observer, Priority priority);

  // Retrieves the embeddings for web_content. Returns the empty vector if
  // embeddings have not yet been computed.
  std::vector<PassageEmbedding> GetEmbeddings(
      content::WebContents* web_content) const;

  // PageContentExtractionService:
  void OnPageContentExtracted(
      content::Page& page,
      const optimization_guide::proto::AnnotatedPageContent& page_content)
      override;

 private:
  void OnEmbeddingsComputed(base::WeakPtr<content::WebContents> web_contents,
                            std::vector<std::string> passages,
                            std::vector<Embedding> embeddings,
                            Embedder::TaskId task_id,
                            ComputeEmbeddingsStatus status);

  void OnWebContentsDestroyed(content::WebContents* web_contents);

  static Priority GetActivePriority(
      const base::ObserverList<Observer>& observers,
      const std::map<Observer*, Priority>& temporary_priority);

  void UpdateTaskPriorities(Priority priority);

  struct WebContentsState {
    WebContentsState();
    ~WebContentsState();

    std::unique_ptr<WebContentsDestructionObserver> observer;
    std::optional<Embedder::TaskId> active_task;
    std::vector<PassageEmbedding> passage_embeddings;
  };

  const EmbeddingCandidatesGenerator candidates_generator_;

  const raw_ptr<passage_embeddings::Embedder> embedder_;

  base::ScopedObservation<
      page_content_annotations::PageContentExtractionService,
      PageEmbeddingsService>
      page_content_extraction_observation_{this};

  base::ObserverList<Observer> observers_;
  std::map<Observer*, Priority> temporary_priority_;

  Priority current_priority_ = kDefault;

  std::map<content::WebContents*, WebContentsState> web_contents_state_;

  base::WeakPtrFactory<PageEmbeddingsService> weak_ptr_factory_{this};
};

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_PAGE_EMBEDDINGS_SERVICE_H_
