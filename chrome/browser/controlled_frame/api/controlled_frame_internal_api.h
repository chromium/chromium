// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_API_CONTROLLED_FRAME_INTERNAL_API_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_API_CONTROLLED_FRAME_INTERNAL_API_H_

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

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_API_CONTROLLED_FRAME_INTERNAL_API_H_
