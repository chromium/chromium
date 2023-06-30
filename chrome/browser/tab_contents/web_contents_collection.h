// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_COLLECTION_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_COLLECTION_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

// Utility class for receiving `WebContentsObserver` callbacks from sets of
// `WebContents`. Manages a set of `WebContentsObserver` which forward their
// callbacks annotated with the WebContents they occurred in to an observer. The
// collection ensures that observer lifetimes are properly handled.
class WebContentsCollection {
 public:
  class Observer {
   public:
    // Observer callbacks that will be fired from each web contents being
    // watched in `web_contents_observers_`.
    virtual void WebContentsDestroyed(content::WebContents* web_contents) {}
    virtual void RenderProcessGone(content::WebContents* web_contents,
                                   base::TerminationStatus status) {}
    virtual void DidFinishNavigation(
        content::WebContents* web_contents,
        content::NavigationHandle* navigation_handle) {}
    virtual void DidStartLoading(content::WebContents* web_contents) {}

   protected:
    virtual ~Observer() = default;
  };

  // `observer` must outlive `this`.
  explicit WebContentsCollection(Observer* observer);
  ~WebContentsCollection();

  // Start forwarding `WebContentsObserver` calls from `web_contents` to
  // `observer_`.
  void StartObserving(content::WebContents* web_contents);

  // Stops `observer_` from receiving calls from `web_contents`.
  void StopObserving(content::WebContents* web_contents);

 private:
  class ForwardingWebContentsObserver;

  void WebContentsDestroyed(content::WebContents* web_contents);

  // Observer which will receive callbacks from any of the `WebContentsObserver`
  // in `web_contents_observers_`.
  const raw_ptr<Observer> observer_;

  // Map of observers for the WebContents part of this collection.
  base::flat_map<content::WebContents*,
                 std::unique_ptr<ForwardingWebContentsObserver>>
      web_contents_observers_;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_COLLECTION_H_
