// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/desk_template.h"

#include "base/time/time.h"

namespace ash {

DeskTemplate::DeskTemplate() : uuid_(base::Time::Now().ToDoubleT()) {}
DeskTemplate::DeskTemplate(double uuid) : uuid_(uuid) {}
DeskTemplate::~DeskTemplate() = default;

}  // namespace ash
