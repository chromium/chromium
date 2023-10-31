// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/oobe_dialog_util.h"

#include "base/check_deref.h"
#include "base/check_op.h"

namespace ash {

namespace {

OobeDialogUtil* g_instance = nullptr;

}  // namespace

// static
OobeDialogUtil& OobeDialogUtil::Get() {
  return CHECK_DEREF(g_instance);
}

OobeDialogUtil::OobeDialogUtil() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

OobeDialogUtil::~OobeDialogUtil() {
  g_instance = nullptr;
}

}  // namespace ash
