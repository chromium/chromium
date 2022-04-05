// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_STRING_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_STRING_UTIL_H_

#include <string>

#include "base/values.h"

class Profile;

base::Value::Dict GetFileManagerStrings();

void AddFileManagerFeatureStrings(const std::string& locale,
                                  Profile* profile,
                                  base::Value::Dict* dict);

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_STRING_UTIL_H_
