// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_SETTINGS_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_SETTINGS_H_

#include <string>

class Profile;

namespace base {
class Value;
}

class DevToolsSettings {
 public:
  explicit DevToolsSettings(Profile* profile);

  const base::Value* Get();
  void Set(const std::string& name, const std::string& value);
  void Remove(const std::string& name);
  void Clear();

 private:
  Profile* const profile_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_SETTINGS_H_
