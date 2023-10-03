// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_HOST_H_
#define CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_HOST_H_

#include <memory>
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/companion/core/companion_metrics_logger.h"
#include "chrome/browser/companion/visual_search/visual_search_suggestions_service.h"
#include "chrome/common/companion/visual_search.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

namespace companion::visual_search {

// Data container includes image (as base64 string) and alt-text.
struct VisualSuggestionsResult {
  // Base64 representation of image with jpeg encoding.
  std::string base64_img;

  // Alt text for image, needed for accessibility.
  std::string alt_text;

  ~VisualSuggestionsResult() = default;
};

using VisualSuggestionsResults = std::vector<VisualSuggestionsResult>;

using ClassificationStats = mojom::ClassificationStatsPtr;

// Used to store the last GURL/result pair that was classified.
using VisualSearchResultPair = std::pair<GURL, VisualSuggestionsResults>;

// Used to record classification initialization success or one of the various
// causes for initialization failure.
// Note: Must be kept in sync with //tools/metrics/histograms/enums.xml.
enum class InitStatus {
  // Catch-all bucket in the event that none of the below are applicable.
  kUnknown = 0,

  // Successful classification init.
  kSuccess = 1,

  // Failed classification init due to a null render frame host.
  kEmptyRenderFrame = 2,

  // Failed classification init due to classfication already running on the
  // current URL.
  kAlreadyStarted = 3,

  // Failed classification init due to an invalid classifier model.
  kModelUnavailable = 4,

  // Failed classification init due to the validated URL not matching the
  // current URL.
  kMismatchedUrl = 5,

  // Failed classification init due to the associated remote interface or the
  // request handler not being bound.
  kIpcNotMade = 6,

  // Failed classification init due to ongoing classification has not finished.
  kOngoingClassification = 7,

  // Represents that we have successfully sent a model fetch request.
  kFetchModel = 8,

  // Recorded when the callback is null after model is fetched.
  kCallbackCancelled = 9,

  // Tracks that the query has been cancelled before model was obtained.
  kQueryCancelled = 10,

  kMaxValue = kQueryCancelled,
};

// This class serves as the main orchestator for visual search suggestions
// components. It handles mojom IPC with both main renderer and side panel.
// It also fetches model file descriptors from the keyed service.
class VisualSearchClassifierHost : mojom::VisualSuggestionsResultHandler {
 public:
  using ResultCallback =
      base::OnceCallback<void(const VisualSuggestionsResults results,
                              const VisualSuggestionsMetrics metrics)>;

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
      std::vector<mojom::VisualSearchSuggestionPtr> results,
      mojom::ClassificationStatsPtr stats) override;

  // This is the main method used by the companion page handler to start the
  // visual search classification task. The RenderFrameHost is needed to
  // establish IPC channel with the Renderer process.
  void StartClassification(content::RenderFrameHost* render_frame_host,
                           const GURL& validated_url,
                           ResultCallback callback);

  // Used to cancel and cleanup any ongoing classification; currently it
  // mainly tracks the model fetching step.
  void CancelClassification(const GURL& visible_url);

  // Returns the |VisualSearchResult| for a given url, currently we only cache
  // the current url that we are processing.
  absl::optional<VisualSuggestionsResults> GetVisualResult(const GURL& url);

 private:
  // This method performs the actual mojom IPC to start classifier agent after
  // we have obtained the model from |visual_search_service_|.
  void StartClassificationWithModel(
      mojo::AssociatedRemote<mojom::VisualSuggestionsRequestHandler>
          visual_search,
      base::File file,
      const std::string& base64_config);

  // Pointer to visual search service which we do not own.
  raw_ptr<VisualSearchSuggestionsService> visual_search_service_ = nullptr;

  // This callback is used to send list of data uris (i.e. strings) to caller.
  ResultCallback result_callback_;

  // This reference binds this class to the result handler for the mojom IPC.
  mojo::Receiver<mojom::VisualSuggestionsResultHandler> result_handler_{this};

  // Used to track the time at which StartClassification was invoked.
  base::TimeTicks classification_start_time_;

  // Tracks whether or not we are waiting for result to a request.
  bool waiting_for_result_ = false;

  // Used to store last |VisualSearchResult|, this is needed for instances where
  // the result is ready before the WebUI is ready to render it.
  absl::optional<VisualSearchResultPair> current_result_;

  // Pointer factory necessary for scheduling tasks on different threads.
  base::WeakPtrFactory<VisualSearchClassifierHost> weak_ptr_factory_{this};
};
}  // namespace companion::visual_search

#endif  // CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_HOST_H_
