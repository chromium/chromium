// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_TYPES_H_
#define ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_TYPES_H_

#include <string>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {

// Lightweight course definition. Created from `google_apis::classroom::Course`
// (google_apis/classroom/classroom_api_courses_response_types.h).
// API definition:
// https://developers.google.com/classroom/reference/rest/v1/courses.
struct ASH_EXPORT GlanceablesClassroomCourse {
  GlanceablesClassroomCourse(const std::string& id, const std::string& name);
  GlanceablesClassroomCourse(const GlanceablesClassroomCourse&) = delete;
  GlanceablesClassroomCourse& operator=(const GlanceablesClassroomCourse&) =
      delete;
  ~GlanceablesClassroomCourse() = default;

  // Identifier for this course assigned by Classroom.
  const std::string id;

  // Name of the course. For example, "10th Grade Biology".
  const std::string name;
};

// Lightweight course work item definition. Created from
// `google_apis::classroom::CourseWorkItem`
// (google_apis/classroom/classroom_api_course_work_response_types.h).
// API definition:
// https://developers.google.com/classroom/reference/rest/v1/courses.courseWork.
struct ASH_EXPORT GlanceablesClassroomCourseWorkItem {
  GlanceablesClassroomCourseWorkItem(const std::string& id,
                                     const std::string& title,
                                     const GURL& link,
                                     const absl::optional<base::Time>& due);
  GlanceablesClassroomCourseWorkItem(
      const GlanceablesClassroomCourseWorkItem&) = delete;
  GlanceablesClassroomCourseWorkItem& operator=(
      const GlanceablesClassroomCourseWorkItem&) = delete;
  ~GlanceablesClassroomCourseWorkItem() = default;

  // Classroom-assigned identifier of this course work, unique per course.
  const std::string id;

  // Title of this course work item.
  const std::string title;

  // Absolute link to this course work in the Classroom web UI.
  const GURL link;

  // Due date and time in UTC of this course work item.
  const absl::optional<base::Time> due;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_TYPES_H_
