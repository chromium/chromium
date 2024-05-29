// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/autofill/autofill_profile_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_metadata.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/AutofillProfileBridge_jni.h"

namespace autofill {

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaIntArray;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressUiComponent;
using ::i18n::addressinput::BuildComponents;
using ::i18n::addressinput::COUNTRY;
using ::i18n::addressinput::GetRegionCodes;
using ::i18n::addressinput::Localization;
using ::i18n::addressinput::RECIPIENT;

static ScopedJavaLocalRef<jstring>
JNI_AutofillProfileBridge_GetDefaultCountryCode(JNIEnv* env) {
  std::string default_country_code =
      autofill::AutofillCountry::CountryCodeForLocale(
          g_browser_process->GetApplicationLocale());
  return ConvertUTF8ToJavaString(env, default_country_code);
}

static void JNI_AutofillProfileBridge_GetSupportedCountries(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_country_code_list,
    const JavaParamRef<jobject>& j_country_name_list) {
  std::vector<std::string> country_codes = GetRegionCodes();
  std::vector<std::string> known_country_codes;
  std::vector<std::u16string> known_country_names;
  std::string locale = g_browser_process->GetApplicationLocale();
  for (auto country_code : country_codes) {
    const std::u16string& country_name =
        l10n_util::GetDisplayNameForCountry(country_code, locale);
    // Don't display a country code for which a name is not known yet.
    if (country_name != base::UTF8ToUTF16(country_code)) {
      known_country_codes.push_back(country_code);
      known_country_names.push_back(country_name);
    }
  }

  Java_AutofillProfileBridge_stringArrayToList(
      env, ToJavaArrayOfStrings(env, known_country_codes), j_country_code_list);
  Java_AutofillProfileBridge_stringArrayToList(
      env, ToJavaArrayOfStrings(env, known_country_names), j_country_name_list);
}

static void JNI_AutofillProfileBridge_GetRequiredFields(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_country_code,
    const JavaParamRef<jobject>& j_required_fields_list) {
  std::string country_code = ConvertJavaStringToUTF8(env, j_country_code);
  std::vector<int> required;

  // Iterating over fields in AddressField to ensure that only fields from
  // libaddressinput can be required. Should iterate over all fields in:
  // third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h
  for (int i = COUNTRY; i <= RECIPIENT; ++i) {
    AddressField field = static_cast<AddressField>(i);
    if (IsFieldRequired(field, country_code)) {
      required.push_back(i18n::TypeForField(field));
    }
  }

  Java_AutofillProfileBridge_intArrayToList(env, ToJavaIntArray(env, required),
                                            j_required_fields_list);
}

static ScopedJavaLocalRef<jstring>
JNI_AutofillProfileBridge_GetAddressUiComponents(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_country_code,
    const JavaParamRef<jstring>& j_language_code,
    jint j_validation_type,
    const JavaParamRef<jobject>& j_id_list,
    const JavaParamRef<jobject>& j_name_list,
    const JavaParamRef<jobject>& j_required_list,
    const JavaParamRef<jobject>& j_length_list) {
  std::string best_language_tag;
  std::vector<int> component_ids;
  std::vector<std::string> component_labels;
  std::vector<int> component_required;
  std::vector<int> component_length;
  Localization localization;
  localization.SetGetter(l10n_util::GetStringUTF8);

  std::string language_code;
  if (j_language_code != NULL) {
    language_code = ConvertJavaStringToUTF8(env, j_language_code);
  }
  if (language_code.empty()) {
    language_code = g_browser_process->GetApplicationLocale();
  }

  std::string country_code = ConvertJavaStringToUTF8(env, j_country_code);
  AutofillCountry country(country_code);
  std::vector<AutofillAddressUIComponent> ui_components =
      ConvertAddressUiComponents(
          BuildComponents(country_code, localization, language_code,
                          &best_language_tag),
          country);
  ExtendAddressComponents(ui_components, country, localization,
                          /*include_literals=*/false);

  AddressValidationType validation_type =
      static_cast<AddressValidationType>(j_validation_type);
  for (const auto& ui_component : ui_components) {
    component_ids.push_back(ui_component.field);
    component_labels.push_back(ui_component.name);
    component_length.push_back(ui_component.length_hint ==
                               autofill::AutofillAddressUIComponent::HINT_LONG);

    switch (validation_type) {
      case AddressValidationType::kPaymentRequest:
        component_required.push_back(
            i18n::IsFieldRequired(ui_component.field, country_code));
        break;
      case AddressValidationType::kAccount:
        component_required.push_back(
            country.IsAddressFieldRequired(ui_component.field));
    }
  }

  Java_AutofillProfileBridge_intArrayToList(
      env, ToJavaIntArray(env, component_ids), j_id_list);
  Java_AutofillProfileBridge_stringArrayToList(
      env, ToJavaArrayOfStrings(env, component_labels), j_name_list);
  Java_AutofillProfileBridge_intArrayToList(
      env, ToJavaIntArray(env, component_required), j_required_list);
  Java_AutofillProfileBridge_intArrayToList(
      env, ToJavaIntArray(env, component_length), j_length_list);

  return ConvertUTF8ToJavaString(env, best_language_tag);
}

}  // namespace autofill
