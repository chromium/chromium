// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_CLASSROOM_FAKE_GLANCEABLES_CLASSROOM_CLIENT_H_
#define ASH_GLANCEABLES_CLASSROOM_FAKE_GLANCEABLES_CLASSROOM_CLIENT_H_

#include "ash/glanceables/classroom/glanceables_classroom_client.h"

namespace ash {

class FakeGlanceablesClassroomClient : public GlanceablesClassroomClient {
 public:
  FakeGlanceablesClassroomClient();
  FakeGlanceablesClassroomClient(const FakeGlanceablesClassroomClient&) =
      delete;
  FakeGlanceablesClassroomClient& operator=(
      const FakeGlanceablesClassroomClient&) = delete;
  ~FakeGlanceablesClassroomClient() override;

  // GlanceablesClassroomClient:
  bool IsDisabledByAdmin() const override;
  void IsStudentRoleActive(IsRoleEnabledCallback callback) override;
  void GetCompletedStudentAssignments(GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithApproachingDueDate(
      GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithMissedDueDate(
      GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithoutDueDate(
      GetAssignmentsCallback callback) override;
  void OnGlanceablesBubbleClosed() override;

  void set_is_disabled_by_admin(bool is_disabled_by_admin) {
    is_disabled_by_admin_ = is_disabled_by_admin;
  }

 private:
  bool is_disabled_by_admin_ = false;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_FAKE_GLANCEABLES_CLASSROOM_CLIENT_H_
