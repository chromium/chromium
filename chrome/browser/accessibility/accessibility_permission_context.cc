// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_permission_context.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

AccessibilityPermissionContext::AccessibilityPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::ACCESSIBILITY_EVENTS,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

AccessibilityPermissionContext::~AccessibilityPermissionContext() = default;

bool AccessibilityPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}
