// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "chrome/browser/metrics/incognito_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

namespace {

class IncognitoObserverDesktop : public IncognitoObserver,
                                 public BrowserCollectionObserver {
 public:
  explicit IncognitoObserverDesktop(
      const base::RepeatingClosure& update_closure)
      : update_closure_(update_closure) {
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
  }

  IncognitoObserverDesktop(const IncognitoObserverDesktop&) = delete;
  IncognitoObserverDesktop& operator=(const IncognitoObserverDesktop&) = delete;

  ~IncognitoObserverDesktop() override = default;

 private:
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    update_closure_.Run();
  }
  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    update_closure_.Run();
  }

  const base::RepeatingClosure update_closure_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

}  // namespace

// static
std::unique_ptr<IncognitoObserver> IncognitoObserver::Create(
    const base::RepeatingClosure& update_closure) {
  return std::make_unique<IncognitoObserverDesktop>(update_closure);
}
