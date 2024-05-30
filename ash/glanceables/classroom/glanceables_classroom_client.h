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

struct GlanceablesClassroomAssignment;

// Interface for the classroom browser client.
class ASH_EXPORT GlanceablesClassroomClient {
 public:
  using IsRoleEnabledCallback = base::OnceCallback<void(bool active)>;
  // `success` indicates whether the requested assignment data was successfully
  // refreshed. If `success` is false, `assignment` may be non-empty, but the
  // assignment information may be obsolete, incomplete.
  using GetAssignmentsCallback = base::OnceCallback<void(
      bool success,
      std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
          assignments)>;

  virtual ~GlanceablesClassroomClient() = default;

  // Verifies if the Classroom integration is disabled by admin by checking:
  // 1) if the integration is not listed in
  //    `prefs::kGoogleCalendarIntegrationName`,
  // 2) if the Classroom app is disabled by policy,
  // 3) if access to the Classroom web UI (URLs) is blocked by policy.
  virtual bool IsDisabledByAdmin() const = 0;

  // Returns `true` if current user is enrolled in at least one classroom course
  // as a student.
  virtual void IsStudentRoleActive(IsRoleEnabledCallback callback) = 0;

  // Return student assignments based on different due date/time and submission
  // state filters.
  virtual void GetCompletedStudentAssignments(
      GetAssignmentsCallback callback) = 0;
  virtual void GetStudentAssignmentsWithApproachingDueDate(
      GetAssignmentsCallback callback) = 0;
  virtual void GetStudentAssignmentsWithMissedDueDate(
      GetAssignmentsCallback callback) = 0;
  virtual void GetStudentAssignmentsWithoutDueDate(
      GetAssignmentsCallback callback) = 0;

  // Method called when the glanceables bubble UI closes. The client can use
  // this as a signal to invalidate cached classroom data.
  virtual void OnGlanceablesBubbleClosed() = 0;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_CLIENT_H_
