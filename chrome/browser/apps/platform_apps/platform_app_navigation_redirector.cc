// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/platform_app_navigation_redirector.h"

#include "apps/launcher.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/guest_mode.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::WebContents;
using extensions::Extension;
using extensions::UrlHandlers;
using extensions::UrlHandlerInfo;

namespace {

bool LaunchAppWithUrl(const scoped_refptr<const Extension> app,
                      const std::string& handler_id,
                      content::WebContents* source,
                      const navigation_interception::NavigationParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Redirect top-level navigations only. This excludes iframes and webviews
  // in particular.
  if (content::GuestMode::IsCrossProcessFrameGuest(source)) {
    DVLOG(1) << "Cancel redirection: source is a inner WebContents";
    return false;
  }

  // If prerendering, don't launch the app but abort the navigation.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(source);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_NAVIGATION_INTERCEPTED);
    return true;
  }

  // These are guaranteed by MaybeCreateThrottleFor below.
  DCHECK(UrlHandlers::CanPlatformAppHandleUrl(app.get(), params.url()));
  DCHECK(!params.is_post());

  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());

  DVLOG(1) << "Launching app handler with URL: " << params.url().spec()
           << " -> " << app->name() << "(" << app->id() << "):" << handler_id;
  apps::LaunchPlatformAppWithUrl(profile, app.get(), handler_id, params.url(),
                                 params.referrer().url);

  return true;
}

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
PlatformAppNavigationRedirector::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(1) << "Considering URL for redirection: " << handle->GetURL().spec();

  content::BrowserContext* browser_context =
      handle->GetWebContents()->GetBrowserContext();
  DCHECK(browser_context);

  // Support only GET for now.
  if (handle->IsPost()) {
    DVLOG(1) << "Skip redirection: method is not GET";
    return nullptr;
  }

  if (!handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    DVLOG(1) << "Skip redirection: scheme is not HTTP or HTTPS";
    return nullptr;
  }

  // Redirect URLs to apps only in regular mode. Technically, apps are not
  // supported in incognito and guest modes, but that may change in future.
  // See crbug.com/240879, which tracks incognito support for v2 apps.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile->IsRegularProfile()) {
    DVLOG(1) << "Skip redirection: unsupported in incognito and guest modes";
    return nullptr;
  }

  for (const auto& extension_ref :
       extensions::ExtensionRegistry::Get(browser_context)
           ->enabled_extensions()) {
    // BookmarkAppNavigationThrottle handles intercepting links to Hosted Apps
    // that are Bookmark Apps. Other types of apps don't intercept links.
    if (!extension_ref->is_platform_app())
      continue;

    const UrlHandlerInfo* handler =
        UrlHandlers::GetMatchingPlatformAppUrlHandler(extension_ref.get(),
                                                      handle->GetURL());
    if (handler) {
      DVLOG(1) << "Found matching app handler for redirection: "
               << extension_ref->name() << "(" << extension_ref->id()
               << "):" << handler->id;
      return std::make_unique<
          navigation_interception::InterceptNavigationThrottle>(
          handle, base::Bind(&LaunchAppWithUrl, extension_ref, handler->id),
          navigation_interception::SynchronyMode::kSync);
    }
  }

  DVLOG(1) << "Skipping redirection: no matching app handler found";
  return nullptr;
}
