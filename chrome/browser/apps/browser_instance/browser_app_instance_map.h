// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_MAP_H_
#define CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_MAP_H_

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"

namespace apps {

template <typename KeyT, typename ValueT>
using BrowserAppInstanceMap = std::map<KeyT, std::unique_ptr<ValueT>>;

template <typename KeyT, typename ValueT>
ValueT& AddInstance(BrowserAppInstanceMap<KeyT, ValueT>& instances,
                    const KeyT& key,
                    std::unique_ptr<ValueT> instance) {
  DCHECK(!base::Contains(instances, key));
  auto it = instances.insert(std::make_pair(key, std::move(instance)));
  return *it.first->second;
}

template <typename KeyT, typename ValueT>
std::unique_ptr<ValueT> PopInstanceIfExists(
    BrowserAppInstanceMap<KeyT, ValueT>& instances,
    const KeyT& key) {
  auto it = instances.find(key);
  if (it == instances.end()) {
    return nullptr;
  }
  auto instance = std::move(it->second);
  instances.erase(it);
  return instance;
}

template <typename KeyT, typename ValueT>
ValueT* GetInstance(const BrowserAppInstanceMap<KeyT, ValueT>& instances,
                    const KeyT& key) {
  auto it = instances.find(key);
  return (it == instances.end()) ? nullptr : it->second.get();
}

template <typename KeyT, typename ValueT, typename PredicateT>
void SelectInstances(std::set<const ValueT*>& result,
                     const BrowserAppInstanceMap<KeyT, ValueT>& instances,
                     PredicateT predicate) {
  for (const auto& pair : instances) {
    const ValueT& instance = *pair.second;
    if (predicate(instance)) {
      result.insert(&instance);
    }
  }
}

template <typename KeyT, typename ValueT, typename PredicateT>
const ValueT* FindInstanceIf(
    const BrowserAppInstanceMap<KeyT, ValueT>& instances,
    PredicateT predicate) {
  auto it = base::ranges::find_if(
      instances, predicate,
      [](const auto& pair) -> const ValueT& { return *pair.second; });
  if (it == instances.end()) {
    return nullptr;
  }
  return it->second.get();
}

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_MAP_H_
