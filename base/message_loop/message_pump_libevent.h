// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_

#include "base/message_loop/message_pump_epoll.h"

namespace base {

// TODO(crbug.com/40057013): Rewrite all references to MessagePumpLibevent and
// delete this header.
using MessagePumpLibevent = MessagePumpEpoll;

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_
