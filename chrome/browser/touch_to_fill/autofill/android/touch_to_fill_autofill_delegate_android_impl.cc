// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_delegate_android_impl.h"

#include <algorithm>
#include <optional>
#include <vector>

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

namespace {

// Returns whether any of the `suggestions` is going to fill a Personal Context
// entity.
bool HasPersonalContextSuggestion(const std::vector<Suggestion>& suggestions,
                                  const EntityDataManager& entity_manager) {
  auto is_personal_context_suggestion =
      [&entity_manager](const Suggestion& suggestion) {
        const auto* payload =
            std::get_if<Suggestion::AutofillAiPayload>(&suggestion.payload);

        if (!payload) {
          return false;
        }

        base::optional_ref<const EntityInstance> entity =
            entity_manager.GetEntityInstance(payload->guid);

        return entity.has_value() &&
               entity->record_type() ==
                   EntityInstance::RecordType::kPersonalContext;
      };

  return std::ranges::any_of(suggestions, is_personal_context_suggestion);
}

}  // namespace

TouchToFillAutofillDelegateAndroidImpl::TouchToFillAutofillDelegateAndroidImpl(
    BrowserAutofillManager* manager)
    : manager_(CHECK_DEREF(manager)) {}

TouchToFillAutofillDelegateAndroidImpl::
    ~TouchToFillAutofillDelegateAndroidImpl() = default;

std::optional<TouchToFillAutofillDelegateAndroidImpl::NoticeType>
TouchToFillAutofillDelegateAndroidImpl::GetNoticeToShow(
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  if (ttf_autofill_state_ == TouchToFillAutofillState::kSuppressing &&
      field_id == query_field_id_) {
    return std::nullopt;
  }

  personal_context::PersonalContextFirstRunService* first_run_service =
      manager_->client().GetPersonalContextFirstRunService();
  if (!first_run_service ||
      !first_run_service->ShouldShowPersonalContextAmbientAutofillNotice()) {
    return std::nullopt;
  }

  const FormStructure* form = manager_->FindCachedFormById(form_id);
  const AutofillField* field = form ? form->GetFieldById(field_id) : nullptr;
  if (!form || !field) {
    return std::nullopt;
  }

  AutofillAiManager* ai_manager = manager_->client().GetAutofillAiManager();
  if (!ai_manager) {
    return std::nullopt;
  }

  const EntityDataManager* entity_manager =
      manager_->client().GetEntityDataManager();
  if (!entity_manager) {
    return std::nullopt;
  }

  const std::vector<Suggestion> suggestions =
      ai_manager->GetSuggestions(*form, *field);
  if (HasPersonalContextSuggestion(suggestions, *entity_manager)) {
    return NoticeType::kPersonalContext;
  }

  return std::nullopt;
}

bool TouchToFillAutofillDelegateAndroidImpl::IntendsToShowTouchToFill(
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  return GetNoticeToShow(form_id, field_id).has_value();
}

bool TouchToFillAutofillDelegateAndroidImpl::ShowNotice(NoticeType notice) {
  switch (notice) {
    case NoticeType::kPersonalContext:
      if (!manager_->client().ShowAmbientAutofillNotice(
              weak_ptr_factory_.GetWeakPtr())) {
        return false;
      }
      if (personal_context::PersonalContextFirstRunService* service =
              manager_->client().GetPersonalContextFirstRunService()) {
        service->RecordAmbientAutofillNoticeImpression(
            AutofillSuggestionController::GenerateSuggestionUiSessionId()
                .value());
      }
      return true;
  }
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
  std::optional<NoticeType> notice =
      GetNoticeToShow(form.global_id(), field.global_id());
  if (!notice) {
    return false;
  }
  if (!ShowNotice(*notice)) {
    return false;
  }
  shown_notice_ = *notice;
  ttf_autofill_state_ = TouchToFillAutofillState::kShowing;
  query_field_id_ = field.global_id();
  return true;
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
      manager_->client().HideAmbientAutofillNotice();
      ttf_autofill_state_ = TouchToFillAutofillState::kInactive;
      break;
    case TouchToFillAutofillState::kNavigatingAway:
    case TouchToFillAutofillState::kSuppressing:
    case TouchToFillAutofillState::kInactive:
      break;
  }
}

void TouchToFillAutofillDelegateAndroidImpl::
    OnPersonalContextNoticeAcknowledged() {
  if (personal_context::PersonalContextFirstRunService* service =
          manager_->client().GetPersonalContextFirstRunService()) {
    service->MarkPersonalContextAmbientAutofillNoticeAsAcknowledged();
  }
}

void TouchToFillAutofillDelegateAndroidImpl::OnNoticeAcknowledged() {
  if (!shown_notice_) {
    return;
  }
  switch (*shown_notice_) {
    case NoticeType::kPersonalContext:
      OnPersonalContextNoticeAcknowledged();
      break;
  }
}

void TouchToFillAutofillDelegateAndroidImpl::OnSettingsLinkClicked() {
  if (!shown_notice_) {
    return;
  }
  ttf_autofill_state_ = TouchToFillAutofillState::kNavigatingAway;
  switch (*shown_notice_) {
    case NoticeType::kPersonalContext:
      if (content::WebContents* web_contents =
              static_cast<ContentAutofillClient&>(manager_->client())
                  .web_contents()) {
        ShowAutofillPersonalContextSettings(
            web_contents,
            AutofillOptionsReferrer::kPersonalContextAmbientAutofillNotice);
      }
      break;
  }
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

  // TODO(crbug.com/547562303): Introduce a new AutofillSuggestionTriggerSource
  // to prevent throttling.
  manager_->driver().RendererShouldTriggerSuggestions(
      query_field_id_,
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
}

}  // namespace autofill
