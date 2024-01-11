// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_EXTENSIONS_BROWSER_API_PROVIDER_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_EXTENSIONS_BROWSER_API_PROVIDER_H_

#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extensions_browser_api_provider.h"

namespace controlled_frame {

// Registers the internal API implementations for Controlled Frame for the
// extensions system.
class ControlledFrameExtensionsBrowserAPIProvider
    : public extensions::ExtensionsBrowserAPIProvider {
 public:
  ControlledFrameExtensionsBrowserAPIProvider() = default;
  ~ControlledFrameExtensionsBrowserAPIProvider() override = default;

  ControlledFrameExtensionsBrowserAPIProvider(
      const ControlledFrameExtensionsBrowserAPIProvider&) = delete;
  ControlledFrameExtensionsBrowserAPIProvider& operator=(
      const ControlledFrameExtensionsBrowserAPIProvider&) = delete;

  void RegisterExtensionFunctions(ExtensionFunctionRegistry* registry) override;
};

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_EXTENSIONS_BROWSER_API_PROVIDER_H_
