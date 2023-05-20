// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_TYPES_H_
#define ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_TYPES_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

// Lightweight course definition. Created from `google_apis::classroom::Course`
// (google_apis/classroom/classroom_api_courses_response_types.h).
// API definition:
// https://developers.google.com/classroom/reference/rest/v1/courses.
struct ASH_EXPORT GlanceablesClassroomCourse {
  GlanceablesClassroomCourse(const std::string& id, const std::string& title);
  GlanceablesClassroomCourse(const GlanceablesClassroomCourse&) = delete;
  GlanceablesClassroomCourse& operator=(const GlanceablesClassroomCourse&) =
      delete;
  ~GlanceablesClassroomCourse() = default;

  // Identifier for this course assigned by Classroom.
  const std::string id;

  // Name of the course. For example, "10th Grade Biology".
  const std::string name;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_TYPES_H_
