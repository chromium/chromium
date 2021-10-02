// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/mac_intent_picker_helpers.h"

#import <Cocoa/Cocoa.h>
#import <SafariServices/SafariServices.h>

#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/elapsed_timer.h"
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

}  // namespace

absl::optional<IntentPickerAppInfo> FindMacAppForUrl(const GURL& url) {
  if (@available(macOS 10.15, *)) {
    // This function is called synchronously on the main thread for every
    // navigation, which means it needs to be fast. Unfortunately, for some
    // machines, -[SFUniversalLink initWithWebpageURL:] is consistently slow
    // (https://crbug.com/1228740, FB9364726). Therefore, time all executions of
    // that API, and if it ever runs too slowly, stop calling it in an attempt
    // to preserve the user experience. 100ms is the speed of human perception;
    // this API call must never be allowed to push a navigation from being
    // perceived as "instant" to not being perceived as "instant". See
    // https://web.dev/rail/ for more philosophy.
    static bool api_is_fast_enough = true;
    if (!api_is_fast_enough)
      return absl::nullopt;

    NSURL* nsurl = net::NSURLWithGURL(url);
    if (!nsurl)
      return absl::nullopt;

    base::ElapsedTimer timer;
    SFUniversalLink* link =
        [[[SFUniversalLink alloc] initWithWebpageURL:nsurl] autorelease];
    base::TimeDelta api_duration = timer.Elapsed();
    static constexpr auto kHowFastIsFastEnough = base::Milliseconds(100);
    if (api_duration > kHowFastIsFastEnough) {
      api_is_fast_enough = false;
      // In doing metrics, allow for hangs up to an hour. This is exceptionally
      // pessimistic, but will provide useful data for feedback to Apple.
      base::UmaHistogramLongTimes("Mac.UniversalLink.APIDuration",
                                  api_duration);
    }

    if (link)
      return AppInfoForAppUrl(link.applicationURL);
  }

  return absl::nullopt;
}

void LaunchMacApp(const GURL& url, const std::string& launch_name) {
  [[NSWorkspace sharedWorkspace]
                  openURLs:@[ net::NSURLWithGURL(url) ]
      withApplicationAtURL:[NSURL fileURLWithPath:base::SysUTF8ToNSString(
                                                      launch_name)]
                   options:0
             configuration:@{}
                     error:nil];
}

}  // namespace apps
