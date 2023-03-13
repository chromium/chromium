// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_TREE_CONVERTER_H_
#define ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_TREE_CONVERTER_H_

namespace ash::eche_app {

// Converter to convert Android UI tree from proto to mojom format
// https://crsrc.org/c/ash/webui/eche_app_ui/proto/accessibility_mojom.proto
// https://crsrc.org/c/ash/components/arc/mojom/accessibility_helper.mojom
class AccessibilityTreeConverter {
 public:
  AccessibilityTreeConverter();
  ~AccessibilityTreeConverter();

 private:
};
}  // namespace ash::eche_app

#endif  // ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_TREE_CONVERTER_H_
