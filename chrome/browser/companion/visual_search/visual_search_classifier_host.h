// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_HOST_H_
#define CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_HOST_H_

#include <memory>
#include "base/memory/weak_ptr.h"
#include "chrome/browser/companion/visual_search/visual_search_suggestions_service.h"
#include "chrome/common/companion/visual_search.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

namespace companion::visual_search {

// This class serves as the main orchestator for visual search suggestions
// components. It handles mojom IPC with both main renderer and side panel.
// It also fetches model file descriptors from the keyed service.
class VisualSearchClassifierHost : mojom::VisualSuggestionsResultHandler {
 public:
  using ResultCallback = base::OnceCallback<void(std::vector<std::string>)>;

  explicit VisualSearchClassifierHost(
      VisualSearchSuggestionsService* visual_search_service);

  VisualSearchClassifierHost(const VisualSearchClassifierHost&) = delete;
  VisualSearchClassifierHost& operator=(const VisualSearchClassifierHost&) =
      delete;
  ~VisualSearchClassifierHost() override;

  // From mojom::VisualSuggestionsResultsHandler.
  // Processes the list of images returned from the visual search classifier.
  // Its main job is to take a list of SkBitmap and convert to data uris.
  // The list of image data uris are sent to side panel companion for
  // rendering.
  void HandleClassification(
      std::vector<mojom::VisualSearchSuggestionPtr> results) override;

  // This is the main method used by the companion page handler to start the
  // visual search classification task. The RenderFrameHost is needed to
  // establish IPC channel with the Renderer process.
  void StartClassification(content::RenderFrameHost* render_frame_host,
                           const GURL& validated_url,
                           ResultCallback callback);

  // Used to cancel and cleanup any ongoing classification; currently it
  // mainly tracks the model fetching step.
  void CancelClassification();

 private:
  // This method performs the actual mojom IPC to start classifier agent after
  // we have obtained the model from |visual_search_service_|.
  void StartClassificationWithModel(content::RenderFrameHost* render_frame_host,
                                    const GURL validated_url,
                                    ResultCallback callback,
                                    base::File file,
                                    std::string base64_config);

  // Used to track the url that is currently being processed.
  GURL current_url_;

  // Pointer to visual search service which we do not own.
  raw_ptr<VisualSearchSuggestionsService> visual_search_service_ = nullptr;

  // This callback is used to send list of data uris (i.e. strings) to caller.
  ResultCallback result_callback_;

  // This reference binds this class to the result handler for the mojom IPC.
  mojo::Receiver<mojom::VisualSuggestionsResultHandler> result_handler_{this};

  // Pointer factory necessary for scheduling tasks on different threads.
  base::WeakPtrFactory<VisualSearchClassifierHost> weak_ptr_factory_{this};
};
}  // namespace companion::visual_search

#endif  // CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_HOST_H_
