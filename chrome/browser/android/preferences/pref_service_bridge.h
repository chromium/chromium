// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_PREF_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_PREF_SERVICE_BRIDGE_H_

class PrefServiceBridge {
 public:
  static const char* GetPrefNameExposedToJava(int pref_index);
};

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_PREF_SERVICE_BRIDGE_H_
