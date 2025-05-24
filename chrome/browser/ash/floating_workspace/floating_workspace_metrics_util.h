// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_METRICS_UTIL_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_METRICS_UTIL_H_

#include "base/time/time.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash::floating_workspace_metrics_util {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(StartupUiClosureReason)
enum class StartupUiClosureReason {
  // User manually pressed the button to close the UI without waiting for
  // sessions to restore.
  kManual = 0,
  // UI closed automatically once the session was restored.
  kAutomatic,
  kMaxValue = kAutomatic,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ash/enums.xml:FloatingWorkspaceStartupUiClosureReason)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LaunchTemplateFailureType)
enum class LaunchTemplateFailureType {
  // Unknown error.
  kUnknownError = 0,
  // Storage error.
  kStorageError,
  // The desk count requirement not met.
  kDesksCountCheckFailedError,
  kMaxValue = kDesksCountCheckFailedError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ash/enums.xml:FloatingWorkspaceV2LaunchTemplateFailureType)

inline constexpr char kFloatingWorkspaceStartupUiClosureReason[] =
    "Ash.FloatingWorkspace.StartupUiClosureReason";

inline constexpr char kFloatingWorkspaceV2TemplateLaunchFailureStatus[] =
    "Ash.FloatingWorkspace.TemplateLaunchFailureStatus";
inline constexpr char kFloatingWorkspaceV2TemplateLaunchTimedOut[] =
    "Ash.FloatingWorkspace.TemplateLaunchTimeOut";
inline constexpr char kFloatingWorkspaceV2TemplateLoadTime[] =
    "Ash.FloatingWorkspace.TemplateLoadTime";
inline constexpr char kFloatingWorkspaceV2TemplateSize[] =
    "Ash.FloatingWorkspace.TemplateSize";
inline constexpr char kFloatingWorkspaceV2TemplateUploadStatus[] =
    "Ash.FloatingWorkspace.TemplateUploadStatus";
inline constexpr char kFloatingWorkspaceV2Initialized[] =
    "Ash.FloatingWorkspace.FloatingWorkspaceV2Initialized";
inline constexpr char kFloatingWorkspaceV2TemplateNotFound[] =
    "Ash.FloatingWorkspace.TemplateNotFound";

void RecordFloatingWorkspaceStartupUiClosureReason(
    StartupUiClosureReason reason);

void RecordFloatingWorkspaceV2TemplateLaunchFailureType(
    LaunchTemplateFailureType type);
void RecordFloatingWorkspaceV2TemplateLoadTime(base::TimeDelta duration);
void RecordFloatingWorkspaceV2TemplateSize(size_t file_size);
void RecordFloatingWorkspaceV2TemplateUploadStatusHistogram(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status);
void RecordFloatingWorkspaceV2InitializedHistogram();
void RecordFloatingWorkspaceV2TemplateNotFound();

}  // namespace ash::floating_workspace_metrics_util

#endif  // CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_METRICS_UTIL_H_
