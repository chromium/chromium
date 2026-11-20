// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_drive_picker_types.h"

namespace glic {

GlicDriveFileSelection::GlicDriveFileSelection() = default;
GlicDriveFileSelection::GlicDriveFileSelection(const GlicDriveFileSelection&) =
    default;
GlicDriveFileSelection& GlicDriveFileSelection::operator=(
    const GlicDriveFileSelection&) = default;
GlicDriveFileSelection::GlicDriveFileSelection(GlicDriveFileSelection&&) =
    default;
GlicDriveFileSelection& GlicDriveFileSelection::operator=(
    GlicDriveFileSelection&&) = default;
GlicDriveFileSelection::~GlicDriveFileSelection() = default;

}  // namespace glic
