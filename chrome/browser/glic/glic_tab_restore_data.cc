// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_tab_restore_data.h"

namespace glic {

GlicRestoredState::GlicRestoredState() = default;
GlicRestoredState::~GlicRestoredState() = default;
GlicRestoredState::GlicRestoredState(GlicRestoredState&&) = default;
GlicRestoredState& GlicRestoredState::operator=(GlicRestoredState&&) = default;

GlicTabRestoreData::~GlicTabRestoreData() = default;

GlicTabRestoreData::GlicTabRestoreData(content::WebContents* contents,
                                       GlicRestoredState state)
    : content::WebContentsUserData<GlicTabRestoreData>(*contents),
      state_(std::move(state)) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(GlicTabRestoreData);

}  // namespace glic
