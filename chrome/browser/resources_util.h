// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_UTIL_H_
#define CHROME_BROWSER_RESOURCES_UTIL_H_

#include <string>

class ResourcesUtil {
 public:
  ResourcesUtil(const ResourcesUtil&) = delete;
  ResourcesUtil& operator=(const ResourcesUtil&) = delete;

  // Returns the theme resource id or -1 if no resource with the name exists.
  static int GetThemeResourceId(const std::string& resource_name);

 private:
  ResourcesUtil() {}
};

#endif  // CHROME_BROWSER_RESOURCES_UTIL_H_
