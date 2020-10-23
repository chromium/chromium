// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/mac_apps_navigation_throttle.h"

#import <Cocoa/Cocoa.h>
#import <SafariServices/SafariServices.h>

#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/mac/url_conversions.h"

namespace apps {

namespace {

IntentPickerAppInfo AppInfoForAppUrl(NSURL* app_url) {
  NSString* app_name = nil;
  if (![app_url getResourceValue:&app_name
                          forKey:NSURLLocalizedNameKey
                           error:nil]) {
    // This shouldn't happen but just in case.
    app_name = [app_url lastPathComponent];
  }
  NSImage* app_icon = nil;
  if (![app_url getResourceValue:&app_icon
                          forKey:NSURLEffectiveIconKey
                           error:nil]) {
    // This shouldn't happen but just in case.
    app_icon = [NSImage imageNamed:NSImageNameApplicationIcon];
  }
  app_icon.size = NSMakeSize(16, 16);

  return IntentPickerAppInfo(PickerEntryType::kMacOs, gfx::Image(app_icon),
                             base::SysNSStringToUTF8([app_url path]),
                             base::SysNSStringToUTF8(app_name));
}

base::Optional<IntentPickerAppInfo> AppInfoForUrl(const GURL& url) {
  if (@available(macOS 10.15, *)) {
    NSURL* nsurl = net::NSURLWithGURL(url);
    if (!nsurl)
      return base::nullopt;

    SFUniversalLink* link =
        [[[SFUniversalLink alloc] initWithWebpageURL:nsurl] autorelease];
    if (link)
      return AppInfoForAppUrl(link.applicationURL);
  }

  return base::nullopt;
}

}  // namespace

// static
std::unique_ptr<apps::AppsNavigationThrottle>
MacAppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nullptr;

  if (!apps::AppsNavigationThrottle::CanCreate(handle->GetWebContents()))
    return nullptr;

  return std::make_unique<MacAppsNavigationThrottle>(handle);
}

// static
void MacAppsNavigationThrottle::ShowIntentPickerBubble(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  std::vector<IntentPickerAppInfo> apps;

  // First, the Universal Link, if there is one.
  if (auto app_info = AppInfoForUrl(url))
    apps.push_back(std::move(app_info.value()));

  // Then, any PWAs.
  apps = apps::AppsNavigationThrottle::FindPwaForUrl(web_contents, url,
                                                     std::move(apps));

  bool show_persistence_options = ShouldShowPersistenceOptions(apps);
  apps::AppsNavigationThrottle::ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps),
      /*show_stay_in_chrome=*/show_persistence_options,
      /*show_remember_selection=*/show_persistence_options,
      base::BindOnce(&OnIntentPickerClosed, web_contents,
                     ui_auto_display_service, url));
}

MacAppsNavigationThrottle::MacAppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : apps::AppsNavigationThrottle(navigation_handle) {}

MacAppsNavigationThrottle::~MacAppsNavigationThrottle() = default;

std::vector<IntentPickerAppInfo> MacAppsNavigationThrottle::FindAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps) {
  // First, the Universal Link, if there is one.
  if (auto app_info = AppInfoForUrl(url))
    apps.push_back(std::move(app_info.value()));

  // Then, any PWAs.
  apps = apps::AppsNavigationThrottle::FindPwaForUrl(web_contents, url,
                                                     std::move(apps));

  return apps;
}

// static
void MacAppsNavigationThrottle::OnIntentPickerClosed(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type,
    apps::IntentPickerCloseReason close_reason,
    bool should_persist) {
  if (entry_type == PickerEntryType::kMacOs) {
    if (close_reason == apps::IntentPickerCloseReason::OPEN_APP) {
      [[NSWorkspace sharedWorkspace]
                      openURLs:@[ net::NSURLWithGURL(url) ]
          withApplicationAtURL:[NSURL fileURLWithPath:base::SysUTF8ToNSString(
                                                          launch_name)]
                       options:0
                 configuration:@{}
                         error:nil];
    }
    return;
  }
  apps::AppsNavigationThrottle::OnIntentPickerClosed(
      web_contents, ui_auto_display_service, url, launch_name, entry_type,
      close_reason, should_persist);
}

apps::AppsNavigationThrottle::PickerShowState
MacAppsNavigationThrottle::GetPickerShowState(
    const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  return PickerShowState::kOmnibox;
}

IntentPickerResponse MacAppsNavigationThrottle::GetOnPickerClosedCallback(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  return base::BindOnce(&OnIntentPickerClosed, web_contents,
                        ui_auto_display_service, url);
}

}  // namespace apps
