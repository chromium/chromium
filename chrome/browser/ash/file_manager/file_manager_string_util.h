// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_STRING_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_STRING_UTIL_H_

#include <string>

#include "base/values.h"

class Profile;

base::Value::Dict GetFileManagerStrings();

base::Value::Dict GetFileManagerPluralStrings();

// This function will return a number between 0 (Sunday) and 6 (Saturday)
// to indicate which day is the start of week based on the current locale.
int GetLocaleBasedWeekStart();

void AddFileManagerFeatureStrings(const std::string& locale,
                                  Profile* profile,
                                  base::Value::Dict* dict);

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_STRING_UTIL_H_
