// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_CLIENT_H_
#define ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_CLIENT_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

namespace ash {

struct GlanceablesClassroomStudentAssignment;

// Interface for the classroom browser client.
class ASH_EXPORT GlanceablesClassroomClient {
 public:
  using IsRoleEnabledCallback = base::OnceCallback<void(bool active)>;
  using GetStudentAssignmentsCallback = base::OnceCallback<void(
      std::vector<std::unique_ptr<GlanceablesClassroomStudentAssignment>>
          assignments)>;

  virtual ~GlanceablesClassroomClient() = default;

  // Returns `true` if current user is enrolled in at least one classroom course
  // as a student.
  virtual void IsStudentRoleActive(IsRoleEnabledCallback callback) = 0;

  // Return student assignments based on different due date/time and submission
  // state filters.
  virtual void GetCompletedStudentAssignments(
      GetStudentAssignmentsCallback callback) = 0;
  virtual void GetStudentAssignmentsWithApproachingDueDate(
      GetStudentAssignmentsCallback callback) = 0;
  virtual void GetStudentAssignmentsWithMissedDueDate(
      GetStudentAssignmentsCallback callback) = 0;
  virtual void GetStudentAssignmentsWithoutDueDate(
      GetStudentAssignmentsCallback callback) = 0;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_CLIENT_H_
