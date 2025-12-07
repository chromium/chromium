// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_THROTTLE_CROSTINI_THROTTLE_H_
#define CHROME_BROWSER_ASH_CROSTINI_THROTTLE_CROSTINI_THROTTLE_H_

#include <memory>
#include <string>
#include <utility>

#include "chromeos/ash/components/throttle/throttle_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace crostini {

// This class holds a number observers which watch for conditions and adjust the
// throttle state of the Crostini VM on a change in conditions.
class CrostiniThrottle : public KeyedService, public ash::ThrottleService {
 public:
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual void SetCpuRestriction(bool) = 0;
  };

  explicit CrostiniThrottle(content::BrowserContext* context);

  CrostiniThrottle(const CrostiniThrottle&) = delete;
  CrostiniThrottle& operator=(const CrostiniThrottle&) = delete;

  ~CrostiniThrottle() override;

  // KeyedService:
  void Shutdown() override;

  void set_delegate_for_testing(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

 private:
  // ash::ThrottleService:
  void ThrottleInstance(bool should_throttle) override;

  std::unique_ptr<Delegate> delegate_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_THROTTLE_CROSTINI_THROTTLE_H_
