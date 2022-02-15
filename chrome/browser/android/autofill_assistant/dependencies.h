// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_H_

#include <memory>
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/android/autofill_assistant/assistant_field_trial_util.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// Interface for platform delegates that provide platform-dependent features
// and dependencies to the starter.
class Dependencies {
 public:
  static std::unique_ptr<Dependencies> CreateFromJavaStaticDependencies(
      const base::android::JavaRef<jobject>& jstatic_dependencies);

  static std::unique_ptr<Dependencies> CreateFromJavaDependencies(
      const base::android::JavaRef<jobject>& jdependencies);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaStaticDependencies() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateInfoPageUtil() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateAccessTokenUtil() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateImageFetcher() const;
  base::android::ScopedJavaGlobalRef<jobject> CreateIconBridge() const;

  bool IsAccessibilityEnabled() const;

  virtual ~Dependencies();

  virtual std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil()
      const = 0;

  virtual variations::VariationsService* GetVariationsService() const = 0;

  virtual std::string GetChromeSignedInEmailAddress(
      content::WebContents* web_contents) const = 0;

  virtual AnnotateDomModelService* GetOrCreateAnnotateDomModelService(
      content::BrowserContext* browser_context) const = 0;

  virtual bool IsCustomTab(const content::WebContents& web_contents) const = 0;

 protected:
  Dependencies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jstatic_dependencies);

 private:
  const base::android::ScopedJavaGlobalRef<jobject> jstatic_dependencies_;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_H_
