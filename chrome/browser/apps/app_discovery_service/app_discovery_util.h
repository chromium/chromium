// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_

#include "base/callback.h"

namespace base {
class Value;
}  // namespace base

namespace apps {

enum class ResultType {
  kRecommendedArcApps,
};

// TODO(crbug.com/1243545): Define an App struct instead of returning
// base::Value.
//
// TODO(crbug.com/1243546): Define error management. The ResultCallback
// should have an error response to inform the consumer whether the request
// was successful.
using ResultCallback = base::OnceCallback<void(const base::Value& result)>;

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_
