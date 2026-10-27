// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_row_view_model.h"

namespace glic {

GlicActivityRowViewModel::GlicActivityRowViewModel() = default;

GlicActivityRowViewModel::GlicActivityRowViewModel(
    const GlicActivityRowViewModel&) = default;

GlicActivityRowViewModel::GlicActivityRowViewModel(GlicActivityRowViewModel&&) =
    default;

GlicActivityRowViewModel& GlicActivityRowViewModel::operator=(
    const GlicActivityRowViewModel&) = default;

GlicActivityRowViewModel& GlicActivityRowViewModel::operator=(
    GlicActivityRowViewModel&&) = default;

GlicActivityRowViewModel::~GlicActivityRowViewModel() = default;

}  // namespace glic
