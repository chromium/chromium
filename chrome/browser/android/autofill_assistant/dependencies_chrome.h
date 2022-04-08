// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_CHROME_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_CHROME_H_

#include "base/android/scoped_java_ref.h"
#include "base/strings/string_piece.h"
#include "components/autofill_assistant/browser/android/dependencies.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/metrics/metrics_service_accessor.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// Interface for platform delegates that provide platform-dependent features
// and dependencies to the starter.
class DependenciesChrome : public Dependencies {
 public:
  DependenciesChrome(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jstatic_dependencies);

  std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil()
      const override;

  variations::VariationsService* GetVariationsService() const override;

  autofill::PersonalDataManager* GetPersonalDataManager() const override;

  password_manager::PasswordManagerClient* GetPasswordManagerClient(
      content::WebContents* web_contents) const override;

  std::string GetChromeSignedInEmailAddress(
      content::WebContents* web_contents) const override;

  // The AnnotateDomModelService is a KeyedService. There is only one per
  // BrowserContext.
  AnnotateDomModelService* GetOrCreateAnnotateDomModelService(
      content::BrowserContext* browser_context) const override;

  bool IsCustomTab(const content::WebContents& web_contents) const override;

  bool IsWebLayer() const override;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_CHROME_H_
