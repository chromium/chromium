// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_CORE_FINDS_SERVICE_H_
#define CHROME_BROWSER_FINDS_CORE_FINDS_SERVICE_H_

#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"

namespace finds {

// Service to interact with the optimization guide to perform Finds related
// inference.
class FindsService : public KeyedService, public base::SupportsUserData {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnOptInCriteriaFulfilled() = 0;
  };

  FindsService();
  ~FindsService() override;

  FindsService(const FindsService&) = delete;
  FindsService& operator=(const FindsService&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace finds

#endif  // CHROME_BROWSER_FINDS_CORE_FINDS_SERVICE_H_
