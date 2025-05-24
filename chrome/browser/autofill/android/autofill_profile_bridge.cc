// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/autofill_profile_bridge.h"

#include <algorithm>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/ui/addresses/android/autofill_address_editor_ui_info_android.h"
#include "components/autofill/core/browser/ui/addresses/android/autofill_address_ui_component_android.h"
#include "components/autofill/core/browser/ui/addresses/android/dropdown_key_value_android.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
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

static std::string JNI_AutofillProfileBridge_GetDefaultCountryCode(
    JNIEnv* env) {
  return autofill::AutofillCountry::CountryCodeForLocale(
      g_browser_process->GetApplicationLocale());
}

static std::vector<DropdownKeyValueAndroid>
JNI_AutofillProfileBridge_GetSupportedCountries(JNIEnv* env) {
  std::vector<std::string> country_codes = GetRegionCodes();
  std::vector<DropdownKeyValueAndroid> display_countries;
  display_countries.reserve(country_codes.size());
  std::string locale = g_browser_process->GetApplicationLocale();
  for (auto& country_code : country_codes) {
    std::u16string country_name =
        l10n_util::GetDisplayNameForCountry(country_code, locale);
    // Don't display a country code for which a name is not known yet.
    if (country_name != base::UTF8ToUTF16(country_code)) {
      display_countries.emplace_back(std::move(country_code),
                                     std::move(country_name));
    }
  }

  return display_countries;
}

static std::vector<int> JNI_AutofillProfileBridge_GetRequiredFields(
    JNIEnv* env,
    std::string& country_code) {
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

  return required;
}

static AutofillAddressEditorUiInfoAndroid
JNI_AutofillProfileBridge_GetAddressEditorUiInfo(JNIEnv* env,
                                                 std::string& country_code,
                                                 std::string& language_code,
                                                 jint j_validation_type) {
  std::string best_language_tag;
  Localization localization;
  localization.SetGetter(l10n_util::GetStringUTF8);

  if (language_code.empty()) {
    language_code = g_browser_process->GetApplicationLocale();
  }

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
  std::vector<AutofillAddressUiComponentAndroid> components;
  components.reserve(ui_components.size());
  for (const auto& ui_component : ui_components) {
    bool is_required = false;
    switch (validation_type) {
      case AddressValidationType::kPaymentRequest:
        is_required = i18n::IsFieldRequired(ui_component.field, country_code);
        break;
      case AddressValidationType::kAccount:
        is_required = country.IsAddressFieldRequired(ui_component.field);
    }
    components.emplace_back(
        ui_component.field, ui_component.name, is_required,
        ui_component.length_hint ==
            autofill::AutofillAddressUIComponent::HINT_LONG);
  }

  return AutofillAddressEditorUiInfoAndroid(best_language_tag, components);
}

}  // namespace autofill
