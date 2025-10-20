// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_impl.h"

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_controller.h"
#include "components/autofill/android/payments/legal_message_line_android.h"
#include "components/autofill/core/browser/data_model/valuables/android/loyalty_card_android.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TouchToFillPaymentMethodViewBridge_jni.h"
#include "components/autofill/android/main_autofill_jni_headers/LoyaltyCard_jni.h"
#include "components/autofill/android/payments_jni_headers/BnplIssuerContext_jni.h"
#include "components/autofill/android/payments_jni_headers/BnplIssuerTosDetail_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

namespace {

static base::android::ScopedJavaLocalRef<jobject>
ConvertTextWithLinkToJavaObject(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    const autofill::payments::TextWithLink& link_text) {
  return autofill::Java_TouchToFillPaymentMethodViewBridge_getSpannableString(
      env, obj, ConvertUTF16ToJavaString(env, link_text.text),
      static_cast<int>(link_text.offset.start()),
      static_cast<int>(link_text.offset.end()),
      ConvertUTF8ToJavaString(env, link_text.url.spec()));
}

static base::android::ScopedJavaLocalRef<jobject>
ConvertLegalMessageLinesToJavaObject(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    const autofill::LegalMessageLines legal_message_lines) {
  return autofill::
      Java_TouchToFillPaymentMethodViewBridge_convertLegalMessageLinesForBnplTos(
          env, obj,
          autofill::LegalMessageLineAndroid::ConvertToJavaLinkedList(
              legal_message_lines));
}

static base::android::ScopedJavaLocalRef<jobject>
ConvertBnplIssuerTosDetailToJavaObject(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    const autofill::TouchToFillPaymentMethodViewController& controller,
    const autofill::payments::BnplIssuerTosDetail& bnpl_issuer_tos_detail) {
  return Java_BnplIssuerTosDetail_Constructor(
      env, controller.GetJavaResourceId(bnpl_issuer_tos_detail.header_icon_id),
      controller.GetJavaResourceId(bnpl_issuer_tos_detail.header_icon_id_dark),
      ConvertUTF16ToJavaString(env, bnpl_issuer_tos_detail.title),
      ConvertUTF16ToJavaString(env, bnpl_issuer_tos_detail.review_text),
      ConvertUTF16ToJavaString(env, bnpl_issuer_tos_detail.approve_text),
      ConvertTextWithLinkToJavaObject(env, obj,
                                      bnpl_issuer_tos_detail.link_text),
      ConvertLegalMessageLinesToJavaObject(
          env, obj, bnpl_issuer_tos_detail.legal_message_lines));
}

// TODO(crbug.com/449764859): Refactor BnplIssuerContext to use JNI type
// converters.
static base::android::ScopedJavaLocalRef<jobject>
CreateJavaBnplIssuerContextFromNative(
    JNIEnv* env,
    const autofill::payments::BnplIssuerContext& bnpl_issuer_context) {
  // For now, Android only uses the `LightModeImageId`.
  const std::pair<autofill::BnplIssuer::LightModeImageId,
                  autofill::BnplIssuer::DarkModeImageId>
      image_ids = GetBnplIssuerIconIds(
          bnpl_issuer_context.issuer.issuer_id(),
          /*issuer_linked=*/bnpl_issuer_context.issuer.payment_instrument()
              .has_value());

  // TODO(crbug.com/430575808): App locale will be provided to `ShowBnplIssuers`
  // in crrev.com/c/7005163. Once this CL is merged remove the hard-coded app
  // locale, "en-US".
  const std::u16string selection_text =
      autofill::payments::GetBnplIssuerSelectionOptionText(
          bnpl_issuer_context.issuer.issuer_id(), "en-US",
          {bnpl_issuer_context});

  return autofill::Java_BnplIssuerContext_Constructor(
      env, image_ids.first.value(),
      std::string(
          ConvertToBnplIssuerIdString(bnpl_issuer_context.issuer.issuer_id())),
      bnpl_issuer_context.issuer.GetDisplayName(), selection_text,
      bnpl_issuer_context.issuer.payment_instrument().has_value(),
      bnpl_issuer_context.IsEligible());
}

}  // namespace

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

bool TouchToFillPaymentMethodViewImpl::ShowPaymentMethods(
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
            base::to_underlying(suggestion.type),
            custom_icon_url ? url::GURLAndroid::FromNativeGURL(
                                  env, custom_icon_url->value())
                            : url::GURLAndroid::EmptyGURL(env),
            android_icon_id, suggestion.HasDeactivatedStyle(),
            payments_payload.CreateJavaObject()));
  }
  Java_TouchToFillPaymentMethodViewBridge_showPaymentMethods(
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
    base::span<const LoyaltyCard> affiliated_loyalty_cards,
    base::span<const LoyaltyCard> all_loyalty_cards,
    bool first_time_usage) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!IsReadyToShow(controller, env)) {
    return false;
  }

  // TODO: crbug.com/421839554 - Pass a boolean indicating whether the user has
  // seen the feature promotion UI or not.
  Java_TouchToFillPaymentMethodViewBridge_showLoyaltyCards(
      env, java_object_, affiliated_loyalty_cards, all_loyalty_cards,
      first_time_usage);

  return true;
}

bool TouchToFillPaymentMethodViewImpl::UpdateBnplPaymentMethod(
    std::optional<uint64_t> extracted_amount,
    bool is_amount_supported_by_any_issuer) {
  if (!java_object_) {
    return false;
  }
  std::optional<int64_t> final_extracted_amount;
  if (extracted_amount.has_value()) {
    final_extracted_amount = static_cast<int64_t>(extracted_amount.value());
  }
  Java_TouchToFillPaymentMethodViewBridge_updateBnplPaymentMethod(
      base::android::AttachCurrentThread(), java_object_,
      final_extracted_amount, is_amount_supported_by_any_issuer);
  return true;
}

bool TouchToFillPaymentMethodViewImpl::ShowProgressScreen(
    TouchToFillPaymentMethodViewController* controller) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // If the TTF surface isn't already showing, and a new surface is not ready to
  // show, return that showing the progress screen failed, as the progress
  // screen can not be shown.
  if (!java_object_ && !IsReadyToShow(controller, env)) {
    return false;
  }

  // Use either the old `java_object_` or the new one created in
  // `IsReadyToShow()` to show the progress screen.
  Java_TouchToFillPaymentMethodViewBridge_showProgressScreen(env, java_object_);
  return true;
}

bool TouchToFillPaymentMethodViewImpl::ShowBnplIssuers(
    base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts) {
  if (!java_object_) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<base::android::ScopedJavaLocalRef<jobject>> issuer_context_array;
  issuer_context_array.reserve(bnpl_issuer_contexts.size());
  for (const payments::BnplIssuerContext& issuer_context :
       bnpl_issuer_contexts) {
    issuer_context_array.push_back(
        CreateJavaBnplIssuerContextFromNative(env, issuer_context));
  }

  // Pass only the raw string to Java. The link's start/end indices from
  // `GetBnplUiFooterText()` are no longer needed, as the link's position is
  // defined declaratively by `<link>` tags within the string resource. The
  // Android UI layer is responsible for creating the clickable span.
  Java_TouchToFillPaymentMethodViewBridge_showBnplIssuers(
      env, java_object_, std::move(issuer_context_array),
      ConvertUTF16ToJavaString(env, payments::GetBnplUiFooterText()));
  return true;
}

bool TouchToFillPaymentMethodViewImpl::ShowErrorScreen(
    TouchToFillPaymentMethodViewController* controller,
    const std::u16string& title,
    const std::u16string& description) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // If the TTF surface isn't already showing, and a new surface is not ready to
  // show, return that showing the error screen failed, as the error screen can
  // not be shown.
  if (!java_object_ && !IsReadyToShow(controller, env)) {
    return false;
  }

  // Use either the old `java_object_` or the new one created in
  // `IsReadyToShow()` to show the error screen.
  Java_TouchToFillPaymentMethodViewBridge_showErrorScreen(env, java_object_,
                                                          title, description);

  return true;
}

bool TouchToFillPaymentMethodViewImpl::ShowBnplIssuerTos(
    const TouchToFillPaymentMethodViewController& controller,
    const payments::BnplIssuerTosDetail& bnpl_issuer_tos_detail) {
  if (!java_object_) {
    return false;  // View should already be shown.
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  Java_TouchToFillPaymentMethodViewBridge_showBnplIssuerTos(
      env, java_object_,
      ConvertBnplIssuerTosDetailToJavaObject(env, java_object_, controller,
                                             bnpl_issuer_tos_detail));

  return true;
}

void TouchToFillPaymentMethodViewImpl::Hide() {
  if (java_object_) {
    Java_TouchToFillPaymentMethodViewBridge_hideSheet(
        base::android::AttachCurrentThread(), java_object_);
  }
}

}  // namespace autofill
