// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TABLET_MODE_H_
#define ASH_PUBLIC_CPP_TABLET_MODE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/run_loop.h"

namespace ash {

// An interface implemented by Ash that allows Chrome to be informed of changes
// to tablet mode state.
class ASH_PUBLIC_EXPORT TabletMode {
 public:
  // Helper class to wait until the tablet mode transition is complete.
  class Waiter : public TabletModeObserver {
   public:
    explicit Waiter(bool enable);
    ~Waiter() override;

    void Wait();

    // TabletModeObserver:
    void OnTabletModeStarted() override;
    void OnTabletModeEnded() override;

   private:
    bool enable_;
    base::RunLoop run_loop_;

    DISALLOW_COPY_AND_ASSIGN(Waiter);
  };

  // Returns the singleton instance.
  static TabletMode* Get();

  virtual void AddObserver(TabletModeObserver* observer) = 0;
  virtual void RemoveObserver(TabletModeObserver* observer) = 0;

  // Returns true if the system is in tablet mode.
  virtual bool InTabletMode() const = 0;

  virtual void SetEnabledForTest(bool enabled) = 0;

 protected:
  TabletMode();
  virtual ~TabletMode();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TABLET_MODE_H_
