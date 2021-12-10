// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_METRICS_UTIL_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_METRICS_UTIL_H_

namespace ash {

// Histogram names
constexpr char kLoadTemplateGridHistogramName[] =
    "Ash.DeskTemplate.LoadTemplateGrid";
constexpr char kDeleteTemplateHistogramName[] =
    "Ash.DeskTemplate.DeleteTemplate";
constexpr char kNewTemplateHistogramName[] = "Ash.DeskTemplate.NewTemplate";
constexpr char kLaunchTemplateHistogramName[] =
    "Ash.DeskTemplate.LaunchFromTemplate";

// Wrappers calls base::uma with correct histogram name.
void RecordLoadTemplateHistogram();
void RecordDeleteTemplateHistogram();
void RecordLaunchTemplateHistogram();
void RecordNewTemplateHistogram();

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_METRICS_UTIL_H_