// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_POWER_CLIENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_POWER_CLIENT_H_

#include "base/observer_list.h"

class PowerClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void SuspendImminent() {}
    virtual void SuspendDone() {}
  };

  PowerClient();
  virtual ~PowerClient();

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual bool IsSuspended();

 protected:
  void SetSuspended(bool suspended);

 private:
  base::ObserverList<Observer> observers_;
  bool is_suspended_ = false;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_POWER_CLIENT_H_
