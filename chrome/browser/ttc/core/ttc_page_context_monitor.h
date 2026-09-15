// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TTC_PAGE_CONTEXT_MONITOR_H_
#define CHROME_BROWSER_TTC_CORE_TTC_PAGE_CONTEXT_MONITOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ttc/core/page_context.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "content/public/browser/web_contents_observer.h"

namespace ttc {

// Monitors the given WebContents, notifying its client when the contained page
// has changed in various ways that may necessitate a new fetch of page
// context. It is the client's responsibility to schedule a new page context
// fetch by calling StartNewFetch.
// TODO(b/555804152): Monitor for additional in-page changes like DOM
// mutations.
class TtcPageContextMonitor : public content::WebContentsObserver {
 public:
  using PageChangedCallback = base::RepeatingClosure;

  // `page_changed_callback` is invoked whenever the page in `web_contents`
  // changes. The given callback must outlive this object.
  TtcPageContextMonitor(content::WebContents& web_contents,
                        PageChangedCallback page_changed_callback);
  ~TtcPageContextMonitor() override;

  TtcPageContextMonitor(const TtcPageContextMonitor&) = delete;
  TtcPageContextMonitor& operator=(const TtcPageContextMonitor&) = delete;

  // Starts fetching the page context of the monitored page, invoking
  // `callback` with the result. Any fetch already in flight is cancelled and
  // its callback will not be run.
  void StartNewFetch(FetchCompleteCallback callback);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void DidStopLoading() override;

 private:
  void NotifyPageChanged();

  void OnFetchComplete(
      FetchCompleteCallback callback,
      page_content_annotations::FetchPageContextResultCallbackArg result);

  const PageChangedCallback page_changed_callback_;

  std::unique_ptr<page_content_annotations::PageContextFetcher> fetcher_;

  base::WeakPtrFactory<TtcPageContextMonitor> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_PAGE_CONTEXT_MONITOR_H_
