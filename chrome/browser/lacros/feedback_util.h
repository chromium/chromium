// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_FEEDBACK_UTIL_H_
#define CHROME_BROWSER_LACROS_FEEDBACK_UTIL_H_

#include <string>

namespace feedback_util {

// Returns the email of the signed-in user.
std::string GetSignedInUserEmail();

}  // namespace feedback_util

#endif  // CHROME_BROWSER_LACROS_FEEDBACK_UTIL_H_
