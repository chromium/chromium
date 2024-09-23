// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_UTIL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_UTIL_H_

namespace ntp_modules {

const size_t kCategoryBlockListCount = 18;
constexpr std::array<std::string_view, kCategoryBlockListCount>
    kCategoryBlockList{"/g/11b76fyj2r", "/m/09lkz",  "/m/012mj",  "/m/01rbb",
                       "/m/02px0wr",    "/m/028hh",  "/m/034qg",  "/m/034dj",
                       "/m/0jxxt",      "/m/015fwp", "/m/04shl0", "/m/01h6rj",
                       "/m/05qt0",      "/m/06gqm",  "/m/09l0j_", "/m/01pxgq",
                       "/m/0chbx",      "/m/02c66t"};

}  // namespace ntp_modules

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_UTIL_H_
