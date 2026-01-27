// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace accessibility_annotator {

class ContentAnnotatorService : public KeyedService {
 public:
  ContentAnnotatorService();
  ~ContentAnnotatorService() override;

  ContentAnnotatorService(const ContentAnnotatorService&) = delete;
  ContentAnnotatorService& operator=(const ContentAnnotatorService&) = delete;
};

}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_
