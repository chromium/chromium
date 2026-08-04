// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_OBSERVER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_OBSERVER_H_

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace glic {

class GlicGuestObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<GlicGuestObserver> {
 public:
  explicit GlicGuestObserver(content::WebContents* web_contents);
  ~GlicGuestObserver() override;

  GlicGuestObserver(const GlicGuestObserver&) = delete;
  GlicGuestObserver& operator=(const GlicGuestObserver&) = delete;

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<GlicGuestObserver>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_OBSERVER_H_
