// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "chrome/browser/chromeos/throttle_observer.h"
#include "chrome/browser/chromeos/throttle_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace arc {
class ArcBridgeService;

// This class holds a number of observers which watch for several conditions
// (window activation, mojom instance connection, etc) and adjusts the
// throttling state of the ARC container on a change in conditions.
class ArcInstanceThrottle : public KeyedService,
                            public chromeos::ThrottleService {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual void SetCpuRestriction(bool) = 0;
    virtual void RecordCpuRestrictionDisabledUMA(
        const std::string& observer_name,
        base::TimeDelta delta) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Returns singleton instance for the given BrowserContext, or nullptr if
  // the browser |context| is not allowed to use ARC.
  static ArcInstanceThrottle* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcInstanceThrottle* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcInstanceThrottle(content::BrowserContext* context,
                      ArcBridgeService* arc_bridge_service);
  ~ArcInstanceThrottle() override;

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
                                       base::TimeDelta delta) override;

  std::unique_ptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ArcInstanceThrottle);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_
