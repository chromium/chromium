// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_COMMON_DEPENDENCIES_CHROME_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_COMMON_DEPENDENCIES_CHROME_H_

#include "base/strings/string_piece.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/common_dependencies.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/metrics/metrics_service_accessor.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// Chrome implementation of the CommonDependencies interface.
class CommonDependenciesChrome : public CommonDependencies {
 public:
  CommonDependenciesChrome();

  std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil()
      const override;

  std::string GetLocale() const override;

  std::string GetCountryCode() const override;

  autofill::PersonalDataManager* GetPersonalDataManager(
      content::BrowserContext* browser_context) const override;

  password_manager::PasswordManagerClient* GetPasswordManagerClient(
      content::WebContents* web_contents) const override;

  std::string GetSignedInEmail(
      content::BrowserContext* browser_context) const override;

  bool IsSupervisedUser(
      content::BrowserContext* browser_context) const override;

  // The AnnotateDomModelService is a KeyedService. There is only one per
  // BrowserContext.
  AnnotateDomModelService* GetOrCreateAnnotateDomModelService(
      content::BrowserContext* browser_context) const override;

  bool IsWebLayer() const override;

  signin::IdentityManager* GetIdentityManager(
      content::BrowserContext* browser_context) const override;

  version_info::Channel GetChannel() const override;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_COMMON_DEPENDENCIES_CHROME_H_
