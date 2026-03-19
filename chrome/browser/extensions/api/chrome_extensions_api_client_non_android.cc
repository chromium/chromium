// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

// TODO(crbug.com/417770773): This file contains the parts of
// ChromeExtensionsAPIClient that are not yet supported on desktop Android. Once
// this file becomes minimal in size it should be folded into
// chrome_extensions_api_client.cc.

static_assert(BUILDFLAG(ENABLE_EXTENSIONS));

namespace extensions {

std::unique_ptr<UsbDevicePermissionsPrompt>
ChromeExtensionsAPIClient::CreateUsbDevicePermissionsPrompt(
    content::WebContents* web_contents) const {
  return std::make_unique<ChromeUsbDevicePermissionsPrompt>(web_contents);
}

}  // namespace extensions
