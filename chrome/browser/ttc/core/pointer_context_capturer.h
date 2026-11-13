// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_POINTER_CONTEXT_CAPTURER_H_
#define CHROME_BROWSER_TTC_CORE_POINTER_CONTEXT_CAPTURER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/ttc/app/public/tool_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace ttc {

// What get_pointer needs to know about a page to show and describe what the
// user pointed at. Both parts are captured together, so they agree.
struct PointerContext {
  // A JPEG of the viewport at its natural size, i.e. in device pixels.
  std::vector<uint8_t> screenshot;
  // Device pixels per DIP, for both the screenshot and the geometry in
  // `page_content`.
  float scale_factor = 1.0f;
  // What is on the page, with the geometry of everything in the viewport.
  optimization_guide::proto::AnnotatedPageContent page_content;
};

// Captures a PointerContext from a WebContents, giving up if it takes too
// long: the user is waiting on a spoken answer, and captures can stall, e.g.
// while the window is occluded.
class PointerContextCapturer {
 public:
  using Result = base::expected<PointerContext, ToolResponse>;
  using Callback = base::OnceCallback<void(Result)>;

  // A capture that takes longer than `timeout` fails.
  explicit PointerContextCapturer(base::TimeDelta timeout);
  PointerContextCapturer(const PointerContextCapturer&) = delete;
  PointerContextCapturer& operator=(const PointerContextCapturer&) = delete;
  ~PointerContextCapturer();

  // Captures the primary page of `web_contents` and runs `callback` with the
  // result, or with the error to answer the tool call with. The caller must
  // have checked that the page may be captured (see
  // IsUrlSupportedForPageContext()). Only one capture can be in progress at a
  // time. `callback` is not run if this is destroyed first.
  void Capture(content::WebContents* web_contents, Callback callback);

 private:
  void OnFetched(
      float scale_factor,
      page_content_annotations::FetchPageContextResultCallbackArg fetch_result);
  void OnTimeout();

  const base::TimeDelta timeout_;
  // Set while a capture is in progress.
  Callback callback_;
  std::unique_ptr<page_content_annotations::PageContextFetcher> fetcher_;
  base::OneShotTimer timer_;

  // The fetcher reports an error when destroyed mid-fetch; a weak pointer
  // keeps that from reaching this once it has given up or is being destroyed.
  base::WeakPtrFactory<PointerContextCapturer> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_POINTER_CONTEXT_CAPTURER_H_
