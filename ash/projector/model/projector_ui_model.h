// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_MODEL_PROJECTOR_UI_MODEL_H_
#define ASH_PROJECTOR_MODEL_PROJECTOR_UI_MODEL_H_

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {

// A checked observer which receives notification of changes to the Projector UI
// model.
class ASH_EXPORT ProjectorUiModelObserver : public base::CheckedObserver {
 public:
  ProjectorUiModelObserver() = default;
  ProjectorUiModelObserver(const ProjectorUiModelObserver&) = delete;
  ProjectorUiModelObserver& operator=(const ProjectorUiModelObserver&) = delete;
  ~ProjectorUiModelObserver() override = default;

  // Invoked when the bar state is changed.
  virtual void OnProjectorBarStateChanged(bool enabled) {}
};

// Models the Projector UI.
class ASH_EXPORT ProjectorUiModel {
 public:
  ProjectorUiModel();
  ProjectorUiModel(const ProjectorUiModel&) = delete;
  ProjectorUiModel& operator=(const ProjectorUiModel&) = delete;
  ~ProjectorUiModel();

  // Adds/removes the specified |observer|.
  void AddObserver(ProjectorUiModelObserver* observer);
  void RemoveObserver(ProjectorUiModelObserver* observer);

  void SetBarEnabled(bool enabled);

  bool bar_enabled() const { return enabled_; }

 private:
  void NotifyBarStateChanged(bool enabled);

  // Whether the projector bar is enabled.
  bool enabled_ = false;

  base::ObserverList<ProjectorUiModelObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_MODEL_PROJECTOR_UI_MODEL_H_
