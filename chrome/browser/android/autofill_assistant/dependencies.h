// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_H_

#include <memory>
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/android/autofill_assistant/assistant_field_trial_util.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// Interface for platform delegates that provide platform-dependent features
// and dependencies to the starter.
class Dependencies {
 public:
  static std::unique_ptr<Dependencies> CreateFromJavaObject(
      base::android::ScopedJavaGlobalRef<jobject> java_object);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject() const;

  static base::android::ScopedJavaGlobalRef<jobject> GetInfoPageUtil(
      const base::android::ScopedJavaGlobalRef<jobject>& java_object);

  base::android::ScopedJavaGlobalRef<jobject> GetAccessTokenUtil() const;

  virtual ~Dependencies();

  virtual std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil()
      const = 0;

  virtual variations::VariationsService* GetVariationsService() const = 0;

  virtual std::string GetChromeSignedInEmailAddress(
      content::WebContents* web_contents) const = 0;

 protected:
  Dependencies(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& java_object);

 private:
  const base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_H_
