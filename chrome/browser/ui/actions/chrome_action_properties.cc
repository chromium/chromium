// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_action_properties.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(WindowOpenDisposition)
DEFINE_UI_CLASS_PROPERTY_TYPE(const GURL*)
DEFINE_UI_CLASS_PROPERTY_TYPE(const url::Origin*)

namespace chrome {

DEFINE_UI_CLASS_PROPERTY_KEY(WindowOpenDisposition,
                             kDispositionKey,
                             WindowOpenDisposition::UNKNOWN)

DEFINE_UI_CLASS_PROPERTY_KEY(const GURL*, kLinkUrlKey, nullptr)

DEFINE_UI_CLASS_PROPERTY_KEY(const GURL*, kFrameUrlKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(const url::Origin*, kFrameOriginKey, nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(int, kReferrerPolicyKey, 0)

}  // namespace chrome
