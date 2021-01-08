// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_PARAMS_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_PARAMS_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "components/webapps/installable/installable_metrics.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace webapps {

struct ShortcutInfo;

struct AddToHomescreenParams {
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: (
  //  org.chromium.chrome.browser.webapps.addtohomescreen)
  enum class AppType {
    NATIVE,
    WEBAPK,
    SHORTCUT,
  };

  AppType app_type;
  SkBitmap primary_icon;
  bool has_maskable_primary_icon = false;
  std::unique_ptr<ShortcutInfo> shortcut_info;
  WebappInstallSource install_source;
  std::string native_app_package_name;
  base::android::ScopedJavaGlobalRef<jobject> native_app_data;

  AddToHomescreenParams();
  ~AddToHomescreenParams();
};

}  // namespace webapps

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_PARAMS_H_
