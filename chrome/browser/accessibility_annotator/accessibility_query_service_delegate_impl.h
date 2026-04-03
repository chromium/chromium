// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_QUERY_SERVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_QUERY_SERVICE_DELEGATE_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/accessibility_annotator/core/accessibility_query_service_delegate.h"

namespace accessibility_annotator {

class AccessibilityQueryServiceDelegateImpl
    : public AccessibilityQueryServiceDelegate {
 public:
  AccessibilityQueryServiceDelegateImpl();
  AccessibilityQueryServiceDelegateImpl(
      const AccessibilityQueryServiceDelegateImpl&) = delete;
  AccessibilityQueryServiceDelegateImpl& operator=(
      const AccessibilityQueryServiceDelegateImpl&) = delete;
  ~AccessibilityQueryServiceDelegateImpl() override;

  // AccessibilityQueryServiceDelegate:
  void RetrieveLiveTabContext(
      LiveTabContextQuery query,
      base::OnceCallback<void(LiveTabContextResponse)> callback) override;

 private:
  base::WeakPtrFactory<AccessibilityQueryServiceDelegateImpl> weak_ptr_factory_{
      this};
};

}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_QUERY_SERVICE_DELEGATE_IMPL_H_
