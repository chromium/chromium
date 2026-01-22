// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/content_annotator/content_annotator_service.h"

#include "chrome/browser/profiles/profile.h"

namespace accessibility_annotator {

ContentAnnotatorService::ContentAnnotatorService(Profile* profile)
    : profile_(profile) {}

ContentAnnotatorService::~ContentAnnotatorService() = default;

}  // namespace accessibility_annotator
