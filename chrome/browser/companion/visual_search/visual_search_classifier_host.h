// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_HOST_H_
#define CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_HOST_H_

#include <memory>
#include "chrome/browser/companion/visual_search/visual_search_suggestions_service.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"

namespace companion::visual_search {

// This class serves as the main orchestator for visual search suggestions
// components. It handles mojom IPC with both main renderer and side panel.
// It also fetches model file descriptors from the keyed service.
class VisualSearchClassifierHost {
 public:
  using ClassifierAgent =
      base::OnceCallback<void(int, base::File, std::string)>;

  explicit VisualSearchClassifierHost(
      VisualSearchSuggestionsService* visual_search_service);

  VisualSearchClassifierHost(const VisualSearchClassifierHost&) = delete;
  VisualSearchClassifierHost& operator=(const VisualSearchClassifierHost&) =
      delete;
  ~VisualSearchClassifierHost();

  // This is the main method used by the companion page handler to start the
  // visual search classification task. The RenderFrameHost is needed to
  // establish IPC channel with the Renderer process.
  void StartClassification(content::RenderFrameHost* render_frame_host,
                           const GURL& validated_url);

  // Set the classifier agent that will be used to do mojom IPC.
  // This should only be used for testing, not for production.
  void SetClassifierAgentForTesting(ClassifierAgent agent);

 private:
  // Processes the list of images returned from the visual search classifier.
  // Its main job is to take a list of SkBitmap and convert to data uris.
  // The list of image data uris are sent to side panel companion for rendering.
  void OnClassificationResult(const std::vector<SkBitmap>& images);

  // Pointer to visual search service which we do not own.
  raw_ptr<VisualSearchSuggestionsService> visual_search_service_ = nullptr;

  // This classifier agent is used to send mojom IPC to renderer.
  ClassifierAgent classifier_agent_;
};
}  // namespace companion::visual_search

#endif  // CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_HOST_H_
