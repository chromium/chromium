// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_CHROME_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_CHROME_H_

#include "chrome/browser/android/autofill_assistant/dependencies.h"

#include "base/android/scoped_java_ref.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/android/autofill_assistant/assistant_field_trial_util.h"
#include "components/metrics/metrics_service_accessor.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// Interface for platform delegates that provide platform-dependent features
// and dependencies to the starter.
class DependenciesChrome : public Dependencies {
 public:
  DependenciesChrome(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& java_object);

  std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil()
      const override;

  variations::VariationsService* GetVariationsService() const override;

  std::string GetChromeSignedInEmailAddress(
      content::WebContents* web_contents) const override;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_CHROME_H_
