// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

namespace safe_browsing {

GeminiAntiscamProtectionService::GeminiAntiscamProtectionService(
    OptimizationGuideKeyedService* optimization_guide_keyed_service)
    : optimization_guide_keyed_service_(optimization_guide_keyed_service) {}

GeminiAntiscamProtectionService::~GeminiAntiscamProtectionService() = default;

}  // namespace safe_browsing
