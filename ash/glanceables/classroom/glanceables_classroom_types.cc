// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_types.h"

#include <sstream>
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

std::string GlanceablesClassroomCourse::ToString() const {
  std::stringstream ss;
  ss << "id: " << id << ", name: " << name;
  return ss.str();
}

// ----------------------------------------------------------------------------
// GlanceablesClassroomCourseWorkItem:

GlanceablesClassroomCourseWorkItem::GlanceablesClassroomCourseWorkItem(
    const std::string& id,
    const std::string& title,
    const GURL& link,
    const absl::optional<base::Time>& due)
    : id(id), title(title), link(link), due(due) {}

std::string GlanceablesClassroomCourseWorkItem::ToString() const {
  std::stringstream ss;
  ss << "id: " << id << ", title: " << title << ", link: " << link;
  if (due.has_value()) {
    ss << ", due: " << base::TimeFormatHTTP(due.value());
  }
  return ss.str();
}

// ----------------------------------------------------------------------------
// GlanceablesClassroomStudentSubmission:

GlanceablesClassroomStudentSubmission::GlanceablesClassroomStudentSubmission(
    const std::string& id,
    const std::string& course_work_id,
    State state)
    : id(id), course_work_id(course_work_id), state(state) {}

std::string GlanceablesClassroomStudentSubmission::ToString() const {
  std::stringstream ss;
  ss << "id: " << id << ", course work id: " << course_work_id
     << ", State: " << static_cast<int>(state);
  return ss.str();
}

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

// ----------------------------------------------------------------------------
// GlanceablesClassroomTeacherAssignment

GlanceablesClassroomTeacherAssignment::GlanceablesClassroomTeacherAssignment(
    const std::string& course_title,
    const std::string& course_work_title,
    const GURL& link,
    const absl::optional<base::Time>& due,
    int total_submission_count,
    int number_turned_in,
    int number_graded)
    : course_title(course_title),
      course_work_title(course_work_title),
      link(link),
      due(due),
      total_submission_count(total_submission_count),
      number_turned_in(number_turned_in),
      number_graded(number_graded) {}

std::string GlanceablesClassroomTeacherAssignment::ToString() const {
  std::stringstream ss;
  ss << "Course Title: " << course_title
     << ", Course Work Title: " << course_work_title << ", Link: " << link;
  if (due.has_value()) {
    ss << ", Due: " << base::TimeFormatHTTP(due.value());
  }
  ss << ", total_submission_count: " << total_submission_count
     << ", number turned in: " << number_turned_in
     << ", number graded:" << number_graded;
  return ss.str();
}

}  // namespace ash
