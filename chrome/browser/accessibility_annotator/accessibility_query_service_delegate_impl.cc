// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_query_service_delegate_impl.h"

#include <utility>

#include "base/functional/callback.h"

namespace accessibility_annotator {

AccessibilityQueryServiceDelegateImpl::AccessibilityQueryServiceDelegateImpl() =
    default;

AccessibilityQueryServiceDelegateImpl::
    ~AccessibilityQueryServiceDelegateImpl() = default;

void AccessibilityQueryServiceDelegateImpl::RetrieveLiveTabContext(
    LiveTabContextQuery query,
    base::OnceCallback<void(LiveTabContextResponse)> callback) {
  // TODO(b/488694873): Fetch all tabs from the browser.
  // TODO(b/488696556): Fetch tabs intelligently using ContextualTasksService.
  // TODO(b/488694873): Implement using LiveTabResponder.
  std::move(callback).Run({});
}

}  // namespace accessibility_annotator
