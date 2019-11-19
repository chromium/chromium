// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/mac_apps_navigation_throttle.h"

#import <Cocoa/Cocoa.h>
#include <dlfcn.h>

#include "base/mac/sdk_forward_declarations.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/mac/url_conversions.h"

namespace apps {

namespace {

const char kSafariServicesFrameworkPath[] =
    "/System/Library/Frameworks/SafariServices.framework/"
    "Versions/Current/SafariServices";

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

  return IntentPickerAppInfo(PickerEntryType::kMacNative, gfx::Image(app_icon),
                             base::SysNSStringToUTF8([app_url path]),
                             base::SysNSStringToUTF8(app_name));
}

// TODO(avi): When we move to the 10.15 SDK, use correct weak-linking of this
// framework rather than dlopen(), and correct @available syntax to access this
// class.
SFUniversalLink* GetUniversalLink(const GURL& url) {
  static void* safari_services = []() -> void* {
    if (@available(macOS 10.15, *))
      return dlopen(kSafariServicesFrameworkPath, RTLD_LAZY);
    return nullptr;
  }();

  static const Class SFUniversalLink_class =
      NSClassFromString(@"SFUniversalLink");

  if (!safari_services || !SFUniversalLink_class)
    return nil;

  return [[[SFUniversalLink_class alloc]
      initWithWebpageURL:net::NSURLWithGURL(url)] autorelease];
}

base::Optional<IntentPickerAppInfo> AppInfoForUrl(const GURL& url) {
  SFUniversalLink* link = GetUniversalLink(url);
  if (link)
    return AppInfoForAppUrl(link.applicationURL);

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
  if (entry_type == PickerEntryType::kMacNative) {
    if (close_reason == apps::IntentPickerCloseReason::OPEN_APP) {
      [[NSWorkspace sharedWorkspace]
                      openURLs:@[ net::NSURLWithGURL(url) ]
          withApplicationAtURL:[NSURL fileURLWithPath:base::SysUTF8ToNSString(
                                                          launch_name)]
                       options:0
                 configuration:@{}
                         error:nil];
    }
    PickerAction action = apps::AppsNavigationThrottle::GetPickerAction(
        entry_type, close_reason, should_persist);
    Platform platform = apps::AppsNavigationThrottle::GetDestinationPlatform(
        launch_name, action);
    apps::AppsNavigationThrottle::RecordUma(launch_name, entry_type,
                                            close_reason, Source::kHttpOrHttps,
                                            should_persist, action, platform);
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