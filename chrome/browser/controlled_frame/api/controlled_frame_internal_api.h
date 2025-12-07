// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_API_CONTROLLED_FRAME_INTERNAL_API_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_API_CONTROLLED_FRAME_INTERNAL_API_H_

#include <string>

#include "extensions/browser/api/guest_view/web_view/web_view_internal_api.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace controlled_frame {

// Handles the <controlledframe> contextMenus.create() API.
class ControlledFrameInternalContextMenusCreateFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("controlledFrameInternal.contextMenusCreate",
                             CONTROLLEDFRAMEINTERNAL_CONTEXTMENUSCREATE)
  ControlledFrameInternalContextMenusCreateFunction() = default;

  ControlledFrameInternalContextMenusCreateFunction(
      const ControlledFrameInternalContextMenusCreateFunction&) = delete;
  ControlledFrameInternalContextMenusCreateFunction& operator=(
      const ControlledFrameInternalContextMenusCreateFunction&) = delete;

 protected:
  ~ControlledFrameInternalContextMenusCreateFunction() override = default;

  // ExtensionFunction implementation.
  ResponseAction Run() override;
};

// Handles the <controlledframe> contextMenus.update() API.
class ControlledFrameInternalContextMenusUpdateFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("controlledFrameInternal.contextMenusUpdate",
                             CONTROLLEDFRAMEINTERNAL_CONTEXTMENUSUPDATE)
  ControlledFrameInternalContextMenusUpdateFunction() = default;

  ControlledFrameInternalContextMenusUpdateFunction(
      const ControlledFrameInternalContextMenusUpdateFunction&) = delete;
  ControlledFrameInternalContextMenusUpdateFunction& operator=(
      const ControlledFrameInternalContextMenusUpdateFunction&) = delete;

 protected:
  ~ControlledFrameInternalContextMenusUpdateFunction() override = default;

  // ExtensionFunction implementation.
  ResponseAction Run() override;
};

// Handles the <controlledframe> setClientHintsUABrandEnabled() API.
class ControlledFrameInternalSetClientHintsEnabledFunction
    : public extensions::WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("controlledFrameInternal.setClientHintsEnabled",
                             CONTROLLEDFRAMEINTERNAL_SETCLIENTHINTSENABLED)
  ControlledFrameInternalSetClientHintsEnabledFunction() = default;

  ControlledFrameInternalSetClientHintsEnabledFunction(
      const ControlledFrameInternalSetClientHintsEnabledFunction&) = delete;
  ControlledFrameInternalSetClientHintsEnabledFunction& operator=(
      const ControlledFrameInternalSetClientHintsEnabledFunction&) = delete;

 protected:
  ~ControlledFrameInternalSetClientHintsEnabledFunction() override = default;

  // ExtensionFunction implementation.
  ResponseAction Run() override;

  // extensions::WebViewInternalExtensionFunction
  bool PreRunValidation(std::string* error) override;
};

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_API_CONTROLLED_FRAME_INTERNAL_API_H_
