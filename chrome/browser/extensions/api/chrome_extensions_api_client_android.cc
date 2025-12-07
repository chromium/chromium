// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"

#include <memory>

#include "base/notimplemented.h"
#include "extensions/buildflags/buildflags.h"

// TODO(crbug.com/417770773): This file contains stubs for the parts of
// ChromeExtensionsAPIClient that are not yet supported on desktop Android. Once
// these functions are supported on desktop Android this file can be deleted.
// The stubs are implemented here instead of falling back to ExtensionsAPIClient
// to allow NOTIMPLEMENTED() logging and a place to put TODOs with bug IDs.

static_assert(BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS));

namespace extensions {

std::unique_ptr<DevicePermissionsPrompt>
ChromeExtensionsAPIClient::CreateDevicePermissionsPrompt(
    content::WebContents* web_contents) const {
  // TODO(crbug.com/417785993): Support device permissions prompts on desktop
  // Android.
  return nullptr;
}

std::unique_ptr<DisplayInfoProvider>
ChromeExtensionsAPIClient::CreateDisplayInfoProvider() const {
  // TODO(crbug.com/417786011): Support display APIs on desktop Android.
  NOTIMPLEMENTED();
  return nullptr;
}

std::vector<KeyedServiceBaseFactory*>
ChromeExtensionsAPIClient::GetFactoryDependencies() {
  // TODO(crbug.com/402488726): Delete this stub and use the version from
  // _non_android.cc when we have supervised user support on desktop Android.
  // Don't use NOTIMPLEMENTED() here because this is the correct implementation
  // for this stub class.
  return {};
}

}  // namespace extensions
