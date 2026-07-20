// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_delegate_android_impl.h"

#include "base/check_deref.h"
#include "chrome/browser/android/preferences/autofill/settings_navigation_helper.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

TouchToFillAutofillDelegateAndroidImpl::TouchToFillAutofillDelegateAndroidImpl(
    BrowserAutofillManager* manager)
    : manager_(CHECK_DEREF(manager)) {}

TouchToFillAutofillDelegateAndroidImpl::
    ~TouchToFillAutofillDelegateAndroidImpl() = default;

bool TouchToFillAutofillDelegateAndroidImpl::IntendsToShowTouchToFill(
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  if (!manager_->client().ShouldShowPersonalContextAmbientAutofillNotice()) {
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
  if (ttf_autofill_state_ ==
      TouchToFillAutofillState::kShowingPersonalContextNotice) {
    return true;
  }
  if (!IntendsToShowTouchToFill(form.global_id(), field.global_id())) {
    return false;
  }
  if (manager_->client().ShowAmbientAutoFillNotice(
          weak_ptr_factory_.GetWeakPtr())) {
    ttf_autofill_state_ =
        TouchToFillAutofillState::kShowingPersonalContextNotice;
    OnShow();
    return true;
  }
  return false;
}

bool TouchToFillAutofillDelegateAndroidImpl::IsShowingTouchToFill() {
  return ttf_autofill_state_ ==
         TouchToFillAutofillState::kShowingPersonalContextNotice;
}

void TouchToFillAutofillDelegateAndroidImpl::HideTouchToFill() {
  if (IsShowingTouchToFill()) {
    manager_->client().HideAmbientAutoFillNotice();
    ttf_autofill_state_ = TouchToFillAutofillState::kInactive;
  }
}

void TouchToFillAutofillDelegateAndroidImpl::OnShow() {
  // TODO(crbug.com/521716313): Record shown metrics.
}

void TouchToFillAutofillDelegateAndroidImpl::OnNoticeAcknowledged() {
  manager_->client().MarkPersonalContextAmbientAutofillNoticeAsAcknowledged();
  ttf_autofill_state_ = TouchToFillAutofillState::kInactive;
}

void TouchToFillAutofillDelegateAndroidImpl::OnSettingsLinkClicked() {
  content::WebContents* web_contents =
      static_cast<ContentAutofillClient&>(manager_->client()).web_contents();
  if (!web_contents) {
    return;
  }
  ShowAutofillPersonalContextSettings(
      web_contents,
      AutofillOptionsReferrer::kPersonalContextAmbientAutofillNotice);
  ttf_autofill_state_ = TouchToFillAutofillState::kInactive;
}

void TouchToFillAutofillDelegateAndroidImpl::OnDismissed() {
  ttf_autofill_state_ = TouchToFillAutofillState::kInactive;
}

}  // namespace autofill
