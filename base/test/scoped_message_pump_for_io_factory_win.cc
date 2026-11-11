// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_message_pump_for_io_factory_win.h"

#include "base/message_loop/message_pump_win.h"

namespace base::test {

ScopedMessagePumpForIOEventFactoryForTesting::
    ScopedMessagePumpForIOEventFactoryForTesting()
    : previously_enabled_(MessagePumpForIO::SetEventBasedPumpForTesting(true)) {
}

ScopedMessagePumpForIOEventFactoryForTesting::
    ~ScopedMessagePumpForIOEventFactoryForTesting() {
  MessagePumpForIO::SetEventBasedPumpForTesting(previously_enabled_);
}

}  // namespace base::test
