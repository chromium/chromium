// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_RESOURCE_LOADING_HINTS_RESOURCE_LOADING_HINTS_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_PREVIEWS_RESOURCE_LOADING_HINTS_RESOURCE_LOADING_HINTS_WEB_CONTENTS_OBSERVER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/common/previews_resource_loading_hints.mojom-forward.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

class Profile;

// Observes navigation events and sends the resource loading hints mojo message
// to the renderer.
class ResourceLoadingHintsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          ResourceLoadingHintsWebContentsObserver> {
 public:
  ~ResourceLoadingHintsWebContentsObserver() override;

 private:
  friend class content::WebContentsUserData<
      ResourceLoadingHintsWebContentsObserver>;

  explicit ResourceLoadingHintsWebContentsObserver(
      content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Overridden from content::WebContentsObserver. If the navigation is of type
  // resource loading hints preview, then this method sends the resource loading
  // hints mojo message to the renderer before the commit occurs. This ensures
  // that the hints will be available to the renderer as soon as the document
  // starts rendering.
  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  // TODO(https://crbug.com/891328): Clean up older interfaces once
  // kUseRenderFrameObserverForPreviewsLoadingHints is enabled by default.
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Reports the start URL and the end URL in the current redirect chain to
  // previews service.
  void ReportRedirects(content::NavigationHandle* navigation_handle);

  // Set in constructor.
  Profile* profile_ = nullptr;

  mojo::AssociatedRemote<previews::mojom::PreviewsResourceLoadingHintsReceiver>
  GetResourceLoadingHintsReceiver(content::NavigationHandle* navigation_handle);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingHintsWebContentsObserver);
};

#endif  // CHROME_BROWSER_PREVIEWS_RESOURCE_LOADING_HINTS_RESOURCE_LOADING_HINTS_WEB_CONTENTS_OBSERVER_H_
