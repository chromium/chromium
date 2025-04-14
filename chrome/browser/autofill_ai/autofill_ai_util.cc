// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_ai/autofill_ai_util.h"

#include "base/feature_list.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/web_contents.h"

namespace autofill_ai {

bool CanShowAutofillAiPageInSettings(Profile* profile,
                                     content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiWithDataSchema)) {
    return false;
  }
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(web_contents);
  autofill::EntityDataManager* entity_data_manager =
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile);

  return (client && autofill::MayPerformAutofillAiAction(
                        *client, autofill::AutofillAiAction::kOptIn)) ||
         (entity_data_manager &&
          entity_data_manager->GetEntityInstances().size() > 0);
}

}  // namespace autofill_ai
