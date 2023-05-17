// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_types.h"

#include <string>

namespace ash {

GlanceablesClassroomCourse::GlanceablesClassroomCourse(const std::string& id,
                                                       const std::string& name)
    : id(id), name(name) {}

}  // namespace ash
