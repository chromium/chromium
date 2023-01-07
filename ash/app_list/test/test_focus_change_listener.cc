// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/test_focus_change_listener.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/apps_grid_view.h"

namespace ash {

namespace {
static TestFocusChangeListener* g_instance = nullptr;
}

TestFocusChangeListener::TestFocusChangeListener(
    views::FocusManager* focus_manager)
    : focus_manager_(focus_manager) {
  DCHECK(focus_manager_);
  focus_manager_->AddFocusChangeListener(this);

  DCHECK(!g_instance);
  g_instance = this;
}

TestFocusChangeListener::~TestFocusChangeListener() {
  focus_manager_->RemoveFocusChangeListener(this);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// views::FocusChangeListener:
void TestFocusChangeListener::OnDidChangeFocus(views::View* focused_before,
                                               views::View* focused_now) {
  ++focus_change_count_;
}

}  // namespace ash
