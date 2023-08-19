// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_contents_collection.h"

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"

class WebContentsCollection::ForwardingWebContentsObserver
    : public content::WebContentsObserver {
 public:
  ForwardingWebContentsObserver(content::WebContents* web_contents,
                                WebContentsCollection::Observer* observer,
                                WebContentsCollection* collection)
      : content::WebContentsObserver(web_contents),
        observer_(observer),
        collection_(collection) {}

 private:
  // WebContentsObserver:
  void WebContentsDestroyed() override {
    // Deletes `this`. Do no add any code after this line.
    collection_->WebContentsDestroyed(web_contents());
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    observer_->RenderProcessGone(web_contents(), status);
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    observer_->DidFinishNavigation(web_contents(), navigation_handle);
  }

  void DidStartLoading() override {
    observer_->DidStartLoading(web_contents());
  }

  // The observer that callbacks should forward to, annotating the
  // web contents they were fired in.
  raw_ptr<WebContentsCollection::Observer> observer_;

  // The collection this observer belongs to, needed to cleanup observer
  // lifetime.
  raw_ptr<WebContentsCollection> collection_;
};

WebContentsCollection::WebContentsCollection(
    WebContentsCollection::Observer* observer)
    : observer_(observer) {}

WebContentsCollection::~WebContentsCollection() = default;

void WebContentsCollection::StartObserving(content::WebContents* web_contents) {
  if (web_contents_observers_.find(web_contents) !=
      web_contents_observers_.end())
    return;

  auto emplace_result = web_contents_observers_.emplace(
      web_contents, std::make_unique<ForwardingWebContentsObserver>(
                        web_contents, observer_, this));
  DCHECK(emplace_result.second);
}

void WebContentsCollection::StopObserving(content::WebContents* web_contents) {
  web_contents_observers_.erase(web_contents);
}

void WebContentsCollection::WebContentsDestroyed(
    content::WebContents* web_contents) {
  web_contents_observers_.erase(web_contents);

  // We invoke the observer callback here rather than in
  // `ForwardingWebContentsObserver` to ensure that calling `RemoveObserver()`
  // during `WebContentsCollection::Observer::WebContentsDestroyed()` does not
  // cause a heap-use-after-free.
  observer_->WebContentsDestroyed(web_contents);
}
