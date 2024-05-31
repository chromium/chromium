// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_ANCHOR_ELEMENT_PRELOADER_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_ANCHOR_ELEMENT_PRELOADER_H_

#include "base/memory/raw_ref.h"
#include "content/public/browser/anchor_element_preconnect_delegate.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/preloading.h"
#include "url/scheme_host_port.h"

extern const char kPreloadingAnchorElementPreloaderPreloadingTriggered[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

enum class AnchorElementPreloaderType {
  kUnspecified = 0,
  kPreconnect = 1,
  kMaxValue = kPreconnect,
};

// If you change this, please follow the process in
// go/preloading-dashboard-updates to update the mapping reflected in dashboard,
// or if you are not a Googler, please file an FYI bug on https://crbug.new with
// component Internals>Preload.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange
enum class AnchorPreloadingFailureReason {
  // Numbering starts from `kPreloadingFailureReasonContentEnd` defined in
  // //content/public/preloading.h . Advance numbering by +1 when adding a new
  // element.

  // The number of allowed anchor element preloading attempts has been exceeded.
  kLimitExceeded = static_cast<int>(
      content::PreloadingFailureReason::kPreloadingFailureReasonContentEnd),
};
// LINT.ThenChange()

// Helper function to convert AnchorPreloadingFailureReason to
// content::PreloadingFailureReason without casting.
content::PreloadingFailureReason ToFailureReason(
    AnchorPreloadingFailureReason reason);

class AnchorElementPreloader : public content::AnchorElementPreconnectDelegate {
 public:
  explicit AnchorElementPreloader(content::RenderFrameHost& render_frame_host);
  ~AnchorElementPreloader() override;

  // Disallows copy and move operations.
  AnchorElementPreloader(const AnchorElementPreloader&) = delete;
  AnchorElementPreloader& operator=(const AnchorElementPreloader&) = delete;

  AnchorElementPreloader(AnchorElementPreloader&&) = delete;
  AnchorElementPreloader& operator=(AnchorElementPreloader&&) = delete;

  // Preconnects to the given URL `target`.
  void MaybePreconnect(const GURL& target) override;

 private:
  void RecordUmaPreloadedTriggered(AnchorElementPreloaderType);

  void RecordUkmPreloadType(AnchorElementPreloaderType);

  // content::PreloadingDecider, which inherits content::DocumentUserData, owns
  // `this`, so `this` can access `render_frame_host_` safely.
  const raw_ref<content::RenderFrameHost> render_frame_host_;

  std::set<url::SchemeHostPort> preconnected_targets_;
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_ANCHOR_ELEMENT_PRELOADER_H_
