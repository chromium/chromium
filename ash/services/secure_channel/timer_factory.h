// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_H_
#define ASH_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_H_

#include <memory>

namespace base {
class OneShotTimer;
}  // namespace base

namespace chromeos {

namespace secure_channel {

// Creates timers. This class is needed so that tests can inject test doubles
// for timers.
class TimerFactory {
 public:
  TimerFactory(const TimerFactory&) = delete;
  TimerFactory& operator=(const TimerFactory&) = delete;

  virtual ~TimerFactory() = default;
  virtual std::unique_ptr<base::OneShotTimer> CreateOneShotTimer() = 0;

 protected:
  TimerFactory() = default;
};

}  // namespace secure_channel

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash::secure_channel {
using ::chromeos::secure_channel::TimerFactory;
}

#endif  // ASH_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_H_
