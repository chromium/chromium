// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/unified_password_manager_proto_utils.h"

#include <optional>
#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/password_manager/android/protos/list_affiliated_passwords_result.pb.h"
#include "chrome/browser/password_manager/android/protos/list_passwords_result.pb.h"
#include "chrome/browser/password_manager/android/protos/password_with_local_data.pb.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"

using autofill::FormData;
using autofill::FormFieldData;

namespace password_manager {

namespace {

// Keys used to serialize and deserialize password form data.
constexpr char kActionKey[] = "action";
constexpr char kFieldsKey[] = "fields";
constexpr char kFormControlTypeKey[] = "form_control_type";
constexpr char kFormDataKey[] = "form_data";
constexpr char kNameKey[] = "name";
constexpr char kSkipZeroClickKey[] = "skip_zero_click";
constexpr char kUrlKey[] = "url";

base::DictValue SerializeSignatureRelevantMembersInFormData(
    const FormData& form_data) {
  base::DictValue serialized_data;
  // Stored FormData is used only for signature calculations, therefore only
  // members that are used for signature calculation are stored.
  serialized_data.Set(kNameKey, form_data.name());
  serialized_data.Set(kUrlKey, form_data.url().spec());
  serialized_data.Set(kActionKey, form_data.action().spec());

  base::ListValue serialized_fields;
  for (const auto& field : form_data.fields()) {
    base::DictValue serialized_field;
    // Stored FormFieldData is used only for signature calculations, therefore
    // only members that are used for signature calculation are stored.
    serialized_field.Set(kNameKey, field.name());
    serialized_field.Set(kFormControlTypeKey, autofill::FormControlTypeToString(
                                                  field.form_control_type()));
    serialized_fields.Append(std::move(serialized_field));
  }
  serialized_data.Set(kFieldsKey, std::move(serialized_fields));
  return serialized_data;
}

std::string SerializeOpaqueLocalData(const StoredCredential& credential) {
  base::DictValue local_data_json;
  local_data_json.Set(kSkipZeroClickKey, credential.skip_zero_click);

  base::DictValue serialized_form_data =
      SerializeSignatureRelevantMembersInFormData(credential.form_data);
  local_data_json.Set(kFormDataKey, std::move(serialized_form_data));

  std::optional<std::string> serialized_local_data =
      base::WriteJson(local_data_json);
  return serialized_local_data.value_or(std::string());
}

std::optional<FormData> DeserializeFormData(base::DictValue& serialized_data) {
  std::string* form_name = serialized_data.FindString(kNameKey);
  std::string* form_url = serialized_data.FindString(kUrlKey);
  std::string* form_action = serialized_data.FindString(kActionKey);
  base::ListValue* fields = serialized_data.FindList(kFieldsKey);
  if (!form_name || !form_url || !form_action || !fields) {
    return std::nullopt;
  }

  std::vector<FormFieldData> form_fields;
  form_fields.reserve(fields->size());
  for (auto& serialized_field : *fields) {
    base::DictValue* serialized_field_dictionary = serialized_field.GetIfDict();
    if (!serialized_field_dictionary) {
      return std::nullopt;
    }
    FormFieldData field;
    std::string* field_name = serialized_field_dictionary->FindString(kNameKey);
    std::string* field_type =
        serialized_field_dictionary->FindString(kFormControlTypeKey);
    if (!field_name || !field_type) {
      return std::nullopt;
    }
    field.set_name(base::UTF8ToUTF16(*field_name));
    // TODO(crbug.com/40858431): Why does the Password Manager
    // (de)serialize form control types?
    field.set_form_control_type(
        autofill::StringToFormControlTypeDiscouraged(*field_type)
            .value_or(autofill::FormControlType::kInputText));
    form_fields.push_back(field);
  }

  FormData form_data;
  form_data.set_name(base::UTF8ToUTF16(*form_name));
  form_data.set_url(GURL(*form_url));
  form_data.set_action(GURL(*form_action));
  form_data.set_fields(std::move(form_fields));
  return form_data;
}

void DeserializeOpaqueLocalData(const std::string& opaque_metadata,
                                StoredCredential& credential) {
  std::optional<base::DictValue> root = base::JSONReader::ReadDict(
      opaque_metadata, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!root) {
    return;
  }

  auto skip_zero_click = root->FindBool(kSkipZeroClickKey);
  auto* serialized_form_data = root->FindDict(kFormDataKey);
  if (!skip_zero_click.has_value() || !serialized_form_data) {
    return;
  }
  std::optional<FormData> form_data =
      DeserializeFormData(*serialized_form_data);
  if (!form_data.has_value()) {
    return;
  }
  credential.skip_zero_click = *skip_zero_click;
  credential.form_data = std::move(form_data.value());
}

void SetStoreForCredential(StoredCredential& credential,
                           IsAccountStore is_account_store) {
  credential.in_store = is_account_store ? PasswordForm::Store::kAccountStore
                                         : PasswordForm::Store::kProfileStore;
}

}  // namespace

PasswordWithLocalData PasswordWithLocalDataFromStoredCredential(
    const StoredCredential& credential) {
  PasswordWithLocalData password_with_local_data;

  *password_with_local_data.mutable_password_specifics_data() =
      SpecificsDataFromStoredCredential(credential, /*base_password_data=*/{});

  auto* local_data = password_with_local_data.mutable_local_data();
  local_data->set_opaque_metadata(SerializeOpaqueLocalData(credential));
  if (!credential.previously_associated_sync_account_email.empty()) {
    local_data->set_previously_associated_sync_account_email(
        credential.previously_associated_sync_account_email);
  }

  return password_with_local_data;
}

StoredCredential StoredCredentialFromProtoWithLocalData(
    const PasswordWithLocalData& password) {
  StoredCredential cred =
      StoredCredentialFromSpecifics(password.password_specifics_data());
  cred.previously_associated_sync_account_email =
      password.local_data().previously_associated_sync_account_email();
  DeserializeOpaqueLocalData(password.local_data().opaque_metadata(), cred);
  return cred;
}

std::vector<StoredCredential> StoredCredentialVectorFromListResult(
    const ListPasswordsResult& list_result,
    IsAccountStore is_account_store) {
  std::vector<StoredCredential> credentials;
  for (const PasswordWithLocalData& password : list_result.password_data()) {
    credentials.push_back(StoredCredentialFromProtoWithLocalData(password));
    SetStoreForCredential(credentials.back(), is_account_store);
  }
  return credentials;
}

std::vector<StoredCredential> StoredCredentialVectorFromListResult(
    const ListAffiliatedPasswordsResult& list_result,
    IsAccountStore is_account_store) {
  std::vector<StoredCredential> credentials;
  for (const auto& password : list_result.affiliated_passwords()) {
    StoredCredential cred =
        StoredCredentialFromProtoWithLocalData(password.password_data());
    cred.app_display_name = password.password_branding_info().display_name();
    cred.app_icon_url = GURL(password.password_branding_info().icon_url());
    if (password.is_credential_sharing_affiliation_match()) {
      cred.match_type |= PasswordForm::MatchType::kAffiliated;
    }
    if (password.is_grouping_affiliation_match()) {
      cred.match_type |= PasswordForm::MatchType::kGrouped;
    }
    SetStoreForCredential(cred, is_account_store);
    credentials.push_back(std::move(cred));
  }
  return credentials;
}

std::vector<StoredCredential> StoredCredentialVectorFromListResult(
    const ListPasswordsWithUiInfoResult& list_result,
    IsAccountStore is_account_store) {
  std::vector<StoredCredential> credentials;
  for (const auto& password : list_result.passwords_with_ui_info()) {
    StoredCredential cred =
        StoredCredentialFromProtoWithLocalData(password.password_data());
    cred.app_display_name = password.ui_info().display_name();
    cred.app_icon_url = GURL(password.ui_info().icon_url());
    SetStoreForCredential(cred, is_account_store);
    credentials.push_back(std::move(cred));
  }
  return credentials;
}

}  // namespace password_manager
