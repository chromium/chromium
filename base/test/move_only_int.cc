// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/move_only_int.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/no_destructor.h"

namespace base {

MoveOnlyInt::~MoveOnlyInt() {
  int old_data = std::exchange(data_, 0);
  if (GetDestructionCallbackStorage()) {
    GetDestructionCallbackStorage().Run(old_data);
  }
}

RepeatingCallback<void(int)>& MoveOnlyInt::GetDestructionCallbackStorage() {
  static NoDestructor<RepeatingCallback<void(int)>> callback;
  return *callback;
}

}  // namespace base
