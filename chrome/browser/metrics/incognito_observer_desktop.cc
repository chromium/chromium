// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/metrics/incognito_observer.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"

namespace {

class IncognitoObserverDesktop : public IncognitoObserver,
                                 public BrowserListObserver {
 public:
  explicit IncognitoObserverDesktop(
      const base::RepeatingClosure& update_closure)
      : update_closure_(update_closure) {
    BrowserList::AddObserver(this);
  }

  ~IncognitoObserverDesktop() override { BrowserList::RemoveObserver(this); }

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override { update_closure_.Run(); }
  void OnBrowserRemoved(Browser* browser) override { update_closure_.Run(); }

  const base::RepeatingClosure update_closure_;

  DISALLOW_COPY_AND_ASSIGN(IncognitoObserverDesktop);
};

}  // namespace

// static
std::unique_ptr<IncognitoObserver> IncognitoObserver::Create(
    const base::RepeatingClosure& update_closure) {
  return std::make_unique<IncognitoObserverDesktop>(update_closure);
}
