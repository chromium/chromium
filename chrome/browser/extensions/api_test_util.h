// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_TEST_UTIL_H_

#include <string>

class Profile;

namespace extensions {
class Extension;

namespace api_test_util {

// Tests that exactly one extension loaded. If so, returns a pointer to the
// extension. If not, returns nullptr and sets `message`.
const Extension* GetSingleLoadedExtension(Profile* profile,
                                          std::string& message);

}  // namespace api_test_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TEST_UTIL_H_
