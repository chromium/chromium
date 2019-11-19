// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/icon_bundle.h"

#include <utility>

namespace notifications {

IconBundle::IconBundle() : resource_id(0) {}
IconBundle::IconBundle(SkBitmap skbitmap)
    : bitmap(std::move(skbitmap)), resource_id(0) {}
IconBundle::IconBundle(int resource_id) : resource_id(resource_id) {}
IconBundle::IconBundle(const IconBundle& other) = default;
IconBundle::~IconBundle() = default;

}  // namespace notifications
