// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_MESSAGE_PUMP_FOR_IO_FACTORY_WIN_H_
#define BASE_TEST_SCOPED_MESSAGE_PUMP_FOR_IO_FACTORY_WIN_H_

namespace base::test {

// Selects the event backend for I/O threads created within this scope.
// Construction and destruction must not overlap I/O pump creation.
class ScopedMessagePumpForIOEventFactoryForTesting {
 public:
  ScopedMessagePumpForIOEventFactoryForTesting();
  ~ScopedMessagePumpForIOEventFactoryForTesting();

  ScopedMessagePumpForIOEventFactoryForTesting(
      const ScopedMessagePumpForIOEventFactoryForTesting&) = delete;
  ScopedMessagePumpForIOEventFactoryForTesting& operator=(
      const ScopedMessagePumpForIOEventFactoryForTesting&) = delete;

 private:
  bool previously_enabled_;
};

}  // namespace base::test

#endif  // BASE_TEST_SCOPED_MESSAGE_PUMP_FOR_IO_FACTORY_WIN_H_
