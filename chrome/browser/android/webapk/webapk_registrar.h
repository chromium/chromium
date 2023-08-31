// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_REGISTRAR_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_REGISTRAR_H_

#include <map>

#include "chrome/browser/android/webapk/proto/webapk_database.pb.h"

namespace webapk {

using Registry = std::map<std::string, std::unique_ptr<WebApkProto>>;

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_REGISTRAR_H_
