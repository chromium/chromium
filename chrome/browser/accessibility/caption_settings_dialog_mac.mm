// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_settings_dialog.h"

#import <AppKit/AppKit.h>

namespace captions {

static NSString* kCaptionSettingsUrlString =
    @"x-apple.systempreferences:com.apple.preference.universalaccess?"
    @"Captioning";

void CaptionSettingsDialog::ShowCaptionSettingsDialog() {
  [[NSWorkspace sharedWorkspace]
      openURL:[NSURL URLWithString:kCaptionSettingsUrlString]];
}

}  // namespace captions
