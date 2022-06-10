// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator.h"

#include <memory>

#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// static
std::unique_ptr<ApcOnboardingCoordinator> ApcOnboardingCoordinator::Create(
    content::WebContents* web_contents) {
  return std::make_unique<ApcOnboardingCoordinatorImpl>(web_contents);
}

// static
AssistantOnboardingInformation
ApcOnboardingCoordinator::CreateOnboardingInformation() {
  // TODO(crbug.com/1322387): Remove the "translateable='false'" tag in the
  // string definition once all strings are final.
  AssistantOnboardingInformation info;
  info.title = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_TITLE);
  info.description = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_DESCRIPTION);
  info.consent_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_CONSENT_TEXT);
  info.learn_more_title = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_LEARN_MORE);
  info.button_cancel_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_BUTTON_CANCEL_TEXT);
  info.button_accept_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_BUTTON_ACCEPT_TEXT);

  // TODO(crbug.com/1322387): Update link so that it also applies to Desktop.
  info.learn_more_url = GURL(
      "https://support.google.com/assistant/answer/"
      "9201753?visit_id=637880404267471228-1286648363&p=fast_checkout&rd=1");

  return info;
}
