// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_METRICS_UTIL_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_METRICS_UTIL_H_

#include "base/time/time.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash::floating_workspace_metrics_util {

enum class RestoredBrowserSessionType {
  // Unknown browser session.
  kUnknown = 0,
  // Local session restored.
  kLocal,
  // Remote Session restored.
  kRemote,
  kMaxValue = kRemote,
};

enum class LaunchTemplateTimeoutType {
  // Unknown timeout reason.
  kUnknown = 0,
  // Passed wait period timeout.
  kPassedWaitPeriod,
  // No floating workspace template.
  kNoFloatingWorkspaceTemplate,
  kMaxValue = kNoFloatingWorkspaceTemplate,
};

enum class LaunchTemplateFailureType {
  // Unknown error.
  kUnknownError = 0,
  // Storage error.
  kStorageError,
  // The desk count requirement not met.
  kDesksCountCheckFailedError,
  kMaxValue = kDesksCountCheckFailedError,
};

constexpr char kFloatingWorkspaceV1Initialized[] =
    "Ash.FloatingWorkspace.FloatingWorkspaceV1Initialized";
constexpr char kFloatingWorkspaceV1RestoredSessionType[] =
    "Ash.FloatingWorkspace.FloatingWorkspaceV1RestoredSessionType";

constexpr char kFloatingWorkspaceV2TemplateLaunchFailureStatus[] =
    "Ash.FloatingWorkspace.TemplateLaunchFailureStatus";
constexpr char kFloatingWorkspaceV2TemplateLaunchTimedOut[] =
    "Ash.FloatingWorkspace.TemplateLaunchTimeOut";
constexpr char kFloatingWorkspaceV2TemplateLoadTime[] =
    "Ash.FloatingWorkspace.TemplateLoadTime";
constexpr char kFloatingWorkspaceV2TemplateSize[] =
    "Ash.FloatingWorkspace.TemplateSize";
constexpr char kFloatingWorkspaceV2TemplateUploadStatus[] =
    "Ash.FloatingWorkspace.TemplateUploadStatus";
constexpr char kFloatingWorkspaceV2Initialized[] =
    "Ash.FloatingWorkspace.FloatingWorkspaceV2Initialized";
constexpr char kFloatingWorkspaceV2TemplateNotFound[] =
    "Ash.FloatingWorkspace.TemplateNotFound";

void RecordFloatingWorkspaceV1InitializedHistogram();
void RecordFloatingWorkspaceV1RestoredSessionType(
    RestoredBrowserSessionType type);
void RecordFloatingWorkspaceV2TemplateLaunchFailureType(
    LaunchTemplateFailureType type);
void RecordFloatingWorkspaceV2TemplateLaunchTimeout(
    LaunchTemplateTimeoutType type);
void RecordFloatingWorkspaceV2TemplateLoadTime(base::TimeDelta duration);
void RecordFloatingWorkspaceV2TemplateSize(size_t file_size);
void RecordFloatingWorkspaceV2TemplateUploadStatusHistogram(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status);
void RecordFloatingWorkspaceV2InitializedHistogram();
void RecordFloatingWorkspaceV2TemplateNotFound();

}  // namespace ash::floating_workspace_metrics_util

#endif  // CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_METRICS_UTIL_H_
