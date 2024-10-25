// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CCT_PASSWORD_SAVING_METRICS_RECORDER_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CCT_PASSWORD_SAVING_METRICS_RECORDER_BRIDGE_H_

#include <memory>

#include "base/android/jni_android.h"
#include "content/public/browser/web_contents.h"

// Communicates with Java to record metrics related to saving passwords
// in CCTs like the time elapsed from a password form submission to the CCT
// closing for specific auth flows. The metrics recording is done on the Java
// side.
class CctPasswordSavingMetricsRecorderBridge {
 public:
  // Returns nullptr if the `WindowAndroid` retrieved from the `web_contents`
  // is null.
  static std::unique_ptr<CctPasswordSavingMetricsRecorderBridge> MaybeCreate(
      content::WebContents* web_contents);

  CctPasswordSavingMetricsRecorderBridge(
      const CctPasswordSavingMetricsRecorderBridge&) = delete;
  CctPasswordSavingMetricsRecorderBridge& operator=(
      const CctPasswordSavingMetricsRecorderBridge&) = delete;

  ~CctPasswordSavingMetricsRecorderBridge();

  // This is the signal letting the recorder know that it can start tracking
  // time.
  void OnPotentialSaveFormSubmitted();

 private:
  explicit CctPasswordSavingMetricsRecorderBridge(
      ui::WindowAndroid* window_android);

  // Reference to the instance of the Java counterpart of this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CCT_PASSWORD_SAVING_METRICS_RECORDER_BRIDGE_H_
