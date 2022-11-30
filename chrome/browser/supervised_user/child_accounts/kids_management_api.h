// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_KIDS_MANAGEMENT_API_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_KIDS_MANAGEMENT_API_H_

#include <string>

class GURL;

namespace kids_management_api {

GURL GetBaseURL();

GURL GetURL(const std::string& path);

}  // namespace kids_management_api

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_KIDS_MANAGEMENT_API_H_
