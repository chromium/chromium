// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_impl.h"

#include <variant>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/to_vector.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_controller.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/touch_to_fill/autofill/android/internal/jni/TouchToFillPaymentMethodViewBridge_jni.h"
#include "components/autofill/android/main_autofill_jni_headers/LoyaltyCard_jni.h"

namespace autofill {

TouchToFillPaymentMethodViewImpl::TouchToFillPaymentMethodViewImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
}

TouchToFillPaymentMethodViewImpl::~TouchToFillPaymentMethodViewImpl() {
  Hide();
}

bool TouchToFillPaymentMethodViewImpl::IsReadyToShow(
    TouchToFillPaymentMethodViewController* controller,
    JNIEnv* env) {
  if (java_object_)
    return false;  // Already shown.

  if (!web_contents_->GetNativeView() ||
      !web_contents_->GetNativeView()->GetWindowAndroid()) {
    return false;  // No window attached (yet or anymore).
  }

  DCHECK(controller);
  base::android::ScopedJavaLocalRef<jobject> java_controller =
      controller->GetJavaObject();
  if (!java_controller)
    return false;

  java_object_.Reset(Java_TouchToFillPaymentMethodViewBridge_create(
      env, java_controller,
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetJavaObject(),
      web_contents_->GetTopLevelNativeWindow()->GetJavaObject()));
  if (!java_object_)
    return false;

  return true;
}

bool TouchToFillPaymentMethodViewImpl::ShowCreditCards(
    TouchToFillPaymentMethodViewController* controller,
    base::span<const Suggestion> suggestions,
    bool should_show_scan_credit_card) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!IsReadyToShow(controller, env)) {
    return false;
  }

  std::vector<base::android::ScopedJavaLocalRef<jobject>> suggestions_array;
  suggestions_array.reserve(suggestions.size());
  for (const Suggestion& suggestion : suggestions) {
    CHECK_GT(suggestion.labels.size(), 0U);
    CHECK_EQ(suggestion.labels[0].size(), 1U);
    std::u16string secondarySubLabel =
        suggestion.labels.size() > 1 && suggestion.labels[1].size() > 0 &&
                !suggestion.labels[1][0].value.empty()
            ? suggestion.labels[1][0].value
            : u"";
    std::u16string minor_text = base::JoinString(
        base::ToVector(suggestion.minor_texts, &Suggestion::Text::value), u" ");

    Suggestion::PaymentsPayload payments_payload =
        suggestion.GetPayload<Suggestion::PaymentsPayload>();
    const Suggestion::CustomIconUrl* custom_icon_url =
        std::get_if<Suggestion::CustomIconUrl>(&suggestion.custom_icon);
    int android_icon_id = 0;
    if (suggestion.icon != Suggestion::Icon::kNoIcon) {
      android_icon_id =
          controller->GetJavaResourceId(GetIconResourceID(suggestion.icon));
    }
    suggestions_array.push_back(
        Java_TouchToFillPaymentMethodViewBridge_createAutofillSuggestion(
            env, suggestion.main_text.value, minor_text,
            suggestion.labels[0][0].value, secondarySubLabel,
            payments_payload.main_text_content_description,
            base::to_underlying(suggestion.type),
            custom_icon_url ? url::GURLAndroid::FromNativeGURL(
                                  env, custom_icon_url->value())
                            : url::GURLAndroid::EmptyGURL(env),
            android_icon_id, suggestion.HasDeactivatedStyle(),
            payments_payload.should_display_terms_available,
            payments_payload.guid.value(),
            payments_payload.is_local_payments_method));
  }
  Java_TouchToFillPaymentMethodViewBridge_showCreditCards(
      env, java_object_, std::move(suggestions_array),
      should_show_scan_credit_card);
  return true;
}

bool TouchToFillPaymentMethodViewImpl::ShowIbans(
    TouchToFillPaymentMethodViewController* controller,
    base::span<const autofill::Iban> ibans_to_suggest) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!IsReadyToShow(controller, env)) {
    return false;
  }

  std::vector<base::android::ScopedJavaLocalRef<jobject>> ibans_array;
  ibans_array.reserve(ibans_to_suggest.size());
  for (const autofill::Iban& iban : ibans_to_suggest) {
    ibans_array.push_back(
        PersonalDataManagerAndroid::CreateJavaIbanFromNative(env, iban));
  }
  Java_TouchToFillPaymentMethodViewBridge_showIbans(env, java_object_,
                                                    std::move(ibans_array));
  return true;
}

bool TouchToFillPaymentMethodViewImpl::ShowLoyaltyCards(
    TouchToFillPaymentMethodViewController* controller,
    base::span<const LoyaltyCard> loyalty_cards_to_suggest) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!IsReadyToShow(controller, env)) {
    return false;
  }

  std::vector<base::android::ScopedJavaLocalRef<jobject>> loyalty_cards_array;
  loyalty_cards_array.reserve(loyalty_cards_to_suggest.size());
  for (const LoyaltyCard& loyalty_card : loyalty_cards_to_suggest) {
    loyalty_cards_array.push_back(Java_LoyaltyCard_Constructor(
        env, *loyalty_card.id(), loyalty_card.merchant_name(),
        loyalty_card.program_name(), loyalty_card.program_logo(),
        loyalty_card.loyalty_card_number(), loyalty_card.merchant_domains()));
  }
  Java_TouchToFillPaymentMethodViewBridge_showLoyaltyCards(
      env, java_object_, std::move(loyalty_cards_array));

  return true;
}

void TouchToFillPaymentMethodViewImpl::Hide() {
  if (java_object_) {
    Java_TouchToFillPaymentMethodViewBridge_hideSheet(
        base::android::AttachCurrentThread(), java_object_);
  }
}

}  // namespace autofill
