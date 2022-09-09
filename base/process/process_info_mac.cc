// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_info.h"

#include <os/availability.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern "C" {
pid_t responsibility_get_pid_responsible_for_pid(pid_t)
    API_AVAILABLE(macosx(10.12));
}

namespace base {

bool IsProcessSelfResponsible() {
  if (__builtin_available(macOS 10.14, *)) {
    const pid_t pid = getpid();
    return responsibility_get_pid_responsible_for_pid(pid) == pid;
  }
  return true;
}

}  // namespace base
