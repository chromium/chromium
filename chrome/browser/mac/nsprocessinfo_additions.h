// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_NSPROCESSINFO_ADDITIONS_H_
#define CHROME_BROWSER_MAC_NSPROCESSINFO_ADDITIONS_H_

#import <Foundation/Foundation.h>

@interface NSProcessInfo(ChromeAdditions)
// Returns YES if the current process is the main browser process or a test
// process. A better way is to check the command line directly (i.e.
// base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessType)),
// however that's not always possible, such as within a class's +load method.
// This method returns YES for test processes because it checks for the "type"
// switch on the command line, which test processes don't have. If your code
// should not run in a test process you will need to apply additional logic.
- (BOOL)cr_isMainBrowserOrTestProcess;
@end

#endif  // CHROME_BROWSER_MAC_NSPROCESSINFO_ADDITIONS_H_
