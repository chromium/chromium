// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_recording_source.h"

#include "cc/test/fake_raster_source.h"

namespace cc {

FakeRecordingSource::FakeRecordingSource() = default;

scoped_refptr<RasterSource> FakeRecordingSource::CreateRasterSource() const {
  return FakeRasterSource::CreateFromRecordingSourceWithWaitable(
      this, playback_allowed_event_);
}

}  // namespace cc
