// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_navigation_throttle.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_switcher/alternative_browser_driver.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

namespace browser_switcher {

namespace {

// Open 'chrome://browser-switch/?url=...' in the current tab.
void OpenBrowserSwitchPage(base::WeakPtr<content::WebContents> web_contents,
                           const GURL& url,
                           ui::PageTransition transition_type) {
  if (!web_contents) {
    return;
  }

  GURL about_url(chrome::kChromeUIBrowserSwitchURL);
  about_url = net::AppendQueryParameter(about_url, "url", url.spec());
  content::OpenURLParams params(about_url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                transition_type, false);
  web_contents->OpenURL(params, /*navigation_handle_callback=*/{});
}

void MaybeLaunchAlternativeBrowser(
    content::NavigationHandle* navigation_handle,
    bool should_run_async,
    navigation_interception::InterceptNavigationThrottle::ResultCallback
        result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!should_run_async);

  BrowserSwitcherService* service =
      BrowserSwitcherServiceFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  if (!service) {
    std::move(result_callback).Run(false);
    return;
  }

  const GURL& url = navigation_handle->GetURL();
  bool should_switch = service->sitelist()->ShouldSwitch(url);

  if (!should_switch) {
    std::move(result_callback).Run(false);
    return;
  }

  // This check is for GuestViews in particular. This works because we can only
  // navigate a guest after attaching to the outer WebContents.
  if (navigation_handle->GetWebContents()->GetOuterWebContents()) {
    std::move(result_callback).Run(false);
    return;
  }

  // If no-state prefetching, don't launch the alternative browser but abort the
  // navigation.
  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          navigation_handle->GetWebContents());
  if (no_state_prefetch_contents) {
    no_state_prefetch_contents->Destroy(prerender::FINAL_STATUS_BROWSER_SWITCH);
    std::move(result_callback).Run(true);
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OpenBrowserSwitchPage,
                     navigation_handle->GetWebContents()->GetWeakPtr(), url,
                     navigation_handle->GetPageTransition()));
  std::move(result_callback).Run(true);
}

}  // namespace

// static
void BrowserSwitcherNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::NavigationHandle& handle = registry.GetNavigationHandle();
  content::BrowserContext* browser_context =
      handle.GetWebContents()->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);

  if (!profile->IsRegularProfile()) {
    return;
  }

  if (!handle.IsInPrimaryMainFrame()) {
    return;
  }

  registry.AddThrottle(
      std::make_unique<navigation_interception::InterceptNavigationThrottle>(
          registry, base::BindRepeating(&MaybeLaunchAlternativeBrowser),
          navigation_interception::SynchronyMode::kSync, std::nullopt));
}

}  // namespace browser_switcher
