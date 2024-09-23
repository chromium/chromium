// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/copy_only_int.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/no_destructor.h"

namespace base {

// static
int CopyOnlyInt::num_copies_ = 0;

CopyOnlyInt::~CopyOnlyInt() {
  int old_data = std::exchange(data_, 0);
  if (GetDestructionCallbackStorage()) {
    GetDestructionCallbackStorage().Run(old_data);
  }
}

RepeatingCallback<void(int)>& CopyOnlyInt::GetDestructionCallbackStorage() {
  static NoDestructor<RepeatingCallback<void(int)>> callback;
  return *callback;
}

}  // namespace base
