// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_resumption/tab_resumption_bridge.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/tab_resumption/jni_headers/TabResumptionBridge_jni.h"

static jlong JNI_TabResumptionBridge_Init(JNIEnv* env, Profile* profile) {
  TabResumptionBridge* tab_resumption_bridge = new TabResumptionBridge(profile);
  return reinterpret_cast<intptr_t>(tab_resumption_bridge);
}

TabResumptionBridge::TabResumptionBridge(Profile* profile)
    : profile_(profile) {}

TabResumptionBridge::~TabResumptionBridge() = default;

void TabResumptionBridge::Destroy(JNIEnv* env) {
  delete this;
}
