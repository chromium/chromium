// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_SCOPED_MACH_MSG_DESTROY_H_
#define BASE_MAC_SCOPED_MACH_MSG_DESTROY_H_

#include <mach/message.h>

#include "base/macros.h"

namespace base {

// Calls mach_msg_destroy on the specified message when the object goes out
// of scope.
class ScopedMachMsgDestroy {
 public:
  explicit ScopedMachMsgDestroy(mach_msg_header_t* header) : header_(header) {}

  ~ScopedMachMsgDestroy() {
    if (header_) {
      mach_msg_destroy(header_);
    }
  }

  // Prevents the message from being destroyed when it goes out of scope.
  void Disarm() { header_ = nullptr; }

 private:
  mach_msg_header_t* header_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMachMsgDestroy);
};

}  // namespace base

#endif  // BASE_MAC_SCOPED_MACH_MSG_DESTROY_H_
