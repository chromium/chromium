// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace accessibility_annotator {

class ContentAnnotatorService : public KeyedService {
 public:
  explicit ContentAnnotatorService(Profile* profile);
  ~ContentAnnotatorService() override;

  ContentAnnotatorService(const ContentAnnotatorService&) = delete;
  ContentAnnotatorService& operator=(
      const ContentAnnotatorService&) = delete;

 private:
  // Not owned. Guaranteed to outlive `this`.
  raw_ptr<Profile> profile_;
};

}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_
