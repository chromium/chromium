// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "chrome/browser/metrics/incognito_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"

namespace {

class IncognitoObserverAndroid : public IncognitoObserver,
                                 public TabModelListObserver {
 public:
  explicit IncognitoObserverAndroid(
      const base::RepeatingClosure& update_closure)
      : update_closure_(update_closure) {
    TabModelList::AddObserver(this);
  }

  IncognitoObserverAndroid(const IncognitoObserverAndroid&) = delete;
  IncognitoObserverAndroid& operator=(const IncognitoObserverAndroid&) = delete;

  ~IncognitoObserverAndroid() override { TabModelList::RemoveObserver(this); }

  // TabModelListObserver:
  void OnTabModelAdded() override { update_closure_.Run(); }
  void OnTabModelRemoved() override { update_closure_.Run(); }

 private:
  const base::RepeatingClosure update_closure_;
};

}  // namespace

// static
std::unique_ptr<IncognitoObserver> IncognitoObserver::Create(
    const base::RepeatingClosure& update_closure) {
  return std::make_unique<IncognitoObserverAndroid>(update_closure);
}
