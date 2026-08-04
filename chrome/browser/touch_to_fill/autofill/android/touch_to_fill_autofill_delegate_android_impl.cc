// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_delegate_android_impl.h"

#include "base/check_deref.h"
#include "chrome/browser/android/preferences/autofill/settings_navigation_helper.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/personal_context/first_run/personal_context_first_run_service.h"

namespace autofill {

TouchToFillAutofillDelegateAndroidImpl::TouchToFillAutofillDelegateAndroidImpl(
    BrowserAutofillManager* manager)
    : manager_(CHECK_DEREF(manager)) {}

TouchToFillAutofillDelegateAndroidImpl::
    ~TouchToFillAutofillDelegateAndroidImpl() = default;

bool TouchToFillAutofillDelegateAndroidImpl::IntendsToShowTouchToFill(
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  if (ttf_autofill_state_ == TouchToFillAutofillState::kSuppressing &&
      field_id == query_field_id_) {
    return false;
  }
  personal_context::PersonalContextFirstRunService* service =
      manager_->client().GetPersonalContextFirstRunService();
  if (!service || !service->ShouldShowPersonalContextAmbientAutofillNotice()) {
    return false;
  }

  const FormStructure* form = manager_->FindCachedFormById(form_id);
  const AutofillField* field = form ? form->GetFieldById(field_id) : nullptr;
  if (!form || !field) {
    return false;
  }

  AutofillAiManager* ai_manager = manager_->client().GetAutofillAiManager();
  if (!ai_manager) {
    return false;
  }

  EntityDataManager* entity_manager = manager_->client().GetEntityDataManager();
  if (!entity_manager) {
    return false;
  }

  const std::vector<Suggestion> suggestions =
      ai_manager->GetSuggestions(*form, *field);
  auto is_personal_context_suggestion =
      [entity_manager](const Suggestion& suggestion) {
        const auto* payload =
            std::get_if<Suggestion::AutofillAiPayload>(&suggestion.payload);

        if (!payload) {
          return false;
        }

        base::optional_ref<const EntityInstance> entity =
            entity_manager->GetEntityInstance(payload->guid);

        return entity.has_value() &&
               entity->record_type() ==
                   EntityInstance::RecordType::kPersonalContext;
      };

  return std::ranges::any_of(suggestions, is_personal_context_suggestion);
}

bool TouchToFillAutofillDelegateAndroidImpl::TryToShowTouchToFill(
    const FormData& form,
    const FormFieldData& field) {
  switch (ttf_autofill_state_) {
    case TouchToFillAutofillState::kShowing:
    case TouchToFillAutofillState::kNavigatingAway:
      return true;
    case TouchToFillAutofillState::kSuppressing:
      ttf_autofill_state_ = TouchToFillAutofillState::kInactive;
      if (field.global_id() == query_field_id_) {
        return false;
      }
      break;
    case TouchToFillAutofillState::kInactive:
      break;
  }
  if (!IntendsToShowTouchToFill(form.global_id(), field.global_id())) {
    return false;
  }
  if (manager_->client().ShowAmbientAutoFillNotice(
          weak_ptr_factory_.GetWeakPtr())) {
    ttf_autofill_state_ = TouchToFillAutofillState::kShowing;
    query_field_id_ = field.global_id();
    if (personal_context::PersonalContextFirstRunService* service =
            manager_->client().GetPersonalContextFirstRunService()) {
      service->RecordAmbientAutofillNoticeImpression(
          AutofillSuggestionController::GenerateSuggestionUiSessionId()
              .value());
    }
    return true;
  }
  return false;
}

bool TouchToFillAutofillDelegateAndroidImpl::IsShowingTouchToFill() {
  switch (ttf_autofill_state_) {
    case TouchToFillAutofillState::kShowing:
      return true;
    case TouchToFillAutofillState::kInactive:
    case TouchToFillAutofillState::kNavigatingAway:
    case TouchToFillAutofillState::kSuppressing:
      return false;
  }
}

void TouchToFillAutofillDelegateAndroidImpl::HideTouchToFill() {
  switch (ttf_autofill_state_) {
    case TouchToFillAutofillState::kShowing:
      manager_->client().HideAmbientAutoFillNotice();
      ttf_autofill_state_ = TouchToFillAutofillState::kInactive;
      break;
    case TouchToFillAutofillState::kNavigatingAway:
    case TouchToFillAutofillState::kSuppressing:
    case TouchToFillAutofillState::kInactive:
      break;
  }
}

void TouchToFillAutofillDelegateAndroidImpl::OnNoticeAcknowledged() {
  if (personal_context::PersonalContextFirstRunService* service =
          manager_->client().GetPersonalContextFirstRunService()) {
    service->MarkPersonalContextAmbientAutofillNoticeAsAcknowledged();
  }
}

void TouchToFillAutofillDelegateAndroidImpl::OnSettingsLinkClicked() {
  ttf_autofill_state_ = TouchToFillAutofillState::kNavigatingAway;

  content::WebContents* web_contents =
      static_cast<ContentAutofillClient&>(manager_->client()).web_contents();
  if (!web_contents) {
    return;
  }
  ShowAutofillPersonalContextSettings(
      web_contents,
      AutofillOptionsReferrer::kPersonalContextAmbientAutofillNotice);
}

void TouchToFillAutofillDelegateAndroidImpl::OnDismissed() {
  switch (ttf_autofill_state_) {
    case TouchToFillAutofillState::kInactive:
    case TouchToFillAutofillState::kSuppressing:
      return;
    case TouchToFillAutofillState::kNavigatingAway:
      ttf_autofill_state_ = TouchToFillAutofillState::kInactive;
      break;
    case TouchToFillAutofillState::kShowing:
      ttf_autofill_state_ = TouchToFillAutofillState::kSuppressing;
      TriggerAskForValuesToFill();
      break;
  }
}

void TouchToFillAutofillDelegateAndroidImpl::TriggerAskForValuesToFill() {
  if (ttf_autofill_state_ != TouchToFillAutofillState::kSuppressing) {
    return;
  }

  manager_->driver().RendererShouldTriggerSuggestions(
      query_field_id_,
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
}

}  // namespace autofill
