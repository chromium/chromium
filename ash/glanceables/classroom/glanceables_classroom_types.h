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

// Lightweight student submission definition. Created from
// `google_apis::classroom::StudentSubmission`
// (google_apis/classroom/classroom_api_student_submissions_response_types.h).
// API definition:
// https://developers.google.com/classroom/reference/rest/v1/courses.courseWork.studentSubmissions.
struct ASH_EXPORT GlanceablesClassroomStudentSubmission {
 public:
  // State of the student submission. Simplified version of
  // `google_apis::classroom::StudentSubmission::State` by the following rules:
  // - `kNew`, `kCreated`, `kReclaimedByStudent`, `kReturned` **without**
  //   an `assigned_grade()` -> `kAssigned`;
  // - `kTurnedIn` -> `kTurnedIn`;
  // - `kReturned` **with** an `assigned_grade()` -> `kGraded`,
  // - all other unknown values -> `kOther`.
  enum class State {
    kAssigned,
    kTurnedIn,
    kGraded,
    kOther,
  };

  GlanceablesClassroomStudentSubmission(const std::string& id,
                                        const std::string& course_work_id,
                                        State state);
  GlanceablesClassroomStudentSubmission(
      const GlanceablesClassroomStudentSubmission&) = delete;
  GlanceablesClassroomStudentSubmission& operator=(
      const GlanceablesClassroomStudentSubmission&) = delete;
  ~GlanceablesClassroomStudentSubmission() = default;

  // Identifier for this student submission assigned by Classroom.
  const std::string id;

  // Identifier for the course work which this submission belongs to.
  const std::string course_work_id;

  // State of the student submission.
  const State state;
};

// Represents a single classroom assignment for students (contains data from
// `GlanceablesClassroomCourse` and `GlanceablesClassroomCourseWorkItem`).
struct ASH_EXPORT GlanceablesClassroomStudentAssignment {
 public:
  GlanceablesClassroomStudentAssignment(const std::string& course_title,
                                        const std::string& course_work_title,
                                        const GURL& link,
                                        const absl::optional<base::Time>& due);
  GlanceablesClassroomStudentAssignment(
      const GlanceablesClassroomStudentAssignment&) = delete;
  GlanceablesClassroomStudentAssignment& operator=(
      const GlanceablesClassroomStudentAssignment&) = delete;
  ~GlanceablesClassroomStudentAssignment() = default;

  // Intended for debugging.
  std::string ToString() const;

  // Title of the course this assignment belongs to.
  const std::string course_title;

  // Title of the course work item this assignment belongs to.
  const std::string course_work_title;

  // Absolute link for redirects to Classroom web UI.
  const GURL link;

  // Due date and time in UTC of this course work item.
  const absl::optional<base::Time> due;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_TYPES_H_
