// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/fake_glanceables_classroom_client.h"

#include <string>

#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace ash {
namespace {

std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
CreateAssignmentsWithStringForStudents(
    const std::string& course_work_name_prefix,
    int count) {
  std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments;
  for (int i = 0; i < count; ++i) {
    assignments.push_back(std::make_unique<GlanceablesClassroomAssignment>(
        base::StringPrintf("Course %d", i),
        base::StringPrintf("%s Course Work %d", course_work_name_prefix.c_str(),
                           i),
        GURL(base::StringPrintf(
            "https://classroom.google.com/c/test/a/test_course_id_%d/details",
            i)),
        std::nullopt, base::Time(), std::nullopt));
  }
  return assignments;
}

}  // namespace

FakeGlanceablesClassroomClient::FakeGlanceablesClassroomClient() = default;

FakeGlanceablesClassroomClient::~FakeGlanceablesClassroomClient() = default;

void FakeGlanceablesClassroomClient::SetAssignmentsCount(size_t count) {
  assignments_count_ = count;
}

bool FakeGlanceablesClassroomClient::IsDisabledByAdmin() const {
  return is_disabled_by_admin_;
}

void FakeGlanceablesClassroomClient::IsStudentRoleActive(
    IsRoleEnabledCallback callback) {
  std::move(callback).Run(true);
}

void FakeGlanceablesClassroomClient::GetCompletedStudentAssignments(
    GetAssignmentsCallback callback) {
  std::move(callback).Run(true, CreateAssignmentsWithStringForStudents(
                                    "Completed", assignments_count_));
}

void FakeGlanceablesClassroomClient::
    GetStudentAssignmentsWithApproachingDueDate(
        GetAssignmentsCallback callback) {
  std::move(callback).Run(true, CreateAssignmentsWithStringForStudents(
                                    "Approaching", assignments_count_));
}

void FakeGlanceablesClassroomClient::GetStudentAssignmentsWithMissedDueDate(
    GetAssignmentsCallback callback) {
  std::move(callback).Run(true, CreateAssignmentsWithStringForStudents(
                                    "Missing", assignments_count_));
}

void FakeGlanceablesClassroomClient::GetStudentAssignmentsWithoutDueDate(
    GetAssignmentsCallback callback) {
  std::move(callback).Run(true, CreateAssignmentsWithStringForStudents(
                                    "No Due Date", assignments_count_));
}

void FakeGlanceablesClassroomClient::OnGlanceablesBubbleClosed() {}

}  // namespace ash
