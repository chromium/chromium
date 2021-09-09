// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_SET_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_SET_H_

#include <iterator>
#include <map>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "chrome/browser/apps/app_service/browser_app_instance.h"
#include "chrome/browser/apps/app_service/browser_app_instance_observer.h"

namespace aura {
class Window;
}

namespace apps {

class BrowserAppInstanceObserver;

// A container for |BrowserAppInstance| objects. Manages adding, updating,
// removing app instances, and notifying registered observers when these events
// happen. Supports lookup of instances by key, and iteration over the list of
// all instances.
template <class KeyT>
class BrowserAppInstanceSet {
 public:
  void AddInstance(const KeyT& key,
                   std::unique_ptr<BrowserAppInstance> instance) {
    DCHECK(!base::Contains(instances_, key));
    auto it = instances_.insert(std::make_pair(key, std::move(instance)));
    auto& inserted_instance = it.first->second;
    for (auto& observer : observers_) {
      observer.OnBrowserAppAdded(*inserted_instance);
    }
  }

  // Updates the instance with the new attributes and notifies observers, if it
  // was changed. Returns true is the instance was changed.
  bool MaybeUpdateInstance(BrowserAppInstance& instance,
                           aura::Window* window,
                           const absl::optional<std::string>& title,
                           bool is_browser_visible,
                           bool is_browser_active,
                           const absl::optional<bool>& is_web_contents_active) {
    if (instance.window == window && instance.title == title &&
        instance.is_browser_visible == is_browser_visible &&
        instance.is_browser_active == is_browser_active &&
        instance.is_web_contents_active == is_web_contents_active) {
      return false;
    }
    instance.window = window;
    instance.title = title;
    instance.is_browser_visible = is_browser_visible;
    instance.is_browser_active = is_browser_active;
    instance.is_web_contents_active = is_web_contents_active;
    for (auto& observer : observers_) {
      observer.OnBrowserAppUpdated(instance);
    }
    return true;
  }

  // Removes the instance, if it exists, and notifies observers.
  std::unique_ptr<BrowserAppInstance> PopInstanceIfExists(const KeyT& key) {
    auto it = instances_.find(key);
    if (it == instances_.end()) {
      return nullptr;
    }
    auto instance = std::move(it->second);
    instances_.erase(it);
    for (auto& observer : observers_) {
      observer.OnBrowserAppRemoved(*instance);
    }
    return instance;
  }

  void AddObserver(BrowserAppInstanceObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(BrowserAppInstanceObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  BrowserAppInstance* GetInstance(const KeyT& key) {
    const auto* const_this = this;
    return const_cast<BrowserAppInstance*>(const_this->GetInstance(key));
  }

  const BrowserAppInstance* GetInstance(const KeyT& key) const {
    auto it = instances_.find(key);
    return (it == instances_.end()) ? nullptr : it->second.get();
  }

  using MapType = std::map<KeyT, std::unique_ptr<BrowserAppInstance>>;

  typename MapType::const_iterator begin() const {
    return std::begin(instances_);
  }

  typename MapType::const_iterator end() const { return std::end(instances_); }

 private:
  MapType instances_;
  base::ObserverList<BrowserAppInstanceObserver, true>::Unchecked observers_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_SET_H_
