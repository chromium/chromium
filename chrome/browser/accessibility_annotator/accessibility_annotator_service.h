// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_SERVICE_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace accessibility_annotator {

class AccessibilityAnnotatorService : public KeyedService {
 public:
  explicit AccessibilityAnnotatorService(Profile* profile);
  ~AccessibilityAnnotatorService() override;

  AccessibilityAnnotatorService(const AccessibilityAnnotatorService&) = delete;
  AccessibilityAnnotatorService& operator=(
      const AccessibilityAnnotatorService&) = delete;

 private:
  // Not owned. Guaranteed to outlive `this`.
  raw_ptr<Profile> profile_;
};

}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_SERVICE_H_
