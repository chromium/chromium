// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/mac/nsprocessinfo_additions.h"

#import "content/public/common/content_switches.h"

@implementation NSProcessInfo(ChromeAdditions)

- (BOOL)cr_isMainBrowserOrTestProcess {
  NSString* processTypeString =
      [NSString stringWithFormat:@"--%s=", switches::kProcessType];

  for (NSString *argument in [self arguments]) {
    if ([argument hasPrefix:processTypeString])
      return NO;
  }
  return YES;
}

@end
