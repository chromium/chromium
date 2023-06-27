// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern "C" {
pid_t responsibility_get_pid_responsible_for_pid(pid_t);
}

namespace base {

bool IsProcessSelfResponsible() {
  const pid_t pid = getpid();
  return responsibility_get_pid_responsible_for_pid(pid) == pid;
}

}  // namespace base
