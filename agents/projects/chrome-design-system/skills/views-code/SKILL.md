---
name: views-code
description: Use this skill when generating or reviewing C++ Views code. This skill contains knowledge on C++ Views code following Chrome's native Views architecture and component guidelines.
metadata:
  author: Chrome Design System Team
---

# Views: Code Guidelines

## General Views Guidelines

- Adhere to the official
  [Chromium Views documentation](//src/docs/ui/views/overview.md).
- Adhering to the Chrome Design System. Reference component specs for relevant
  components using the `chrome-components` skill. Ensure code follows the
  guidelines for each component.
- As much as possible, prefer leverage existing Views components rather than
  creating new, bespoke components. Views components are located under
  `ui/views/` and `chrome/browser/ui/views/`.

______________________________________________________________________

## Implementation Rules

- **Theme Inheritance**: The C++ View must adapt automatically to the user's
  Chrome Appearance setting (e.g. light/dark mode) without hardcoded theme
  checks.
- **Corner Radius Tokens**: Do not hardcode arbitrary radius numbers when design
  system tokens apply. Retrieve standard radii dynamically from
  `views::LayoutProvider`.
- **Responsive Layout & Flex Spacing**: Use `views::BoxLayout` or
  `views::BoxLayoutView` for structured horizontal and vertical stacking.
  - Apply consistent spacing and padding using `gfx::Insets::VH(...)` and
    `SetBetweenChildSpacing(...)`.
  - Use `SetFlexForView(child, 1)` on horizontal or vertical layouts to
    distribute space proportionally.
  - Layout options include `BoxLayout`, `BoxLayoutView`, `FlexLayout`,
    `FlexLayoutView`, `TableLayout` or `TableLayoutView` for structured
    horizontal and vertical stacking, or fully structured tables.

______________________________________________________________________

## Component Implementation

When implementing a new component, create these files.

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

// Recreates the design using native C++ Views controls and
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
  // Map variables to C++ tokens and retrieve standard corner radii.
}
```
