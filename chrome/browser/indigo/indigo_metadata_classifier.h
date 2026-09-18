// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_METADATA_CLASSIFIER_H_
#define CHROME_BROWSER_INDIGO_INDIGO_METADATA_CLASSIFIER_H_

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/document_metadata/document_metadata.mojom.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace indigo {

class IndigoService;

// Manages multi-stage schema.org product metadata extraction and keyword
// classification for a tab's primary page.
//
// To avoid waiting for slow subresources (ads, tracking pixels) at
// window.onload while still accommodating Client-Side Rendered (CSR) pages
// that inject <script type="application/ld+json"> after initial HTML parsing,
// this class evaluates metadata across multiple lifecycle milestones:
//   1. DOMContentLoaded (immediate resolution for Server-Side Rendered pages)
//   2. Post-DCL hydration delay (+1000ms) and window.onload
//   3. Max wait deadline (2500ms, before the 3s contextual cueing timeout)
//
// Any non-null Product/ProductGroup entity match locks in a decision (true or
// false) immediately. An empty (nullptr) result is treated as inconclusive
// until both window.onload and the post-DCL hydration retry have completed, or
// the max wait deadline is reached.
class IndigoMetadataClassifier {
 public:
  IndigoMetadataClassifier(IndigoService* indigo_service,
                           base::RepeatingClosure on_result_updated);
  IndigoMetadataClassifier(const IndigoMetadataClassifier&) = delete;
  IndigoMetadataClassifier& operator=(const IndigoMetadataClassifier&) = delete;
  ~IndigoMetadataClassifier();

  // Resets state and schedules timers for a committed navigation in the
  // primary main frame.
  void OnNavigationCommitted(content::WebContents* web_contents,
                             bool is_same_document);

  // Called when DOMContentLoaded fires for `render_frame_host`.
  void OnDOMContentLoaded(content::RenderFrameHost* render_frame_host);

  // Called when window.onload completes in the primary main frame.
  void OnDocumentOnLoadCompletedInPrimaryMainFrame();

  // Resets all state and cancels any pending timers or in-flight requests.
  void Reset();

  // Cancels pending timers and in-flight requests (e.g. when Optimization Guide
  // already determined the page is eligible) without resetting an already
  // computed result.
  void CancelPendingClassification();

  // Returns true if a Product/ProductGroup entity matched allowed keywords and
  // no blocked keywords.
  bool matches() const { return state_ == State::kMatch; }

  // Returns true if classification is actively pending for the current
  // navigation.
  bool is_pending() const { return state_ == State::kPending; }

 private:
  enum class State {
    kUnknown,  // Not started, reset, feature disabled, or cancelled.
    kPending,  // Actively evaluating metadata across lifecycle milestones.
    kMatch,    // Product entity matched allowed keywords (no blocked keywords).
    kNoMatch,  // Ineligible origin, blocked keyword, or no match by final
               // check.
  };

  // Queries the renderer for product metadata. `is_final_check` is true when
  // this will be the final check (either after all relevant events and retries,
  // or when the max wait deadline has expired), causing an empty (nullptr)
  // result to lock in `kNoMatch` instead of remaining `kPending`.
  void TriggerClassification(bool is_final_check);
  void OnProductClassified(bool is_final_check,
                           blink::mojom::ProductClassificationResultPtr result);
  void LockInResult(bool matches, bool record_uma);
  void OnRetryTimeout();
  void OnMaxWaitTimeout();

  const raw_ptr<IndigoService> indigo_service_;
  base::RepeatingClosure on_result_updated_;

  raw_ptr<content::WebContents> web_contents_ = nullptr;
  State state_ = State::kUnknown;

  bool document_onload_completed_ = false;
  bool retry_completed_ = false;

  base::OneShotTimer retry_timer_;
  base::OneShotTimer max_wait_timer_;

  mojo::Remote<blink::mojom::DocumentMetadata> metadata_remote_;
  base::CancelableOnceCallback<void(
      blink::mojom::ProductClassificationResultPtr)>
      pending_request_;
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_METADATA_CLASSIFIER_H_
