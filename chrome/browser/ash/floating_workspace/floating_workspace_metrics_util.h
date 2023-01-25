// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_METRICS_UTIL_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_METRICS_UTIL_H_

namespace ash::floating_workspace_metrics_util {

enum RestoredBrowserSessionType {
  // Unknown browser session.
  kUnknown = 0,
  // Local session restored.
  kLocal,
  // Remote Session restored.
  kRemote,
  kMaxValue = kRemote,
};

constexpr char kFloatingWorkspaceV1Initialized[] =
    "Ash.FloatingWorkspace.FloatingWorkspaceV1Initialized";
constexpr char kFloatingWorkspaceV1RestoredSessionType[] =
    "Ash.FloatingWorkspace.FloatingWorkspaceV1RestoredSessionType";

void RecordFloatingWorkspaceV1InitializedHistogram();
void RecordFloatingWorkspaceV1RestoredSessionType(
    RestoredBrowserSessionType type);

}  // namespace ash::floating_workspace_metrics_util

#endif  // CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_METRICS_UTIL_H_
