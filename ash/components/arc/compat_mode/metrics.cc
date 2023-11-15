// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/metrics.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"

namespace arc {

namespace {

const char* GetActionName(ResizeLockActionType type) {
  switch (type) {
    case ResizeLockActionType::ResizeToPhone:
      return "ArcResizeLock_ResizeToPhone";
    case ResizeLockActionType::ResizeToTablet:
      return "ArcResizeLock_ResizeToTablet";
    case ResizeLockActionType::TurnOnResizeLock:
      return "ArcResizeLock_TurnOnResizeLock";
    case ResizeLockActionType::TurnOffResizeLock:
      return "ArcResizeLock_TurnOffResizeLock";
  }
}

const char* GetStateHistogramName(ResizeLockStateHistogramType type) {
  switch (type) {
    case ResizeLockStateHistogramType::InitialState:
      return "Arc.CompatMode.InitialResizeLockState";
  }
}

}  // namespace

void RecordResizeLockAction(ResizeLockActionType type) {
  base::RecordAction(base::UserMetricsAction(GetActionName(type)));
}

void RecordResizeLockStateHistogram(ResizeLockStateHistogramType type,
                                    mojom::ArcResizeLockState state) {
  base::UmaHistogramEnumeration(GetStateHistogramName(type), state);
}

const char* GetResizeLockActionNameForTesting(  // IN-TEST
    ResizeLockActionType type) {
  return GetActionName(type);
}

const char* GetResizeLockStateHistogramNameForTesting(  // IN-TEST
    ResizeLockStateHistogramType type) {
  return GetStateHistogramName(type);
}

}  // namespace arc
