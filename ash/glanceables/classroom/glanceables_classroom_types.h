// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_TYPES_H_
#define ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_TYPES_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/time/time.h"
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

  // Intended for debugging.
  std::string ToString() const;

  // Identifier for this course assigned by Classroom.
  const std::string id;

  // Name of the course. For example, "10th Grade Biology".
  const std::string name;
};

// Student submissions state aggregated from a list of student submissions,
// usually associated with a course work item.
struct ASH_EXPORT GlanceablesClassroomAggregatedSubmissionsState {
  GlanceablesClassroomAggregatedSubmissionsState() = default;
  GlanceablesClassroomAggregatedSubmissionsState(int total_count,
                                                 int number_turned_in,
                                                 int number_graded);
  GlanceablesClassroomAggregatedSubmissionsState(
      const GlanceablesClassroomAggregatedSubmissionsState&) = default;
  GlanceablesClassroomAggregatedSubmissionsState& operator=(
      const GlanceablesClassroomAggregatedSubmissionsState&) = default;
  ~GlanceablesClassroomAggregatedSubmissionsState() = default;

  void Reset();

  // The total number of students that have this assigned to them.
  int total_count = 0;

  // The number of this assignment that has been turned in students.
  int number_turned_in = 0;

  // The number of this assignment that has already been graded.
  int number_graded = 0;
};

// Represents a single classroom assignment.  This data is aggregated from all
// student submissions for this assignment.
struct ASH_EXPORT GlanceablesClassroomAssignment {
 public:
  GlanceablesClassroomAssignment(
      const std::string& course_title,
      const std::string& course_work_title,
      const GURL& link,
      const std::optional<base::Time>& due,
      const base::Time& last_update,
      std::optional<GlanceablesClassroomAggregatedSubmissionsState>
          submissions_state);
  GlanceablesClassroomAssignment(const GlanceablesClassroomAssignment&) =
      delete;
  GlanceablesClassroomAssignment& operator=(
      const GlanceablesClassroomAssignment&) = delete;
  ~GlanceablesClassroomAssignment() = default;

  // Intended for debugging.
  std::string ToString() const;

  // Title of the course this assignment belongs to.
  const std::string course_title;

  // Title of the course work item this assignment belongs to.
  const std::string course_work_title;

  // Absolute link for redirects to Classroom web UI.
  const GURL link;

  // Due date and time in UTC of this course work item.
  const std::optional<base::Time> due;

  // The timestamp of the last course work item update.
  const base::Time last_update;

  // Stats about overall student submissions state of the assignment.
  const std::optional<GlanceablesClassroomAggregatedSubmissionsState>
      submissions_state;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_TYPES_H_
