// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_CLASSROOM_FAKE_GLANCEABLES_CLASSROOM_CLIENT_H_
#define ASH_GLANCEABLES_CLASSROOM_FAKE_GLANCEABLES_CLASSROOM_CLIENT_H_

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class FakeGlanceablesClassroomClient : public GlanceablesClassroomClient {
 public:
  explicit FakeGlanceablesClassroomClient(GlanceablesClassroomClient* client);
  FakeGlanceablesClassroomClient(const FakeGlanceablesClassroomClient&) =
      delete;
  FakeGlanceablesClassroomClient& operator=(
      const FakeGlanceablesClassroomClient&) = delete;
  ~FakeGlanceablesClassroomClient() override;

  // GlanceablesClassroomClient:
  void IsStudentRoleActive(IsRoleEnabledCallback callback) override;
  void GetCompletedStudentAssignments(GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithApproachingDueDate(
      GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithMissedDueDate(
      GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithoutDueDate(
      GetAssignmentsCallback callback) override;
  void IsTeacherRoleActive(IsRoleEnabledCallback callback) override;
  void GetTeacherAssignmentsWithApproachingDueDate(
      GetAssignmentsCallback callback) override;
  void GetTeacherAssignmentsRecentlyDue(
      GetAssignmentsCallback callback) override;
  void GetTeacherAssignmentsWithoutDueDate(
      GetAssignmentsCallback callback) override;
  void GetGradedTeacherAssignments(GetAssignmentsCallback callback) override;
  void OpenUrl(const GURL& url) const override;
  void OnGlanceablesBubbleClosed() override;

 private:
  const raw_ptr<GlanceablesClassroomClient, DanglingUntriaged | ExperimentalAsh>
      original_client_;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_FAKE_GLANCEABLES_CLASSROOM_CLIENT_H_
