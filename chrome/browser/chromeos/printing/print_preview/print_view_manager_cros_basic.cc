// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros_basic.h"

#include "content/public/browser/web_contents.h"

namespace chromeos {

PrintViewManagerCrosBasic::PrintViewManagerCrosBasic(
    content::WebContents* web_contents)
    : PrintViewManagerCrosBase(web_contents),
      content::WebContentsUserData<PrintViewManagerCrosBasic>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintViewManagerCrosBasic);
}  // namespace chromeos
