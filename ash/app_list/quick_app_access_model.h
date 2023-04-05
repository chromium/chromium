// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_QUICK_APP_ACCESS_MODEL_H_
#define ASH_APP_LIST_QUICK_APP_ACCESS_MODEL_H_

#include <string>

#include "ash/app_list/model/app_list_item_observer.h"
#include "base/observer_list.h"

namespace ash {

// The model which holds information on which app is currently set as the quick
// app. Shelf home buttons observe changes to this model and will show/hide the
// quick app button accordingly.
class QuickAppAccessModel {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the currently set quick app changes.
    virtual void OnQuickAppChanged() = 0;
  };
  QuickAppAccessModel();

  QuickAppAccessModel(const QuickAppAccessModel&) = delete;
  QuickAppAccessModel& operator=(const QuickAppAccessModel&) = delete;

  ~QuickAppAccessModel();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Set the quick app to be shown next to home buttons in the shelf.
  void SetQuickApp(const std::string& app_id);

  const std::string& quick_app_id() { return quick_app_id_; }

 private:
  base::ObserverList<Observer> observers_;

  // The app id for the quick app.
  std::string quick_app_id_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_QUICK_APP_ACCESS_MODEL_H_
