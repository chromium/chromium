// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_INCOGNITO_OBSERVER_H_
#define CHROME_BROWSER_METRICS_INCOGNITO_OBSERVER_H_

#include "base/functional/callback.h"

// Encapsulates platform-specific functionality for observing events that may
// cause "is incognito active?" state to change. The class takes a closure that
// will be called when an event happens that could result in a state change.
// The incognito state should then be checked by the callback.
// TODO(asvitkine): Considering moving the check for incognito to this class
// too; see ChromeMetricsServicesManagerClient::IsIncognitoSessionActive().
class IncognitoObserver {
 public:
  static std::unique_ptr<IncognitoObserver> Create(
      const base::RepeatingClosure& update_closure);

  IncognitoObserver(const IncognitoObserver&) = delete;
  IncognitoObserver& operator=(const IncognitoObserver&) = delete;

  virtual ~IncognitoObserver();

 protected:
  IncognitoObserver();
};

#endif  // CHROME_BROWSER_METRICS_INCOGNITO_OBSERVER_H_
