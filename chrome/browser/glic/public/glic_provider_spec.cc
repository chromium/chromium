// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_spec.h"

#include "base/check_is_test.h"

namespace glic {

GlicProviderKey GlicProviderSpec::GetProviderKeyForTesting() const {
  CHECK_IS_TEST();
  return GetProviderKeyImpl();
}

GlicSelectionInlineCueFeatureParams::GlicSelectionInlineCueFeatureParams() =
    default;
GlicSelectionInlineCueFeatureParams::GlicSelectionInlineCueFeatureParams(
    const GlicSelectionInlineCueFeatureParams&) = default;
GlicSelectionInlineCueFeatureParams&
GlicSelectionInlineCueFeatureParams::operator=(
    const GlicSelectionInlineCueFeatureParams&) = default;
GlicSelectionInlineCueFeatureParams::GlicSelectionInlineCueFeatureParams(
    GlicSelectionInlineCueFeatureParams&&) = default;
GlicSelectionInlineCueFeatureParams&
GlicSelectionInlineCueFeatureParams::operator=(
    GlicSelectionInlineCueFeatureParams&&) = default;
GlicSelectionInlineCueFeatureParams::~GlicSelectionInlineCueFeatureParams() =
    default;

}  // namespace glic
