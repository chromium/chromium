// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/graduation/graduation_manager.h"

#include <string>

#include "base/check_op.h"

namespace ash::graduation {

namespace {
GraduationManager* g_instance_ = nullptr;
}

// static
GraduationManager* GraduationManager::Get() {
  return g_instance_;
}

GraduationManager::GraduationManager() {
  CHECK_EQ(g_instance_, nullptr);
  g_instance_ = this;
}

GraduationManager::~GraduationManager() {
  CHECK_EQ(g_instance_, this);
  g_instance_ = nullptr;
}

}  // namespace ash::graduation
