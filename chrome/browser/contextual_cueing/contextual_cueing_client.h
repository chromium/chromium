// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CLIENT_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class COMPONENT_EXPORT(CONTEXTUAL_CUEING) ContextualCueingClient
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContextualCueingClient> {
 public:
  ContextualCueingClient(const ContextualCueingClient&) = delete;
  ContextualCueingClient& operator=(const ContextualCueingClient&) = delete;
  ~ContextualCueingClient() override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit ContextualCueingClient(content::WebContents* contents);

  friend WebContentsUserData;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CLIENT_H_
