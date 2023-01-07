// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WINDOW_USER_DATA_H_
#define ASH_WINDOW_USER_DATA_H_

#include <map>
#include <memory>
#include <utility>

#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

// WindowUserData provides a way to associate an object with a Window and have
// that object destroyed when the window is destroyed, or when WindowUserData
// is destroyed (from aura::WindowObserver::OnWindowDestroying()).
//
// NOTE: WindowUserData does not make use of the Set/GetProperty API offered
// on aura::Window. This is done to avoid collisions in the case of multiple
// WindowUserDatas operating on the same Window.
template <typename UserData>
class WindowUserData : public aura::WindowObserver {
 public:
  WindowUserData() {}

  WindowUserData(const WindowUserData&) = delete;
  WindowUserData& operator=(const WindowUserData&) = delete;

  ~WindowUserData() override { clear(); }

  void clear() {
    // Take care to destroy the data after removing from the map.
    while (!window_to_data_.empty()) {
      auto iter = window_to_data_.begin();
      iter->first->RemoveObserver(this);
      std::unique_ptr<UserData> user_data = std::move(iter->second);
      window_to_data_.erase(iter);
    }
  }

  // Sets the data associated with window. This destroys any existing data.
  // |data| may be null.
  void Set(aura::Window* window, std::unique_ptr<UserData> data) {
    if (!data) {
      if (window_to_data_.erase(window))
        window->RemoveObserver(this);
      return;
    }
    if (window_to_data_.count(window) == 0u)
      window->AddObserver(this);
    window_to_data_[window] = std::move(data);
  }

  // Returns the data associated with the window, or null if none set. The
  // returned object is owned by WindowUserData.
  UserData* Get(aura::Window* window) {
    auto it = window_to_data_.find(window);
    return it == window_to_data_.end() ? nullptr : it->second.get();
  }

  // Returns the set of windows with data associated with them.
  std::set<aura::Window*> GetWindows() {
    std::set<aura::Window*> windows;
    for (auto& pair : window_to_data_)
      windows.insert(pair.first);
    return windows;
  }

 private:
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    window_to_data_.erase(window);
  }

  std::map<aura::Window*, std::unique_ptr<UserData>> window_to_data_;
};

}  // namespace ash

#endif  // ASH_WINDOW_USER_DATA_H_
