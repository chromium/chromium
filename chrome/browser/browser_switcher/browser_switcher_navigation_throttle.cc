// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_navigation_throttle.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_switcher/alternative_browser_driver.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/prerender/chrome_prerender_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "components/prerender/browser/prerender_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

namespace browser_switcher {

namespace {

// Open 'chrome://browser-switch/?url=...' in the current tab.
void OpenBrowserSwitchPage(content::WebContents* web_contents,
                           const GURL& url,
                           ui::PageTransition transition_type) {
  GURL about_url(chrome::kChromeUIBrowserSwitchURL);
  about_url = net::AppendQueryParameter(about_url, "url", url.spec());
  content::OpenURLParams params(about_url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                transition_type, false);
  web_contents->OpenURL(params);
}

bool MaybeLaunchAlternativeBrowser(
    content::WebContents* web_contents,
    const navigation_interception::NavigationParams& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  BrowserSwitcherService* service =
      BrowserSwitcherServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  const GURL& url = params.url();
  bool should_switch = service->sitelist()->ShouldSwitch(url);

  if (!should_switch)
    return false;

  // Redirect top-level navigations only. This excludes iframes and webviews
  // in particular. Since we can only navigate a guest after attaching to the
  // outer WebContents, this check works for both guests and portals.
  if (web_contents->GetOuterWebContents())
    return false;

  // If prerendering, don't launch the alternative browser but abort the
  // navigation.
  prerender::PrerenderContents* prerender_contents =
      prerender::ChromePrerenderContentsDelegate::FromWebContents(web_contents);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_BROWSER_SWITCH);
    return true;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OpenBrowserSwitchPage, base::Unretained(web_contents),
                     url, params.transition_type()));
  return true;
}

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
BrowserSwitcherNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserContext* browser_context =
      navigation->GetWebContents()->GetBrowserContext();

  if (browser_context->IsOffTheRecord())
    return nullptr;

  if (!navigation->IsInMainFrame())
    return nullptr;

  return std::make_unique<navigation_interception::InterceptNavigationThrottle>(
      navigation, base::BindRepeating(&MaybeLaunchAlternativeBrowser),
      navigation_interception::SynchronyMode::kSync);
}

}  // namespace browser_switcher
