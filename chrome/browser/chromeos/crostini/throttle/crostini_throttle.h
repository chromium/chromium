// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_THROTTLE_CROSTINI_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_THROTTLE_CROSTINI_THROTTLE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "chrome/browser/chromeos/throttle_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace crostini {

// This class holds a number observers which watch for conditions and adjust the
// throttle state of the Crostini VM on a change in conditions.
class CrostiniThrottle : public KeyedService, public chromeos::ThrottleService {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual void SetCpuRestriction(bool) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Returns singleton instance for the given BrowserContext, or nullptr if
  // the browser |context| is not allowed to use Crostini.
  static CrostiniThrottle* GetForBrowserContext(
      content::BrowserContext* context);

  explicit CrostiniThrottle(content::BrowserContext* context);
  ~CrostiniThrottle() override;

  // KeyedService:
  void Shutdown() override;

  void set_delegate_for_testing(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

 private:
  // chromeos::ThrottleService:
  void ThrottleInstance(
      chromeos::ThrottleObserver::PriorityLevel level) override;
  void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                       base::TimeDelta delta) override {}

  std::unique_ptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniThrottle);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_THROTTLE_CROSTINI_THROTTLE_H_
