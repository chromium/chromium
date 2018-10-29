// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tablet_mode.h"

#include "base/callback.h"
#include "base/no_destructor.h"

namespace ash {

namespace {

TabletMode::TabletModeCallback* GetCallback() {
  static base::NoDestructor<TabletMode::TabletModeCallback> callback;
  return callback.get();
}

}  // namespace

// static
void TabletMode::SetCallback(TabletModeCallback callback) {
  DCHECK(GetCallback()->is_null() || callback.is_null());
  *GetCallback() = std::move(callback);
}

// static
bool TabletMode::IsEnabled() {
  return GetCallback()->Run();
}

}  // namespace ash
