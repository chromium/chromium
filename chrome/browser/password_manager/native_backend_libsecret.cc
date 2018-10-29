// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/native_backend_libsecret.h"

#include <stddef.h>
#include <stdint.h>

#include <libsecret/secret.h>

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager_util_linux.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "url/origin.h"

using autofill::PasswordForm;
using base::UTF8ToUTF16;
using base::UTF16ToUTF8;
using password_manager::MatchResult;
using password_manager::PasswordStore;

namespace {

// Schema is analagous to the fields in PasswordForm.
const SecretSchema kLibsecretSchema = {
    "chrome_libsecret_password_schema",
    // We have to use SECRET_SCHEMA_DONT_MATCH_NAME in order to get old
    // passwords stored with gnome_keyring.
    SECRET_SCHEMA_DONT_MATCH_NAME,
    {{"origin_url", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"action_url", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"username_element", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"username_value", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"password_element", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"submit_element", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"signon_realm", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"preferred", SECRET_SCHEMA_ATTRIBUTE_INTEGER},
     {"date_created", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"blacklisted_by_user", SECRET_SCHEMA_ATTRIBUTE_INTEGER},
     {"scheme", SECRET_SCHEMA_ATTRIBUTE_INTEGER},
     {"type", SECRET_SCHEMA_ATTRIBUTE_INTEGER},
     {"times_used", SECRET_SCHEMA_ATTRIBUTE_INTEGER},
     {"date_synced", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"display_name", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"avatar_url", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"federation_url", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"should_skip_zero_click", SECRET_SCHEMA_ATTRIBUTE_INTEGER},
     {"generation_upload_status", SECRET_SCHEMA_ATTRIBUTE_INTEGER},
     {"form_data", SECRET_SCHEMA_ATTRIBUTE_STRING},
     // This field is always "chrome-profile_id" so that we can search for it.
     {"application", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING}}};

const char* GetStringFromAttributes(GHashTable* attrs, const char* keyname) {
  gpointer value = g_hash_table_lookup(attrs, keyname);
  return value ? static_cast<char*>(value) : "";
}

uint32_t GetUintFromAttributes(GHashTable* attrs, const char* keyname) {
  gpointer value = g_hash_table_lookup(attrs, keyname);
  if (!value)
    return uint32_t();
  uint32_t result;
  bool value_ok = base::StringToUint(static_cast<char*>(value), &result);
  DCHECK(value_ok);
  return result;
}

// Convert the attributes into a new PasswordForm.
// Note: does *not* get the actual password, as that is not a key attribute!
// Returns nullptr if the attributes are for the wrong application.
std::unique_ptr<PasswordForm> FormOutOfAttributes(GHashTable* attrs) {
  base::StringPiece app_value = GetStringFromAttributes(attrs, "application");
  if (!app_value.starts_with(kLibsecretAndGnomeAppString))
    return std::unique_ptr<PasswordForm>();

  std::unique_ptr<PasswordForm> form(new PasswordForm());
  form->origin = GURL(GetStringFromAttributes(attrs, "origin_url"));
  form->action = GURL(GetStringFromAttributes(attrs, "action_url"));
  form->username_element =
      UTF8ToUTF16(GetStringFromAttributes(attrs, "username_element"));
  form->username_value =
      UTF8ToUTF16(GetStringFromAttributes(attrs, "username_value"));
  form->password_element =
      UTF8ToUTF16(GetStringFromAttributes(attrs, "password_element"));
  form->submit_element =
      UTF8ToUTF16(GetStringFromAttributes(attrs, "submit_element"));
  form->signon_realm = GetStringFromAttributes(attrs, "signon_realm");
  form->preferred = GetUintFromAttributes(attrs, "preferred");
  int64_t date_created = 0;
  bool date_ok = base::StringToInt64(
      GetStringFromAttributes(attrs, "date_created"), &date_created);
  DCHECK(date_ok);
  // In the past |date_created| was stored as time_t. Currently is stored as
  // base::Time's internal value. We need to distinguish, which format the
  // number in |date_created| was stored in. We use the fact that
  // kMaxPossibleTimeTValue interpreted as the internal value corresponds to an
  // unlikely date back in 17th century, and anything above
  // kMaxPossibleTimeTValue clearly must be in the internal value format.
  form->date_created = date_created < kMaxPossibleTimeTValue
                           ? base::Time::FromTimeT(date_created)
                           : base::Time::FromInternalValue(date_created);
  form->blacklisted_by_user =
      GetUintFromAttributes(attrs, "blacklisted_by_user");
  form->type =
      static_cast<PasswordForm::Type>(GetUintFromAttributes(attrs, "type"));
  form->times_used = GetUintFromAttributes(attrs, "times_used");
  form->scheme =
      static_cast<PasswordForm::Scheme>(GetUintFromAttributes(attrs, "scheme"));
  int64_t date_synced = 0;
  base::StringToInt64(GetStringFromAttributes(attrs, "date_synced"),
                      &date_synced);
  form->date_synced = base::Time::FromInternalValue(date_synced);
  form->display_name =
      UTF8ToUTF16(GetStringFromAttributes(attrs, "display_name"));
  form->icon_url = GURL(GetStringFromAttributes(attrs, "avatar_url"));
  form->federation_origin = url::Origin::Create(
      GURL(GetStringFromAttributes(attrs, "federation_url")));
  form->skip_zero_click =
      g_hash_table_lookup(attrs, "should_skip_zero_click")
          ? GetUintFromAttributes(attrs, "should_skip_zero_click")
          : true;
  form->generation_upload_status =
      static_cast<PasswordForm::GenerationUploadStatus>(
          GetUintFromAttributes(attrs, "generation_upload_status"));
  base::StringPiece encoded_form_data =
      GetStringFromAttributes(attrs, "form_data");
  if (!encoded_form_data.empty()) {
    bool success = DeserializeFormDataFromBase64String(encoded_form_data,
                                                       &form->form_data);
    password_manager::metrics_util::FormDeserializationStatus status =
        success ? password_manager::metrics_util::GNOME_SUCCESS
                : password_manager::metrics_util::GNOME_FAILURE;
    LogFormDataDeserializationStatus(status);
  }
  return form;
}

}  // namespace

NativeBackendLibsecret::NativeBackendLibsecret(LocalProfileId id)
    : app_string_(GetProfileSpecificAppString(id)),
      ensured_keyring_unlocked_(false) {}

NativeBackendLibsecret::~NativeBackendLibsecret() {
}

bool NativeBackendLibsecret::Init() {
  return LibsecretLoader::EnsureLibsecretLoaded();
}

password_manager::PasswordStoreChangeList NativeBackendLibsecret::AddLogin(
    const PasswordForm& form) {
  // Based on LoginDatabase::AddLogin(), we search for an existing match based
  // on origin_url, username_element, username_value, password_element and
  // signon_realm first, remove that, and then add the new entry.
  password_manager::PasswordStoreChangeList changes;
  std::vector<std::unique_ptr<PasswordForm>> forms;
  if (!AddUpdateLoginSearch(form, &forms))
    return changes;

  if (forms.size() > 0) {
    password_manager::PasswordStoreChangeList temp_changes;
    if (forms.size() > 1) {
      LOG(WARNING) << "Adding login when there are " << forms.size()
                   << " matching logins already!";
    }
    for (const auto& old_form : forms) {
      if (!RemoveLogin(*old_form, &temp_changes))
        return changes;
    }
    changes.push_back(password_manager::PasswordStoreChange(
        password_manager::PasswordStoreChange::REMOVE, *forms[0]));
  }
  if (RawAddLogin(form)) {
    changes.push_back(password_manager::PasswordStoreChange(
        password_manager::PasswordStoreChange::ADD, form));
  }
  return changes;
}

bool NativeBackendLibsecret::UpdateLogin(
    const PasswordForm& form,
    password_manager::PasswordStoreChangeList* changes) {
  // Based on LoginDatabase::UpdateLogin(), we search for forms to update by
  // origin_url, username_element, username_value, password_element, and
  // signon_realm. We then compare the result to the updated form. If they
  // differ in any of the mutable fields, then we remove the original, and
  // then add the new entry. We'd add the new one first, and then delete the
  // original, but then the delete might actually delete the newly-added entry!
  DCHECK(changes);
  std::vector<std::unique_ptr<PasswordForm>> forms;
  if (!AddUpdateLoginSearch(form, &forms))
    return false;
  if (forms.empty())
    return true;
  if (forms.size() == 1 && *forms.front() == form)
    return true;

  password_manager::PasswordStoreChangeList temp_changes;
  for (const auto& keychain_form : forms) {
    // Remove all the obsolete forms. Note that RemoveLogin can remove any form
    // matching the unique key. Thus, it's important to call it the right number
    // of times.
    if (!RemoveLogin(*keychain_form, &temp_changes))
      return false;
  }

  if (RawAddLogin(form)) {
    password_manager::PasswordStoreChange change(
        password_manager::PasswordStoreChange::UPDATE, form);
    changes->push_back(change);
    return true;
  }
  return false;
}

bool NativeBackendLibsecret::RemoveLogin(
    const PasswordForm& form,
    password_manager::PasswordStoreChangeList* changes) {
  DCHECK(changes);
  GError* error = nullptr;
  if (LibsecretLoader::secret_password_clear_sync(
          &kLibsecretSchema, nullptr, &error, "origin_url",
          form.origin.spec().c_str(), "username_element",
          UTF16ToUTF8(form.username_element).c_str(), "username_value",
          UTF16ToUTF8(form.username_value).c_str(), "password_element",
          UTF16ToUTF8(form.password_element).c_str(), "signon_realm",
          form.signon_realm.c_str(), "application", app_string_.c_str(),
          nullptr)) {
    changes->push_back(password_manager::PasswordStoreChange(
        password_manager::PasswordStoreChange::REMOVE, form));
  }

  if (error) {
    LOG(ERROR) << "Libsecret delete failed: " << error->message;
    g_error_free(error);
    return false;
  }
  return true;
}

bool NativeBackendLibsecret::RemoveLoginsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    password_manager::PasswordStoreChangeList* changes) {
  return RemoveLoginsBetween(delete_begin, delete_end, CREATION_TIMESTAMP,
                             changes);
}

bool NativeBackendLibsecret::RemoveLoginsSyncedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    password_manager::PasswordStoreChangeList* changes) {
  return RemoveLoginsBetween(delete_begin, delete_end, SYNC_TIMESTAMP, changes);
}

bool NativeBackendLibsecret::DisableAutoSignInForOrigins(
    const base::Callback<bool(const GURL&)>& origin_filter,
    password_manager::PasswordStoreChangeList* changes) {
  std::vector<std::unique_ptr<PasswordForm>> all_forms;
  if (!GetLoginsList(nullptr, ALL_LOGINS, &all_forms))
    return false;

  for (const std::unique_ptr<PasswordForm>& form : all_forms) {
    if (origin_filter.Run(form->origin) && !form->skip_zero_click) {
      form->skip_zero_click = true;
      if (!UpdateLogin(*form, changes))
        return false;
    }
  }

  return true;
}

bool NativeBackendLibsecret::GetLogins(
    const PasswordStore::FormDigest& form,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  return GetLoginsList(&form, ALL_LOGINS, forms);
}

bool NativeBackendLibsecret::AddUpdateLoginSearch(
    const PasswordForm& lookup_form,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  if (!ensured_keyring_unlocked_) {
    LibsecretLoader::EnsureKeyringUnlocked();
    ensured_keyring_unlocked_ = true;
  }

  LibsecretAttributesBuilder attrs;
  attrs.Append("origin_url", lookup_form.origin.spec());
  attrs.Append("username_element", UTF16ToUTF8(lookup_form.username_element));
  attrs.Append("username_value", UTF16ToUTF8(lookup_form.username_value));
  attrs.Append("password_element", UTF16ToUTF8(lookup_form.password_element));
  attrs.Append("signon_realm", lookup_form.signon_realm);
  attrs.Append("application", app_string_);

  LibsecretLoader::SearchHelper helper;
  helper.Search(&kLibsecretSchema, attrs.Get(),
                SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK);
  if (!helper.success())
    return false;

  PasswordStore::FormDigest form(lookup_form);
  *forms = ConvertFormList(helper.results(), &form);
  return true;
}

bool NativeBackendLibsecret::RawAddLogin(const PasswordForm& form) {
  int64_t date_created = form.date_created.ToInternalValue();
  // If we are asked to save a password with 0 date, use the current time.
  // We don't want to actually save passwords as though on January 1, 1601.
  if (!date_created)
    date_created = base::Time::Now().ToInternalValue();
  int64_t date_synced = form.date_synced.ToInternalValue();
  std::string form_data;
  SerializeFormDataToBase64String(form.form_data, &form_data);
  GError* error = nullptr;
  // clang-format off
  LibsecretLoader::secret_password_store_sync(
      &kLibsecretSchema,
      nullptr,                     // Default collection.
      form.origin.spec().c_str(),  // Display name.
      UTF16ToUTF8(form.password_value).c_str(),
      nullptr,  // no cancellable ojbect
      &error,
      "origin_url", form.origin.spec().c_str(),
      "action_url", form.action.spec().c_str(),
      "username_element", UTF16ToUTF8(form.username_element).c_str(),
      "username_value", UTF16ToUTF8(form.username_value).c_str(),
      "password_element", UTF16ToUTF8(form.password_element).c_str(),
      "submit_element", UTF16ToUTF8(form.submit_element).c_str(),
      "signon_realm", form.signon_realm.c_str(),
      "preferred", form.preferred,
      "date_created", base::Int64ToString(date_created).c_str(),
      "blacklisted_by_user", form.blacklisted_by_user,
      "type", form.type,
      "times_used", form.times_used,
      "scheme", form.scheme,
      "date_synced", base::Int64ToString(date_synced).c_str(),
      "display_name", UTF16ToUTF8(form.display_name).c_str(),
      "avatar_url", form.icon_url.spec().c_str(),
      // We serialize unique origins as "", in order to make other systems that
      // read from the login database happy. https://crbug.com/591310
      "federation_url", form.federation_origin.opaque()
          ? ""
          : form.federation_origin.Serialize().c_str(),
      "should_skip_zero_click", form.skip_zero_click,
      "generation_upload_status", form.generation_upload_status,
      "form_data", form_data.c_str(),
      "application", app_string_.c_str(),
      nullptr);
  // clang-format on

  if (error) {
    LOG(ERROR) << "Libsecret add raw login failed: " << error->message;
    g_error_free(error);
    return false;
  }
  return true;
}

bool NativeBackendLibsecret::GetAutofillableLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  return GetLoginsList(nullptr, AUTOFILLABLE_LOGINS, forms);
}

bool NativeBackendLibsecret::GetBlacklistLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  return GetLoginsList(nullptr, BLACKLISTED_LOGINS, forms);
}

bool NativeBackendLibsecret::GetAllLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  return GetLoginsList(nullptr, ALL_LOGINS, forms);
}

scoped_refptr<base::SequencedTaskRunner>
NativeBackendLibsecret::GetBackgroundTaskRunner() {
  return nullptr;
}

bool NativeBackendLibsecret::GetLoginsList(
    const PasswordStore::FormDigest* lookup_form,
    GetLoginsListOptions options,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  if (!ensured_keyring_unlocked_) {
    LibsecretLoader::EnsureKeyringUnlocked();
    ensured_keyring_unlocked_ = true;
  }

  LibsecretAttributesBuilder attrs;
  attrs.Append("application", app_string_);
  if (options != ALL_LOGINS)
    attrs.Append("blacklisted_by_user", options == BLACKLISTED_LOGINS);
  if (lookup_form &&
      !password_manager::ShouldPSLDomainMatchingApply(
          password_manager::GetRegistryControlledDomain(
              GURL(lookup_form->signon_realm))) &&
      lookup_form->scheme != PasswordForm::SCHEME_HTML)
    attrs.Append("signon_realm", lookup_form->signon_realm);

  LibsecretLoader::SearchHelper helper;
  helper.Search(&kLibsecretSchema, attrs.Get(),
                SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK);
  if (!helper.success())
    return false;

  *forms = ConvertFormList(helper.results(), lookup_form);
  if (lookup_form)
    return true;

  // Get rid of the forms with the same sync tags.
  std::vector<std::unique_ptr<PasswordForm>> duplicates;
  std::vector<std::vector<PasswordForm*>> tag_groups;
  password_manager_util::FindDuplicates(forms, &duplicates, &tag_groups);
  if (duplicates.empty())
    return true;
  for (const auto& group : tag_groups) {
    if (group.size() > 1) {
      // There are duplicates. Readd the first form. AddLogin() is smart enough
      // to clean the previous ones.
      password_manager::PasswordStoreChangeList changes = AddLogin(*group[0]);
      if (changes.empty() ||
          changes.back().type() != password_manager::PasswordStoreChange::ADD)
        return false;
    }
  }
  return true;
}

bool NativeBackendLibsecret::GetLoginsBetween(
    base::Time get_begin,
    base::Time get_end,
    TimestampToCompare date_to_compare,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  forms->clear();
  std::vector<std::unique_ptr<PasswordForm>> all_forms;
  if (!GetLoginsList(nullptr, ALL_LOGINS, &all_forms))
    return false;

  base::Time PasswordForm::*date_member = date_to_compare == CREATION_TIMESTAMP
                                              ? &PasswordForm::date_created
                                              : &PasswordForm::date_synced;
  for (std::unique_ptr<PasswordForm>& saved_form : all_forms) {
    if (get_begin <= saved_form.get()->*date_member &&
        (get_end.is_null() || saved_form.get()->*date_member < get_end)) {
      forms->push_back(std::move(saved_form));
    }
  }

  return true;
}

bool NativeBackendLibsecret::RemoveLoginsBetween(
    base::Time get_begin,
    base::Time get_end,
    TimestampToCompare date_to_compare,
    password_manager::PasswordStoreChangeList* changes) {
  DCHECK(changes);
  changes->clear();
  std::vector<std::unique_ptr<PasswordForm>> forms;
  if (!GetLoginsBetween(get_begin, get_end, date_to_compare, &forms))
    return false;

  for (size_t i = 0; i < forms.size(); ++i) {
    if (!RemoveLogin(*forms[i], changes))
      return false;
  }
  return true;
}

std::vector<std::unique_ptr<PasswordForm>>
NativeBackendLibsecret::ConvertFormList(
    GList* found,
    const PasswordStore::FormDigest* lookup_form) {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  password_manager::PSLDomainMatchMetric psl_domain_match_metric =
      password_manager::PSL_DOMAIN_MATCH_NONE;
  GError* error = nullptr;
  for (GList* element = g_list_first(found); element != nullptr;
       element = g_list_next(element)) {
    SecretItem* secretItem = static_cast<SecretItem*>(element->data);
    GHashTable* attrs = LibsecretLoader::secret_item_get_attributes(secretItem);
    std::unique_ptr<PasswordForm> form(FormOutOfAttributes(attrs));
    g_hash_table_unref(attrs);
    if (!form) {
      VLOG(1) << "Could not initialize PasswordForm from attributes!";
      continue;
    }

    if (lookup_form) {
      switch (GetMatchResult(*form, *lookup_form)) {
        case MatchResult::NO_MATCH:
          continue;
        case MatchResult::EXACT_MATCH:
          break;
        case MatchResult::PSL_MATCH:
          psl_domain_match_metric = password_manager::PSL_DOMAIN_MATCH_FOUND;
          form->is_public_suffix_match = true;
          break;
        case MatchResult::FEDERATED_MATCH:
          break;
        case MatchResult::FEDERATED_PSL_MATCH:
          psl_domain_match_metric =
              password_manager::PSL_DOMAIN_MATCH_FOUND_FEDERATED;
          form->is_public_suffix_match = true;
          break;
      }
    }

    LibsecretLoader::secret_item_load_secret_sync(secretItem, nullptr, &error);
    if (error) {
      LOG(ERROR) << "Unable to load secret item" << error->message;
      g_error_free(error);
      error = nullptr;
      continue;
    }

    SecretValue* secretValue =
        LibsecretLoader::secret_item_get_secret(secretItem);
    if (secretValue) {
      form->password_value =
          UTF8ToUTF16(LibsecretLoader::secret_value_get_text(secretValue));
      LibsecretLoader::secret_value_unref(secretValue);
    } else {
      LOG(WARNING) << "Unable to access password from list element!";
    }
    forms.push_back(std::move(form));
  }

  if (lookup_form) {
    const bool allow_psl_match = password_manager::ShouldPSLDomainMatchingApply(
        password_manager::GetRegistryControlledDomain(
            GURL(lookup_form->signon_realm)));
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.PslDomainMatchTriggering",
                              allow_psl_match
                                  ? psl_domain_match_metric
                                  : password_manager::PSL_DOMAIN_MATCH_NOT_USED,
                              password_manager::PSL_DOMAIN_MATCH_COUNT);
  }
  return forms;
}
