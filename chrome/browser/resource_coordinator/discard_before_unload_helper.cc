// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/discard_before_unload_helper.h"

#include "base/bind.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace resource_coordinator {

namespace {

// This is a helper class that determines whether or not there is a beforeunload
// handler associated with a given WebContents, and if so, whether or not it
// indicates that the page contains unsaved state.
//
// This is done by actually running the beforeunload handler if there is one. If
// the beforeunload handler returns a non-empty string then a javascript dialog
// request is made, in which case it is intercepted before it is displayed and
// implicitly canceled.
//
// The beforeunload is initiated via WebContents::DispatchBeforeUnload, and the
// outcome of the beforeunload is monitored via
// WebContentsObserver::BeforeUnloadFired and ::BeforeUnloadDialogCanceled.
//
// Note that the callback is guaranteed to be invoked; in the worst case
// scenario it will be invoked when the WebContents is destroyed, with a
// |proceed|=true value.
class DiscardBeforeUnloadHelper : public content::WebContentsObserver {
 public:
  static void HasBeforeUnloadHandler(content::WebContents* contents,
                                     HasBeforeUnloadHandlerCallback&& callback);

  ~DiscardBeforeUnloadHelper() override;

 private:
  // This is only meant to be called via "new", as the object takes ownership
  // of itself.
  DiscardBeforeUnloadHelper(content::WebContents* contents,
                            HasBeforeUnloadHandlerCallback&& callback);

  // WebContentsObserver:
  void BeforeUnloadFired(bool proceed,
                         const base::TimeTicks& proceed_time) override;
  void BeforeUnloadDialogCancelled() override;
  void WebContentsDestroyed() override;

  // Responds by invoking the callback, and cleaning itself up.
  void Respond(bool has_beforeunload_handler);

  // This object keeps itself alive while waiting for a callback, and cleans
  // itself up when done.
  std::unique_ptr<DiscardBeforeUnloadHelper> self_;

  HasBeforeUnloadHandlerCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DiscardBeforeUnloadHelper);
};

void DiscardBeforeUnloadHelper::HasBeforeUnloadHandler(
    content::WebContents* contents,
    HasBeforeUnloadHandlerCallback&& callback) {
  // Create this object and deliberately let go of it. It deletes itself after
  // it has invoked the callback.
  new DiscardBeforeUnloadHelper(contents, std::move(callback));
}

DiscardBeforeUnloadHelper::DiscardBeforeUnloadHelper(
    content::WebContents* contents,
    HasBeforeUnloadHandlerCallback&& callback)
    : WebContentsObserver(contents),
      self_(this),
      callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
  // NOTE: Ideally this would call NeedToFireBeforeUnloadOrUnload and entirely
  // skip on the dispatch if there are no unload handlers installed.
  // Unfortunately, NeedToFireBeforeUnloadOrUnload doesn't check the main
  // frame, so this doesn't quite work. See this related bug for more
  // information: crbug.com/869956
  web_contents()->DispatchBeforeUnload(true /* auto_cancel */);
}

DiscardBeforeUnloadHelper::~DiscardBeforeUnloadHelper() = default;

void DiscardBeforeUnloadHelper::BeforeUnloadFired(
    bool proceed,
    const base::TimeTicks& proceed_time) {
  // |proceed = true| means no beforeunload handler and vice-versa.
  Respond(!proceed /* has_beforeunload_handler */);
}

void DiscardBeforeUnloadHelper::BeforeUnloadDialogCancelled() {
  // Canceling a beforeunload dialog means that there was a beforeunload handler
  // that was already running prior to our request, and more specifically that
  // the user wants to stay on the page.
  Respond(true /* has_beforeunload_handler */);
}

void DiscardBeforeUnloadHelper::WebContentsDestroyed() {
  // If a WebContents is destroyed while waiting for the beforeunload response
  // this can be interpreted as |proceed| being true, as the page is no longer
  // going to be around to be preserved.
  Respond(false /* has_beforeunload_handler */);
}

void DiscardBeforeUnloadHelper::Respond(bool has_beforeunload_handler) {
  std::move(callback_).Run(has_beforeunload_handler);
  self_.reset();
}

}  // namespace

void HasBeforeUnloadHandler(content::WebContents* contents,
                            HasBeforeUnloadHandlerCallback&& callback) {
  DiscardBeforeUnloadHelper::HasBeforeUnloadHandler(contents,
                                                    std::move(callback));
}

}  // namespace resource_coordinator
