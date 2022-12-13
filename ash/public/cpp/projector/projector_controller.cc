// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/projector/projector_controller.h"

#include "ash/public/cpp/projector/speech_recognition_availability.h"
#include "base/check_op.h"
#include "base/command_line.h"

namespace ash {

namespace {
ProjectorController* g_instance = nullptr;

// Controls whether to enable the extended features of Projector. These include
// speech recognition and drivefs integration. Only used during development to
// disable extended features in X11 simulator.
constexpr char kExtendedProjectorFeaturesDisabled[] =
    "projector-extended-features-disabled";
}  // namespace

ProjectorController::ProjectorController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

ProjectorController::~ProjectorController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ProjectorController* ProjectorController::Get() {
  return g_instance;
}

// static
bool ProjectorController::AreExtendedProjectorFeaturesDisabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kExtendedProjectorFeaturesDisabled);
}

}  // namespace ash
