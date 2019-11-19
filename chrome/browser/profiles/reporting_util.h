// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_REPORTING_UTIL_H_
#define CHROME_BROWSER_PROFILES_REPORTING_UTIL_H_

class Profile;

namespace base {
class Value;
}  // namespace base

namespace reporting {

// Fetches additional information that is common to every event. Fetches and
// returns corresponding info to a Device, Browser and Profile protos defined in
// google3/google/internal/chrome/reporting/v1/chromereporting.proto.
base::Value GetContext(Profile* profile);

}  // namespace reporting

#endif  // CHROME_BROWSER_PROFILES_REPORTING_UTIL_H_
