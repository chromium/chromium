// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_action_properties.h"

namespace chrome {

DEFINE_UI_CLASS_PROPERTY_KEY(WindowOpenDisposition,
                             kDispositionKey,
                             WindowOpenDisposition::UNKNOWN)

DEFINE_UI_CLASS_PROPERTY_KEY(ActionInvocationSource,
                             kActionInvocationSourceKey,
                             ActionInvocationSource::kUnknown)

}  // namespace chrome

DEFINE_UI_CLASS_PROPERTY_TYPE(WindowOpenDisposition)
DEFINE_UI_CLASS_PROPERTY_TYPE(chrome::ActionInvocationSource)
