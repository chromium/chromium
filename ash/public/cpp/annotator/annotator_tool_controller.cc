// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/annotator/annotator_tool_controller.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

AnnotatorToolController* g_instance = nullptr;

}  // namespace

AnnotatorToolController::AnnotatorToolController() {
  DCHECK(!g_instance);
  g_instance = this;
}

AnnotatorToolController::~AnnotatorToolController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AnnotatorToolController* AnnotatorToolController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

}  // namespace ash
