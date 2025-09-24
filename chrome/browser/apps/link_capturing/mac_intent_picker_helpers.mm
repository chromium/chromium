// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/mac_intent_picker_helpers.h"

#import <Cocoa/Cocoa.h>
#import <SafariServices/SafariServices.h>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/mac/launch_application.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/apple/url_conversions.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"

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

NSImage* CreateRedIconForTesting() {
  return [NSImage imageWithSize:NSMakeSize(16, 16)
                        flipped:NO
                 drawingHandler:^(NSRect rect) {
                   [NSColor.redColor set];
                   NSRectFill(rect);
                   return YES;
                 }];
}

MacAppInfo AppInfoForAppUrl(NSURL* app_url, base::span<int> icon_sizes) {
  CHECK(!icon_sizes.empty());
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
    // This shouldn't happen, but just in case. (Note that, despite its name,
    // NSImageNameApplicationIcon is the icon of "this app". There is no
    // constant for "generic app icon", only this string value. This value has
    // been verified to exist from macOS 10.15 through macOS 14; see -[NSImage
    // _systemImageNamed:].)
    app_icon = [NSImage imageNamed:@"NSDefaultApplicationIcon"];
  }
  if (UseFakeAppForTesting()) {            // IN-TEST
    app_icon = CreateRedIconForTesting();  // IN-TEST
  }

  gfx::ImageFamily image_family;
  if (app_icon) {
    for (int icon_size : icon_sizes) {
      CHECK_GT(icon_size, 0);
      auto image = gfx::ImageSkiaFromResizedNSImage(
          app_icon, NSMakeSize(icon_size, icon_size));
      image.SetReadOnly();
      image_family.Add(std::move(image));
    }
  }

  return MacAppInfo{{PickerEntryType::kMacOs, ui::ImageModel(),
                     base::SysNSStringToUTF8(app_url.path),
                     base::SysNSStringToUTF8(app_name)},
                    std::move(image_family)};
}

}  // namespace

std::optional<MacAppInfo> FindMacAppForUrl(const GURL& url,
                                           base::span<int> icon_sizes) {
  if (UseFakeAppForTesting()) {
    std::string fake_app = FakeAppForTesting();  // IN-TEST
    if (fake_app.empty()) {
      return std::nullopt;
    }

    return AppInfoForAppUrl(
        [NSURL fileURLWithPath:base::SysUTF8ToNSString(fake_app)], icon_sizes);
  }

  NSURL* nsurl = net::NSURLWithGURL(url);
  if (!nsurl) {
    return std::nullopt;
  }

  SFUniversalLink* link = [[SFUniversalLink alloc] initWithWebpageURL:nsurl];

  if (link) {
    return AppInfoForAppUrl(link.applicationURL, icon_sizes);
  }

  return std::nullopt;
}

void LaunchMacApp(const GURL& url,
                  const std::string& launch_name,
                  base::OnceClosure callback) {
  base::mac::LaunchApplication(
      base::FilePath(launch_name),
      /*command_line_args=*/{}, {url.spec()},
      /*options=*/{},
      base::IgnoreArgs<NSRunningApplication*, NSError*>(std::move(callback)));
}

void OverrideMacAppForUrlForTesting(bool fake, const std::string& app_path) {
  UseFakeAppForTesting() = fake;   // IN-TEST
  FakeAppForTesting() = app_path;  // IN-TEST
}

}  // namespace apps
