// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "desktop_media_picker.h"

#include "chrome/browser/media/webrtc/desktop_media_picker.h"

DesktopMediaPicker::Params::Params(RequestSource request_source)
    : request_source(request_source) {}
DesktopMediaPicker::Params::Params()
    : DesktopMediaPicker::Params(RequestSource::kUnknown) {}
DesktopMediaPicker::Params::Params(const Params&) = default;
DesktopMediaPicker::Params& DesktopMediaPicker::Params::operator=(
    const Params&) = default;
DesktopMediaPicker::Params::~Params() = default;
