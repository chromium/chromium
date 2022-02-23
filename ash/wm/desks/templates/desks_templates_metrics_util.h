// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_METRICS_UTIL_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_METRICS_UTIL_H_

#include "ash/public/cpp/desk_template.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {

// Histogram names.
constexpr char kLoadTemplateGridHistogramName[] =
    "Ash.DeskTemplate.LoadTemplateGrid";
constexpr char kDeleteTemplateHistogramName[] =
    "Ash.DeskTemplate.DeleteTemplate";
constexpr char kNewTemplateHistogramName[] = "Ash.DeskTemplate.NewTemplate";
constexpr char kLaunchTemplateHistogramName[] =
    "Ash.DeskTemplate.LaunchFromTemplate";
constexpr char kAddOrUpdateTemplateStatusHistogramName[] =
    "Ash.DeskTemplate.AddOrUpdateTemplateStatus";
constexpr char kWindowCountHistogramName[] = "Ash.DeskTemplate.WindowCount";
constexpr char kTabCountHistogramName[] = "Ash.DeskTemplate.TabCount";
constexpr char kWindowAndTabCountHistogramName[] =
    "Ash.DeskTemplate.WindowAndTabCount";
constexpr char kLaunchFromTemplateHistogramName[] =
    "Ash.DeskTemplate.LaunchFromTemplate";
constexpr char kUserTemplateCountHistogramName[] =
    "Ash.DeskTemplate.UserTemplateCount";
constexpr char kUnsupportedAppDialogShowHistogramName[] =
    "Ash.DeskTemplate.UnsupportedAppDialogShow";
constexpr char kReplaceTemplateHistogramName[] =
    "Ash.DeskTemplate.ReplaceTemplate";

// Wrappers calls base::uma with correct histogram name.
void RecordLoadTemplateHistogram();
void RecordDeleteTemplateHistogram();
void RecordLaunchTemplateHistogram();
void RecordNewTemplateHistogram();
void RecordAddOrUpdateTemplateStatusHistogram(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status);
void RecordUserTemplateCountHistogram(size_t entry_count,
                                      size_t max_entry_count);
void RecordWindowAndTabCountHistogram(DeskTemplate* desk_template);
void RecordUnsupportedAppDialogShowHistogram();
void RecordReplaceTemplateHistogram();

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_METRICS_UTIL_H_
