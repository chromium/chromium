// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_

#include <stdint.h>

#include "ui/accessibility/ax_enum_util.h"

namespace arc {
namespace mojom {
enum class AccessibilityEventType;
}  // namespace mojom

ax::mojom::Event ToAXEvent(mojom::AccessibilityEventType arc_event_type);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_
