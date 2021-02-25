// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/mac_intent_picker_helpers.h"

#import <Cocoa/Cocoa.h>
#import <SafariServices/SafariServices.h>

#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/mac/url_conversions.h"
#include "ui/base/models/image_model.h"

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

  return IntentPickerAppInfo(PickerEntryType::kMacOs,
                             ui::ImageModel::FromImage(gfx::Image(app_icon)),
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

void LaunchMacApp(const GURL& url, const std::string& launch_name) {
  [[NSWorkspace sharedWorkspace]
                  openURLs:@[ net::NSURLWithGURL(url) ]
      withApplicationAtURL:[NSURL fileURLWithPath:base::SysUTF8ToNSString(
                                                      launch_name)]
                   options:0
             configuration:@{}
                     error:nil];
}

std::vector<IntentPickerAppInfo> FindMacAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps) {
  // First, the Universal Link, if there is one.
  if (auto app_info = AppInfoForUrl(url))
    apps.push_back(std::move(app_info.value()));

  return apps;
}

}  // namespace apps
