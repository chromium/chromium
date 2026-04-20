// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_WEB_CONTENTS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
class Page;
}  // namespace content

namespace contextual_cueing {

class ContextualCueingService;

// Observes WebContents visits and notifies ContextualCueingService on changes.
class ContextualCueingWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContextualCueingWebContentsObserver> {
 public:
  ContextualCueingWebContentsObserver(
      const ContextualCueingWebContentsObserver&) = delete;
  ContextualCueingWebContentsObserver& operator=(
      const ContextualCueingWebContentsObserver&) = delete;
  ~ContextualCueingWebContentsObserver() override;

  ContextualCueingWebContentsObserver(content::WebContents* web_contents,
                                      ContextualCueingService* service);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

 private:
  friend class content::WebContentsUserData<
      ContextualCueingWebContentsObserver>;

  raw_ptr<ContextualCueingService> service_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_WEB_CONTENTS_OBSERVER_H_
