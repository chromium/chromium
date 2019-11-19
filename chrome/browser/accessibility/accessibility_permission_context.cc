// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_permission_context.h"

AccessibilityPermissionContext::AccessibilityPermissionContext(Profile* profile)
    : PermissionContextBase(
          profile,
          ContentSettingsType::ACCESSIBILITY_EVENTS,
          blink::mojom::FeaturePolicyFeature::kAccessibilityEvents) {}

AccessibilityPermissionContext::~AccessibilityPermissionContext() = default;

bool AccessibilityPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}
