// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_WEB_CONTENTS_PASSAGE_EMBEDDER_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_WEB_CONTENTS_PASSAGE_EMBEDDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace passage_embeddings {

// Base class for objects that compute embeddings for WebContents passages.
class WebContentsPassageEmbedder : public content::WebContentsObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual Embedder::TaskId ComputePassagesEmbeddings(
        std::vector<std::string> passages,
        Embedder::ComputePassagesEmbeddingsCallback callback) = 0;
    virtual bool TryCancel(Embedder::TaskId task_id) = 0;
    virtual void OnWebContentsDestroyed(content::WebContents* web_contents) = 0;
  };

  WebContentsPassageEmbedder(content::WebContents* web_contents,
                             Delegate& delegate);
  ~WebContentsPassageEmbedder() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  // Process passages for potential embeddings computation.
  virtual void AcceptPassages(std::vector<std::string> passages) = 0;

  // Provides an opportunity for subclasses to process passages when the overall
  // priority increases.
  virtual void MaybeProcessPendingPassagesOnPriorityIncrease() {}

  std::optional<Embedder::TaskId> current_task_id() const {
    return current_task_id_;
  }

 protected:
  // Request embeddings to be computed for `passages`.
  void ComputeEmbeddings(std::vector<std::string> passages);

  // Receive computed embeddings.
  void OnPassageEmbeddingsComputed(std::vector<std::string> passages,
                                   std::vector<Embedding> embeddings,
                                   Embedder::TaskId task_id,
                                   ComputeEmbeddingsStatus status);

 private:
  raw_ref<Delegate> delegate_;
  std::optional<Embedder::TaskId> current_task_id_;
  base::WeakPtrFactory<WebContentsPassageEmbedder> weak_ptr_factory_{this};
};

// A WebContentsPassageEmbedder that computes embeddings immediately on passage
// receipt.
class WebContentsImmediatePassageEmbedder : public WebContentsPassageEmbedder {
 public:
  WebContentsImmediatePassageEmbedder(content::WebContents* web_contents,
                                      Delegate& delegate);

  // WebContentsPassageEmbedder:
  void AcceptPassages(std::vector<std::string> passages) override;
};

// A WebContentsPassageEmbedder that computes embeddings only on WebContents
// backgrounding.
class WebContentsBackgroundPassageEmbedder : public WebContentsPassageEmbedder {
 public:
  WebContentsBackgroundPassageEmbedder(content::WebContents* web_contents,
                                       Delegate& delegate);
  ~WebContentsBackgroundPassageEmbedder() override;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;

  // WebContentsPassageEmbedder:
  void AcceptPassages(std::vector<std::string> passages) override;
  void MaybeProcessPendingPassagesOnPriorityIncrease() override;

 private:
  std::vector<std::string> pending_passages_;
  content::Visibility web_contents_visibility_ = content::Visibility::HIDDEN;
};

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_WEB_CONTENTS_PASSAGE_EMBEDDER_H_
