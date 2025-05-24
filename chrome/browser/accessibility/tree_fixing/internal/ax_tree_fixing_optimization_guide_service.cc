// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/tree_fixing/internal/ax_tree_fixing_optimization_guide_service.h"

#include "base/check.h"

namespace tree_fixing {

AXTreeFixingOptimizationGuideService::AXTreeFixingOptimizationGuideService(
    HeadingsIdentificationDelegate& delegate,
    Profile* profile)
    : headings_identification_delegate_(delegate), profile_(profile) {
  CHECK(profile_);
  Initialize();
}

AXTreeFixingOptimizationGuideService::~AXTreeFixingOptimizationGuideService() =
    default;

void AXTreeFixingOptimizationGuideService::Initialize() {
  // TODO(401035166): Setup connection to optimization guide.
}

void AXTreeFixingOptimizationGuideService::IdentifyHeadings(
    const ui::AXTreeUpdate& ax_tree_update,
    const SkBitmap& bitmap,
    int request_id) {
  // TODO(401035166): Implement call to optimization guide service.
}

}  // namespace tree_fixing
