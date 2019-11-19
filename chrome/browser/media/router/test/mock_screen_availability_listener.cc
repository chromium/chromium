// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/test/mock_screen_availability_listener.h"

namespace media_router {

MockScreenAvailabilityListener::MockScreenAvailabilityListener(
    const GURL& availability_url)
    : availability_url_(availability_url) {}

MockScreenAvailabilityListener::~MockScreenAvailabilityListener() {}

GURL MockScreenAvailabilityListener::GetAvailabilityUrl() {
  return availability_url_;
}

}  // namespace media_router
