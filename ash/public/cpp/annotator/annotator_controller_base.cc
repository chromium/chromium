// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/annotator/annotator_controller_base.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

AnnotatorControllerBase* g_instance = nullptr;

}  // namespace

AnnotatorControllerBase::AnnotatorControllerBase() {
  DCHECK(!g_instance);
  g_instance = this;
}

AnnotatorControllerBase::~AnnotatorControllerBase() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AnnotatorControllerBase* AnnotatorControllerBase::Get() {
  DCHECK(g_instance);
  return g_instance;
}

}  // namespace ash
