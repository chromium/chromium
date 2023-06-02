// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/mac_intent_picker_helpers.h"

#import <Cocoa/Cocoa.h>
#import <SafariServices/SafariServices.h>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/mac/launch_application.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_features.h"
#include "net/base/mac/url_conversions.h"
#include "ui/base/models/image_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace apps {

namespace {

bool& UseFakeAppForTesting() {
  static bool value = false;
  return value;
}

std::string& FakeAppForTesting() {
  static base::NoDestructor<std::string> value;
  return *value;
}

IntentPickerAppInfo AppInfoForAppUrl(NSURL* app_url) {
  NSString* app_name = nil;
  if (![app_url getResourceValue:&app_name
                          forKey:NSURLLocalizedNameKey
                           error:nil]) {
    // This shouldn't happen but just in case.
    app_name = app_url.lastPathComponent;
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
  if (UseFakeAppForTesting()) {
    std::string fake_app = FakeAppForTesting();
    if (fake_app.empty())
      return absl::nullopt;

    return AppInfoForAppUrl(
        [NSURL fileURLWithPath:base::SysUTF8ToNSString(fake_app)]);
  }

  static bool universal_links_enabled =
      base::FeatureList::IsEnabled(features::kEnableUniveralLinks);
  if (!universal_links_enabled)
    return absl::nullopt;

  if (@available(macOS 10.15, *)) {
    NSURL* nsurl = net::NSURLWithGURL(url);
    if (!nsurl)
      return absl::nullopt;

    SFUniversalLink* link = [[SFUniversalLink alloc] initWithWebpageURL:nsurl];

    if (link)
      return AppInfoForAppUrl(link.applicationURL);
  }

  return absl::nullopt;
}

void LaunchMacApp(const GURL& url, const std::string& launch_name) {
  base::mac::LaunchApplication(base::FilePath(launch_name),
                               /*command_line_args=*/{}, {url.spec()},
                               /*options=*/{}, base::DoNothing());
}

void OverrideMacAppForUrlForTesting(bool fake, const std::string& app_path) {
  UseFakeAppForTesting() = fake;
  FakeAppForTesting() = app_path;
}

}  // namespace apps
