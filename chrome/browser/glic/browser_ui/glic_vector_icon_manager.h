// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_VECTOR_ICON_MANAGER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_VECTOR_ICON_MANAGER_H_

#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "ui/gfx/vector_icon_types.h"

namespace glic {

// Handles loading vector icon data from a resource, storing the resulting
// representation and vending references to gfx::VectorIcons for use.
class GlicVectorIconManager {
 public:
  // If the resources corresponding to `id` is not available at runtime, this
  // method will crash.
  static const gfx::VectorIcon& GetVectorIcon(int id);
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_VECTOR_ICON_MANAGER_H_
