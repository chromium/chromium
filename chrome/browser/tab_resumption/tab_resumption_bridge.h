// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_RESUMPTION_TAB_RESUMPTION_BRIDGE_H_
#define CHROME_BROWSER_TAB_RESUMPTION_TAB_RESUMPTION_BRIDGE_H_

#include <jni.h>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"

// Provides the fetch and rank services of the Tab resumption backend to
// Java.
class TabResumptionBridge {
 public:
  explicit TabResumptionBridge(Profile* profile);

  TabResumptionBridge(const TabResumptionBridge&) = delete;
  TabResumptionBridge& operator=(const TabResumptionBridge&) = delete;

  ~TabResumptionBridge();

  void Destroy(JNIEnv* env);

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_TAB_RESUMPTION_TAB_RESUMPTION_BRIDGE_H_
