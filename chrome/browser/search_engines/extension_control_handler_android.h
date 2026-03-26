// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_EXTENSION_CONTROL_HANDLER_ANDROID_H_
#define CHROME_BROWSER_SEARCH_ENGINES_EXTENSION_CONTROL_HANDLER_ANDROID_H_

#include <jni.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"

class ExtensionControlHandler {
 public:
  explicit ExtensionControlHandler(Profile* profile);

  ExtensionControlHandler(const ExtensionControlHandler&) = delete;
  ExtensionControlHandler& operator=(const ExtensionControlHandler&) = delete;

  ~ExtensionControlHandler();

  void DisableExtension(JNIEnv* env, std::string extension_id);
  void Destroy(JNIEnv* env);

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_EXTENSION_CONTROL_HANDLER_ANDROID_H_
