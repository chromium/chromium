// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/projector/projector_annotator_controller.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

ProjectorAnnotatorController* g_instance = nullptr;

}  // namespace

// static
ProjectorAnnotatorController* ProjectorAnnotatorController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

ProjectorAnnotatorController::ProjectorAnnotatorController() {
  DCHECK(!g_instance);
  g_instance = this;
}

ProjectorAnnotatorController::~ProjectorAnnotatorController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
