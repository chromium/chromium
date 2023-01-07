// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/test_distillation_observers.h"
#include "components/dom_distiller/core/url_constants.h"
#include "url/gurl.h"

namespace dom_distiller {

void OriginalPageNavigationObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->GetParent())
    Stop();
}

void DistilledPageObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->GetParent() &&
      validated_url.scheme() == kDomDistillerScheme) {
    loaded_distiller_page_ = true;
    MaybeNotifyLoaded();
  }
}

void DistilledPageObserver::TitleWasSet(content::NavigationEntry* entry) {
  // The title will be set twice on distilled pages; once for the placeholder
  // and once when the distillation has finished. Watch for the second time
  // as a signal that the JavaScript that sets the content has run.
  title_set_count_++;
  MaybeNotifyLoaded();
}

void DistilledPageObserver::MaybeNotifyLoaded() {
  if (title_set_count_ >= 2 && loaded_distiller_page_)
    Stop();
}

}  // namespace dom_distiller
