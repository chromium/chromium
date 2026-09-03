// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_web_contents_destroyer.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace tabs {

namespace {

// 2 seconds was chosen to give sufficient time for unload handlers to run
// during window closure (see DEFAULT_SHUTDOWN_TIMEOUT_MS in
// GracefulShutdownServiceImpl which matches this duration).
constexpr base::TimeDelta kShutdownTimeout = base::Seconds(2);

}  // namespace

// static
void TabWebContentsDestroyer::DestroyWebContents(
    std::unique_ptr<content::WebContents> web_contents) {
  if (!web_contents) {
    return;
  }
  new TabWebContentsDestroyer(std::move(web_contents));
}

TabWebContentsDestroyer::TabWebContentsDestroyer(
    std::unique_ptr<content::WebContents> web_contents)
    : content::WebContentsObserver(web_contents.get()),
      web_contents_(std::move(web_contents)) {
  if (web_contents_) {
    if (Profile* profile =
            Profile::FromBrowserContext(web_contents_->GetBrowserContext())) {
      profile_observation_.Observe(profile);
    }
  }
  web_contents_->SetDelegate(this);
  // Cancel any pre-existing in-flight navigations before ClosePage() cancels
  // NavigationRequests.
  web_contents_->Stop();
  web_contents_->ClosePage();
  // Fallback timer in case the renderer never responds.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TabWebContentsDestroyer::Destroy,
                     weak_ptr_factory_.GetWeakPtr()),
      kShutdownTimeout);
}

TabWebContentsDestroyer::~TabWebContentsDestroyer() {
  profile_observation_.Reset();
  Observe(nullptr);
  if (web_contents_) {
    if (auto* dialog_manager =
            javascript_dialogs::TabModalDialogManager::FromWebContents(
                web_contents_.get())) {
      dialog_manager->CancelDialogs(web_contents_.get(),
                                    /*reset_state=*/true);
    }
    web_contents_->SetDelegate(nullptr);
    web_contents_.reset();
  }
}

// content::WebContentsDelegate:
void TabWebContentsDestroyer::CloseContents(content::WebContents* source) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TabWebContentsDestroyer::Destroy,
                                weak_ptr_factory_.GetWeakPtr()));
}

content::JavaScriptDialogManager*
TabWebContentsDestroyer::GetJavaScriptDialogManager(
    content::WebContents* source) {
  return javascript_dialogs::TabModalDialogManager::FromWebContents(source);
}

// Ignore top-level navigation requests initiated during graceful shutdown
// (e.g. via window.location) to prevent canceled NavigationRequests from
// causing renderer bad message failures.
content::WebContents* TabWebContentsDestroyer::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  return nullptr;
}

// Suppress JavaScript modal dialogs while WebContents is shutting down.
bool TabWebContentsDestroyer::ShouldSuppressDialogs(
    content::WebContents* source) {
  return true;
}

// Intercept window/popup creation requests so CreateCustomWebContents is
// called to reject new windows during graceful shutdown.
bool TabWebContentsDestroyer::IsWebContentsCreationOverridden(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  return true;
}

// Reject creation of new WebContents/popups initiated during graceful
// shutdown.
content::WebContents* TabWebContentsDestroyer::CreateCustomWebContents(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    bool is_new_browsing_instance,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    const content::StoragePartitionConfig& partition_config,
    content::SessionStorageNamespaceHandle* session_storage_namespace) {
  return nullptr;
}

// content::WebContentsObserver:
void TabWebContentsDestroyer::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!web_contents_) {
    return;
  }
  // Synchronously stopping the navigation is not safe as some other callers
  // in the observer chain may try to access the navigation which we want to
  // stop.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&TabWebContentsDestroyer::StopNavigation,
                                weak_ptr_factory_.GetWeakPtr()));
}

void TabWebContentsDestroyer::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  Destroy();
}

// ProfileObserver:
void TabWebContentsDestroyer::OnProfileWillBeDestroyed(Profile* profile) {
  if (profile_observation_.GetSource() == profile) {
    Destroy();
  }
}

void TabWebContentsDestroyer::StopNavigation() {
  if (web_contents_) {
    web_contents_->Stop();
  }
}

void TabWebContentsDestroyer::Destroy() {
  delete this;
}

}  // namespace tabs
