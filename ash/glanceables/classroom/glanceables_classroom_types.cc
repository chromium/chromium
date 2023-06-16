// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_types.h"

#include <string>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {

// ----------------------------------------------------------------------------
// GlanceablesClassroomCourse:

GlanceablesClassroomCourse::GlanceablesClassroomCourse(const std::string& id,
                                                       const std::string& name)
    : id(id), name(name) {}

// ----------------------------------------------------------------------------
// GlanceablesClassroomCourseWorkItem:

GlanceablesClassroomCourseWorkItem::GlanceablesClassroomCourseWorkItem(
    const std::string& id,
    const std::string& title,
    const GURL& link,
    const absl::optional<base::Time>& due)
    : id(id), title(title), link(link), due(due) {}

// ----------------------------------------------------------------------------
// GlanceablesClassroomStudentSubmission:

GlanceablesClassroomStudentSubmission::GlanceablesClassroomStudentSubmission(
    const std::string& id,
    const std::string& course_work_id,
    State state)
    : id(id), course_work_id(course_work_id), state(state) {}

// ----------------------------------------------------------------------------
// GlanceablesClassroomStudentAssignment:

GlanceablesClassroomStudentAssignment::GlanceablesClassroomStudentAssignment(
    const std::string& course_title,
    const std::string& course_work_title,
    const GURL& link,
    const absl::optional<base::Time>& due)
    : course_title(course_title),
      course_work_title(course_work_title),
      link(link),
      due(due) {}

std::string GlanceablesClassroomStudentAssignment::ToString() const {
  std::stringstream ss;
  ss << "Course Title: " << course_title
     << ", Course Work Title: " << course_work_title << ", Link: " << link;
  if (due.has_value()) {
    ss << ", Due: " << base::TimeFormatHTTP(due.value());
  }
  return ss.str();
}

}  // namespace ash
