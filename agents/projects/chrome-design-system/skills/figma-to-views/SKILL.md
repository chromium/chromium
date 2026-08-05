---
name: figma-to-views
description: Recreate Figma design frames as production-ready Chromium C++ Views code following Chrome's native Views architecture, component guidelines, and color/shape token mappings. Use when asked to create C++ Views code from a Figma frame URL.
---

# Figma to Views: Code Generation

## Prerequisites
- **Figma MCP Server**: Must be active and configured in the workspace context to fetch designs and metadata.
- **Chromium Repository**: This skill should be executed from inside the root directory of the Chromium repository source code (`//src/`).

---

## 1. Discovery & Context Retrieval

When given a Figma design URL (e.g., `https://www.figma.com/design/:fileKey/:fileName?node-id=:nodeId`):

1. **Extract Parameters**: Extract the `fileKey` and the `nodeId` (replace hyphens with colons, e.g., `128-1951` becomes `128:1951`).
2. **Retrieve Design Context**: Call `get_design_context` with `fileKey` and `nodeId` to fetch the metadata, layout hierarchy, layer styles, and text annotations of the frame.
3.  **Reference Existing Component Specs**: Look up component spec files for relevant components using the `project-knowledge` skill to map Figma components to Chromium C++ Views components.
4. **Reference Token Mapping**: Consult the Chrome Design System token mapping using the `project-knowledge` skill to translate the variables from the Figma design (e.g., `desktop/sys/base-colors/base`) to equivalent C++ identifiers (`ui::kColorSysBase`).

---

## 2. Design-to-Code Gap Auditing

Before writing any code, first check if the user already did an audit of this Figma design against production coding standards within the current chat session's recent history (past 5 commands). If not, perform the audit. Incorporate the feedback from this audit into the following code implementation.

---

## 3. Views Component Implementation

### Implementation Rules

- **Surface & Background Colors**: Map Figma variables strictly to their C++ `ui::ColorId` equivalents as documented in Chrome Design System token mappings.
   - Theme Inheritance: The C++ View must adapt automatically to the user's Chrome Appearance setting (e.g. light/dark mode) without hardcoded theme checks.
- **Corner Radius Tokens**: Do not hardcode arbitrary radius numbers when design system tokens apply. Retrieve standard radii dynamically from `views::LayoutProvider`.
- **Responsive Layout & Flex Spacing**: Use `views::BoxLayout` or `views::BoxLayoutView` for structured horizontal and vertical stacking.
   - Apply consistent spacing and padding using `gfx::Insets::VH(...)` and `SetBetweenChildSpacing(...)`.
   - Use `SetFlexForView(child, 1)` on horizontal or vertical layouts to distribute space proportionally.
- **Scope**: Focus strictly on building the UI. Do not add functionality beyond what is specified in
the Figma mockup. If the Figma mockup includes a window frame or top Chrome frame, disregard these.
- **Output Location**: Save the implementation files inside the  out/<ComponentName>  directory
(relative to the skill directory), unless specified otherwise.
- **Builds**: Do not run any builds.

### File 1: `<component_name>_view.h`

*Example Boilerplate:*
```cpp
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMPONENT_NAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMPONENT_NAME_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// Recreates the Figma design frame using native C++ Views controls and
// Chrome Design System token mappings.
class ComponentNameView : public views::View {
  METADATA_HEADER(ComponentNameView, views::View)

 public:
  ComponentNameView();
  ComponentNameView(const ComponentNameView&) = delete;
  ComponentNameView& operator=(const ComponentNameView&) = delete;
  ~ComponentNameView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMPONENT_NAME_VIEW_H_
```

### File 2: `<component_name>_view.cc`

*Example Boilerplate:*
```cpp
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/component_name_view.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

}  // namespace

BEGIN_METADATA(ComponentNameView)
END_METADATA

ComponentNameView::ComponentNameView() {
  // Reference: Chrome Design System token mappings.
  // Map Figma variables to C++ tokens and retrieve standard corner radii.
}
```
