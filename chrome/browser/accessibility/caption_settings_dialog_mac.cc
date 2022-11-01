// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_settings_dialog.h"

#include "base/mac/mac_util.h"

namespace captions {

void CaptionSettingsDialog::ShowCaptionSettingsDialog() {
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kAccessibility_Captions);
}

}  // namespace captions
