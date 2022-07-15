// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_METRICS_RECORDER_H_
#define ASH_COMPONENTS_LOGIN_AUTH_METRICS_RECORDER_H_

#include "ash/components/login/auth/public/auth_failure.h"
#include "ash/components/login/auth/public/user_context.h"

namespace ash {

// This class encapsulates metrics reporting. User actions and behaviors are
// reported in multiple stages of the login flow. This metrics reporter would
// centralize the tracking and reporting.
class COMPONENT_EXPORT(ASH_LOGIN_AUTH) MetricsRecorder {
 public:
  // Enum used for UMA. Do NOT reorder or remove entry. Don't forget to
  // update LoginFlowUserLoginType enum in enums.xml when adding new entries.
  enum UserLoginType {
    kOnlineNew = 0,
    kOnlineExisting = 1,
    kOffline = 2,
    kEphemeral = 3,
    kMaxValue
  };

  // Reports various metrics during the login flow.
  MetricsRecorder();
  MetricsRecorder(const MetricsRecorder&) = delete;
  MetricsRecorder& operator=(const MetricsRecorder&) = delete;
  MetricsRecorder(MetricsRecorder&&) = delete;
  MetricsRecorder& operator=(MetricsRecorder&&) = delete;
  ~MetricsRecorder();

  // Logs the auth failure action and reason.
  void OnAuthFailure(const AuthFailure::FailureReason& failure_reason);

  // Logs the login success action and reason.
  void OnLoginSuccess(const SuccessReason& reason);

  // Logs the guest login success action.
  void OnGuestLoignSuccess();

  // Set the total number of regular users on the lock screen.
  void OnUserCount(bool user_count);

  // Set the policy setting whether to show users on sign in or not.
  void OnShowUsersOnSignin(bool show_users_on_signin);

  // Set the policy setting if ephemeral login are enforced.
  void OnEnableEphemeralUsers(bool enable_ephemeral_users);

  // Set whether the last successful login is a new user or not.
  void OnIsUserNew(bool is_new_user);

  // Set whether the last successful login is offline or not.
  void OnIsLoginOffline(bool is_login_offline);

 private:
  // Determine the user login type if 3 information are available:
  // is_login_offline_, is_new_user_, enable_ephemeral_users_.
  void MaybeUpdateUserLoginType();

  // Report the user login type in association with policy and total user count
  // if 3 information are available: user_count_, show_users_on_signin_,
  // user_login_type_.
  void MaybeReportFlowMetrics();

  absl::optional<int> user_count_;
  absl::optional<bool> show_users_on_signin_;
  absl::optional<bool> enable_ephemeral_users_;
  absl::optional<bool> is_new_user_;
  absl::optional<bool> is_login_offline_;
  absl::optional<UserLoginType> user_login_type_;
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_METRICS_RECORDER_H_