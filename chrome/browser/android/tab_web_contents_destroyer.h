// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DESTROYER_H_
#define CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DESTROYER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/window_container_type.mojom-forward.h"
#include "ui/base/window_open_disposition.h"

class GURL;
class Profile;

namespace blink::mojom {
class WindowFeatures;
}  // namespace blink::mojom

namespace content {
class JavaScriptDialogManager;
class NavigationHandle;
class RenderFrameHost;
class SessionStorageNamespaceHandle;
class SiteInstance;
class StoragePartitionConfig;
class WebContents;
struct OpenURLParams;
}  // namespace content

namespace tabs {

// Handles graceful shutdown and deferred destruction of a WebContents
// when a tab is closed.
class TabWebContentsDestroyer : public content::WebContentsDelegate,
                                public content::WebContentsObserver,
                                public ProfileObserver {
 public:
  TabWebContentsDestroyer(const TabWebContentsDestroyer&) = delete;
  TabWebContentsDestroyer& operator=(const TabWebContentsDestroyer&) = delete;

  // Initiates deferred graceful destruction of `web_contents`.
  // TabWebContentsDestroyer self-deletes upon completion or timeout.
  static void DestroyWebContents(
      std::unique_ptr<content::WebContents> web_contents);

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  bool ShouldSuppressDialogs(content::WebContents* source) override;
  bool IsWebContentsCreationOverridden(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
  content::WebContents* CreateCustomWebContents(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      bool is_new_browsing_instance,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      const content::StoragePartitionConfig& partition_config,
      content::SessionStorageNamespaceHandle* session_storage_namespace)
      override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  explicit TabWebContentsDestroyer(
      std::unique_ptr<content::WebContents> web_contents);
  ~TabWebContentsDestroyer() override;

  void StopNavigation();
  void Destroy();

  std::unique_ptr<content::WebContents> web_contents_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  base::WeakPtrFactory<TabWebContentsDestroyer> weak_ptr_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DESTROYER_H_
