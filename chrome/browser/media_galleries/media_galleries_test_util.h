// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"

namespace base {
class FilePath;
}

namespace extensions {
class Extension;
}

class Profile;

scoped_refptr<extensions::Extension> AddMediaGalleriesApp(
    const std::string& name,
    const std::vector<std::string>& media_galleries_permissions,
    Profile* profile);

base::FilePath MakeMediaGalleriesTestingPath(const std::string& dir);

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_
