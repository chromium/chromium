// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/native_backend_kwallet_x.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/dbus/dbus_thread_linux.h"
#include "chrome/grit/chromium_strings.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

using autofill::PasswordForm;

namespace {

// In case the fields in the pickle ever change, version them so we can try to
// read old pickles. (Note: do not eat old pickles past the expiration date.)
const int kPickleVersion = 9;

// We could localize this string, but then changing your locale would cause
// you to lose access to all your stored passwords. Maybe best not to do that.
// Name of the folder to store passwords in.
const char kKWalletFolder[] = "Chrome Form Data";

// Checks a serialized list of PasswordForms for sanity. Returns true if OK.
// Note that |realm| is only used for generating a useful warning message.
bool CheckSerializedValue(const uint8_t* byte_array,
                          size_t length,
                          const std::string& realm) {
  const base::Pickle::Header* header =
      reinterpret_cast<const base::Pickle::Header*>(byte_array);
  if (length < sizeof(*header) ||
      header->payload_size > length - sizeof(*header)) {
    LOG(WARNING) << "Invalid KWallet entry detected (realm: " << realm << ")";
    return false;
  }
  return true;
}

// Convenience function to read a GURL from a Pickle. Assumes the URL has
// been written as a UTF-8 string. Returns true on success.
bool ReadGURL(base::PickleIterator* iter, bool warn_only, GURL* url) {
  std::string url_string;
  if (!iter->ReadString(&url_string)) {
    if (!warn_only)
      LOG(ERROR) << "Failed to deserialize URL.";
    *url = GURL();
    return false;
  }
  *url = GURL(url_string);
  return true;
}

// Convenience function to read a url::Origin from a Pickle. Assumes the origin
// has been written as a UTF-8 string. Returns true on success.
bool ReadOrigin(base::PickleIterator* iter,
                bool warn_only,
                url::Origin* origin) {
  std::string origin_string;
  if (!iter->ReadString(&origin_string)) {
    if (!warn_only)
      LOG(ERROR) << "Failed to deserialize Origin.";
    *origin = url::Origin();
    return false;
  }
  *origin = url::Origin::Create(GURL(origin_string));
  return true;
}

void LogDeserializationWarning(int version,
                               std::string signon_realm,
                               bool warn_only) {
  if (warn_only) {
    LOG(WARNING) << "Failed to deserialize version " << version
                 << " KWallet entry (realm: " << signon_realm
                 << ") with native architecture size; will try alternate "
                 << "size.";
  } else {
    LOG(ERROR) << "Failed to deserialize version " << version
               << " KWallet entry (realm: " << signon_realm << ")";
  }
}

// Deserializes a list of credentials from the wallet to |forms| (replacing
// the contents of |forms|). |size_32| controls reading the size field within
// the pickle as 32 bits. We used to use Pickle::WriteSize() to write the number
// of password forms, but that has a different size on 32- and 64-bit systems.
// So, now we always write a 64-bit quantity, but we support trying to read it
// as either size when reading old pickles that fail to deserialize using the
// native size. Returns true on success.
bool DeserializeValueSize(const std::string& signon_realm,
                          const base::PickleIterator& init_iter,
                          int version,
                          bool size_32,
                          bool warn_only,
                          std::vector<std::unique_ptr<PasswordForm>>* forms) {
  base::PickleIterator iter = init_iter;

  size_t count = 0;
  if (size_32) {
    uint32_t count_32 = 0;
    if (!iter.ReadUInt32(&count_32)) {
      LOG(ERROR) << "Failed to deserialize KWallet entry "
                 << "(realm: " << signon_realm << ")";
      return false;
    }
    count = count_32;
  } else {
    uint64_t count_64 = 0;
    if (!iter.ReadUInt64(&count_64)) {
      LOG(ERROR) << "Failed to deserialize KWallet entry "
                 << "(realm: " << signon_realm << ")";
      return false;
    }
    count = static_cast<size_t>(count_64);
  }

  if (count > 0xFFFF) {
    // Trying to pin down the cause of http://crbug.com/80728 (or fix it).
    // This is a very large number of passwords to be saved for a single realm.
    // It is almost certainly a corrupt pickle and not real data. Ignore it.
    // This very well might actually be http://crbug.com/107701, so if we're
    // reading an old pickle, we don't even log this the first time we try to
    // read it. (That is, when we're reading the native architecture size.)
    if (!warn_only) {
      LOG(ERROR) << "Suspiciously large number of entries in KWallet entry "
                 << "(" << count << "; realm: " << signon_realm << ")";
    }
    return false;
  }

  // We'll swap |converted_forms| with |*forms| on success, to make sure we
  // don't return partial results on failure.
  std::vector<std::unique_ptr<PasswordForm>> converted_forms;
  converted_forms.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    std::unique_ptr<PasswordForm> form(new PasswordForm());
    form->signon_realm.assign(signon_realm);

    int scheme = 0;
    int64_t date_created = 0;
    int type = 0;
    int generation_upload_status = 0;
    // Note that these will be read back in the order listed due to
    // short-circuit evaluation. This is important.
    if (!iter.ReadInt(&scheme) || !ReadGURL(&iter, warn_only, &form->origin) ||
        !ReadGURL(&iter, warn_only, &form->action) ||
        !iter.ReadString16(&form->username_element) ||
        !iter.ReadString16(&form->username_value) ||
        !iter.ReadString16(&form->password_element) ||
        !iter.ReadString16(&form->password_value) ||
        !iter.ReadString16(&form->submit_element)) {
      LogDeserializationWarning(version, signon_realm, warn_only);
      return false;
    }
    if (version <= 8) {
      bool dummy_unused_flag = false;
      if (!iter.ReadBool(&dummy_unused_flag)) {
        LogDeserializationWarning(version, signon_realm, warn_only);
        return false;
      }
    }
    if (!iter.ReadBool(&form->preferred) ||
        !iter.ReadBool(&form->blacklisted_by_user) ||
        !iter.ReadInt64(&date_created)) {
      LogDeserializationWarning(version, signon_realm, warn_only);
      return false;
    }
    form->scheme = static_cast<PasswordForm::Scheme>(scheme);

    if (version > 1) {
      if (!iter.ReadInt(&type) ||
          !iter.ReadInt(&form->times_used) ||
          !autofill::DeserializeFormData(&iter, &form->form_data)) {
        LogDeserializationWarning(version, signon_realm, false);
        return false;
      }
      form->type = static_cast<PasswordForm::Type>(type);
    }

    if (version > 2) {
      int64_t date_synced = 0;
      if (!iter.ReadInt64(&date_synced)) {
        LogDeserializationWarning(version, signon_realm, false);
        return false;
      }
      form->date_synced = base::Time::FromInternalValue(date_synced);
    }

    if (version > 3) {
      if (!iter.ReadString16(&form->display_name) ||
          !ReadGURL(&iter, warn_only, &form->icon_url) ||
          !ReadOrigin(&iter, warn_only, &form->federation_origin) ||
          !iter.ReadBool(&form->skip_zero_click)) {
        LogDeserializationWarning(version, signon_realm, false);
        return false;
      }
      if (version <= 7)
        form->skip_zero_click = true;
    }

    if (version > 4) {
      form->date_created = base::Time::FromInternalValue(date_created);
    } else {
      form->date_created = base::Time::FromTimeT(date_created);
    }

    if (version > 5) {
      bool read_success = iter.ReadInt(&generation_upload_status);
      if (!read_success && version > 6) {
        // Valid version 6 pickles might still lack the
        // generation_upload_status, see http://crbug.com/494229#c11.
        LogDeserializationWarning(version, signon_realm, false);
        return false;
      }
      if (read_success) {
        form->generation_upload_status =
            static_cast<PasswordForm::GenerationUploadStatus>(
                generation_upload_status);
      }
    }

    converted_forms.push_back(std::move(form));
  }

  forms->swap(converted_forms);
  return true;
}

// Serializes a list of PasswordForms to be stored in the wallet.
void SerializeValue(const std::vector<std::unique_ptr<PasswordForm>>& forms,
                    base::Pickle* pickle) {
  pickle->WriteInt(kPickleVersion);
  pickle->WriteUInt64(forms.size());
  for (const auto& form : forms) {
    pickle->WriteInt(form->scheme);
    pickle->WriteString(form->origin.spec());
    pickle->WriteString(form->action.spec());
    pickle->WriteString16(form->username_element);
    pickle->WriteString16(form->username_value);
    pickle->WriteString16(form->password_element);
    pickle->WriteString16(form->password_value);
    pickle->WriteString16(form->submit_element);
    pickle->WriteBool(form->preferred);
    pickle->WriteBool(form->blacklisted_by_user);
    pickle->WriteInt64(form->date_created.ToInternalValue());
    pickle->WriteInt(form->type);
    pickle->WriteInt(form->times_used);
    autofill::SerializeFormData(form->form_data, pickle);
    pickle->WriteInt64(form->date_synced.ToInternalValue());
    pickle->WriteString16(form->display_name);
    pickle->WriteString(form->icon_url.spec());
    // We serialize unique origins as "", in order to make other systems that
    // read from the login database happy. https://crbug.com/591310
    pickle->WriteString(form->federation_origin.opaque()
                            ? std::string()
                            : form->federation_origin.Serialize());
    pickle->WriteBool(form->skip_zero_click);
    pickle->WriteInt(form->generation_upload_status);
  }
}

void UMALogDeserializationStatus(bool success) {
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.KWalletDeserializationStatus",
                        success);
}

}  // namespace

// Using USER_VISIBLE priority, because the passwords obtained through tasks on
// the background runner influence what the user sees.
NativeBackendKWallet::NativeBackendKWallet(
    LocalProfileId id,
    base::nix::DesktopEnvironment desktop_env)
    : profile_id_(id),
      kwallet_dbus_(desktop_env),
      app_name_(l10n_util::GetStringUTF8(IDS_PRODUCT_NAME)) {
  folder_name_ = GetProfileSpecificFolderName();
}

NativeBackendKWallet::~NativeBackendKWallet() {
  // This destructor is called on the thread that is destroying the Profile
  // containing the PasswordStore that owns this NativeBackend. Generally that
  // won't be run by the background task runner; it will be on the main one. So
  // we post a message to shut it down on the background task runner, and it
  // will be destroyed afterward when the scoped_refptr<dbus::Bus> goes out of
  // scope. The NativeBackend will be destroyed before that occurs, but that's
  // OK.
  if (kwallet_dbus_.GetSessionBus()) {
    chrome::GetDBusTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock,
                                  kwallet_dbus_.GetSessionBus()));
  }
}

bool NativeBackendKWallet::Init() {
  // Without the |optional_bus| parameter, a real bus will be instantiated.
  return InitWithBus(scoped_refptr<dbus::Bus>());
}

bool NativeBackendKWallet::InitWithBus(scoped_refptr<dbus::Bus> optional_bus) {
  // We must synchronously do a few DBus calls to figure out if initialization
  // succeeds, but later, we'll want to do most of the work on the background
  // task runner. So we have to do the initialization on the background task
  // runner here too, and wait for it.
  bool success = false;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  // NativeBackendKWallet isn't reference counted, but we wait for InitWithBus
  // to finish, so we can safely use base::Unretained here.
  chrome::GetDBusTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeBackendKWallet::InitOnBackgroundTaskRunner,
                     base::Unretained(this), optional_bus, &event, &success));

  // This ScopedAllowWait should not be here. However, the whole backend is so
  // close to deprecation that it does not make sense to refactor it. More info
  // on https://crbug.com/739897.
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  event.Wait();
  return success;
}

void NativeBackendKWallet::InitOnBackgroundTaskRunner(
    scoped_refptr<dbus::Bus> optional_bus,
    base::WaitableEvent* event,
    bool* success) {
  DCHECK(chrome::GetDBusTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!kwallet_dbus_.GetSessionBus());
  if (optional_bus.get()) {
    // The optional_bus parameter is given when this method is called in tests.
    kwallet_dbus_.SetSessionBus(optional_bus);
  } else {
    // Get a (real) connection to the session bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    options.connection_type = dbus::Bus::PRIVATE;
    kwallet_dbus_.SetSessionBus(new dbus::Bus(options));
  }
  // kwalletd may not be running. If we get a temporary failure initializing it,
  // try to start it and then try again. (Note the short-circuit evaluation.)
  const InitResult result = InitWallet();
  *success = (result == INIT_SUCCESS ||
              (result == TEMPORARY_FAIL && kwallet_dbus_.StartKWalletd() &&
               InitWallet() == INIT_SUCCESS));
  event->Signal();
}

NativeBackendKWallet::InitResult NativeBackendKWallet::InitWallet() {
  DCHECK(chrome::GetDBusTaskRunner()->RunsTasksInCurrentSequence());

  // Check that KWallet is enabled.
  bool enabled = false;
  KWalletDBus::Error error = kwallet_dbus_.IsEnabled(&enabled);
  switch (error) {
    case KWalletDBus::Error::CANNOT_CONTACT:
      return TEMPORARY_FAIL;
    case KWalletDBus::Error::CANNOT_READ:
      return PERMANENT_FAIL;
    case KWalletDBus::Error::SUCCESS:
      break;
  }
  if (!enabled)
    return PERMANENT_FAIL;

  // Get the wallet name.
  error = kwallet_dbus_.NetworkWallet(&wallet_name_);
  switch (error) {
    case KWalletDBus::Error::CANNOT_CONTACT:
      return TEMPORARY_FAIL;
    case KWalletDBus::Error::CANNOT_READ:
      return PERMANENT_FAIL;
    case KWalletDBus::Error::SUCCESS:
      return INIT_SUCCESS;
  }

  NOTREACHED();
  return PERMANENT_FAIL;
}

password_manager::PasswordStoreChangeList NativeBackendKWallet::AddLogin(
    const PasswordForm& form) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return password_manager::PasswordStoreChangeList();

  std::vector<std::unique_ptr<PasswordForm>> forms;
  if (!GetLoginsList(form.signon_realm, wallet_handle, &forms))
    return password_manager::PasswordStoreChangeList();

  auto it = std::partition(
      forms.begin(), forms.end(),
      [&form](const std::unique_ptr<PasswordForm>& current_form) {
        return !ArePasswordFormUniqueKeyEqual(form, *current_form);
      });
  password_manager::PasswordStoreChangeList changes;
  if (it != forms.end()) {
    // It's an update.
    changes.push_back(password_manager::PasswordStoreChange(
        password_manager::PasswordStoreChange::REMOVE, **it));
    forms.erase(it, forms.end());
  }

  forms.push_back(std::make_unique<PasswordForm>(form));
  changes.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD, form));

  bool ok = SetLoginsList(forms, form.signon_realm, wallet_handle);
  if (!ok)
    changes.clear();

  return changes;
}

bool NativeBackendKWallet::UpdateLogin(
    const PasswordForm& form,
    password_manager::PasswordStoreChangeList* changes) {
  DCHECK(changes);
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;

  std::vector<std::unique_ptr<PasswordForm>> forms;
  if (!GetLoginsList(form.signon_realm, wallet_handle, &forms))
    return false;

  auto it = std::partition(
      forms.begin(), forms.end(),
      [&form](const std::unique_ptr<PasswordForm>& current_form) {
        return !ArePasswordFormUniqueKeyEqual(form, *current_form);
      });

  if (it == forms.end())
    return true;

  forms.erase(it, forms.end());
  forms.push_back(std::make_unique<PasswordForm>(form));
  if (SetLoginsList(forms, form.signon_realm, wallet_handle)) {
    changes->push_back(password_manager::PasswordStoreChange(
        password_manager::PasswordStoreChange::UPDATE, form));
    return true;
  }

  return false;
}

bool NativeBackendKWallet::RemoveLogin(
    const PasswordForm& form,
    password_manager::PasswordStoreChangeList* changes) {
  DCHECK(changes);
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;

  std::vector<std::unique_ptr<PasswordForm>> all_forms;
  if (!GetLoginsList(form.signon_realm, wallet_handle, &all_forms))
    return false;

  std::vector<std::unique_ptr<PasswordForm>> kept_forms;
  kept_forms.reserve(all_forms.size());
  for (std::unique_ptr<PasswordForm>& saved_form : all_forms) {
    if (!ArePasswordFormUniqueKeyEqual(form, *saved_form)) {
      kept_forms.push_back(std::move(saved_form));
    }
  }

  if (kept_forms.size() != all_forms.size()) {
    changes->push_back(password_manager::PasswordStoreChange(
        password_manager::PasswordStoreChange::REMOVE, form));
    return SetLoginsList(kept_forms, form.signon_realm, wallet_handle);
  }

  return true;
}

bool NativeBackendKWallet::RemoveLoginsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    password_manager::PasswordStoreChangeList* changes) {
  return RemoveLoginsBetween(
      delete_begin, delete_end, CREATION_TIMESTAMP, changes);
}

bool NativeBackendKWallet::RemoveLoginsSyncedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    password_manager::PasswordStoreChangeList* changes) {
  return RemoveLoginsBetween(delete_begin, delete_end, SYNC_TIMESTAMP, changes);
}

bool NativeBackendKWallet::DisableAutoSignInForOrigins(
    const base::Callback<bool(const GURL&)>& origin_filter,
    password_manager::PasswordStoreChangeList* changes) {
  std::vector<std::unique_ptr<PasswordForm>> all_forms;
  if (!GetAllLogins(&all_forms))
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

bool NativeBackendKWallet::GetLogins(
    const password_manager::PasswordStore::FormDigest& form,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetLoginsList(form.signon_realm, wallet_handle, forms);
}

bool NativeBackendKWallet::GetAutofillableLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetLoginsList(BlacklistOptions::AUTOFILLABLE, wallet_handle, forms);
}

bool NativeBackendKWallet::GetBlacklistLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetLoginsList(BlacklistOptions::BLACKLISTED, wallet_handle, forms);
}

bool NativeBackendKWallet::GetAllLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetAllLoginsInternal(wallet_handle, forms);
}

scoped_refptr<base::SequencedTaskRunner>
NativeBackendKWallet::GetBackgroundTaskRunner() {
  return chrome::GetDBusTaskRunner();
}

bool NativeBackendKWallet::GetLoginsList(
    const std::string& signon_realm,
    int wallet_handle,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  forms->clear();
  // Is there an entry in the wallet?
  bool has_entry = false;
  KWalletDBus::Error error = kwallet_dbus_.HasEntry(
      wallet_handle, folder_name_, signon_realm, app_name_, &has_entry);
  if (error)
    return false;
  if (!has_entry)
    return true;

  std::vector<uint8_t> bytes;
  error = kwallet_dbus_.ReadEntry(wallet_handle, folder_name_, signon_realm,
                                  app_name_, &bytes);
  if (error)
    return false;
  if (!bytes.empty() &&
      !CheckSerializedValue(bytes.data(), bytes.size(), signon_realm)) {
    // This is weird, but we choose not to call it an error. There is an
    // invalid entry somehow, but by just ignoring it, we make it easier to
    // repair without having to delete it using kwalletmanager (that is, by
    // just saving a new password within this realm to overwrite it).
    return true;
  }

  // Can't we all just agree on whether bytes are signed or not? Please?
  base::Pickle pickle(reinterpret_cast<const char*>(bytes.data()),
                      bytes.size());
  *forms = DeserializeValue(signon_realm, pickle);

  return true;
}

bool NativeBackendKWallet::GetLoginsList(
    BlacklistOptions options,
    int wallet_handle,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  forms->clear();
  std::vector<std::unique_ptr<PasswordForm>> all_forms;
  if (!GetAllLoginsInternal(wallet_handle, &all_forms))
    return false;

  // Remove the duplicate sync tags.
  std::vector<std::unique_ptr<PasswordForm>> duplicates;
  password_manager_util::FindDuplicates(&all_forms, &duplicates, nullptr);
  if (!duplicates.empty()) {
    // Fill the signon realms to be updated.
    std::map<std::string, std::vector<std::unique_ptr<PasswordForm>>>
        update_forms;
    for (const auto& form : duplicates) {
      update_forms.insert(std::make_pair(
          form->signon_realm, std::vector<std::unique_ptr<PasswordForm>>()));
    }

    // Fill the actual forms to be saved.
    for (const auto& form : all_forms) {
      auto it = update_forms.find(form->signon_realm);
      if (it != update_forms.end())
        it->second.push_back(std::make_unique<PasswordForm>(*form));
    }

    // Update the backend.
    for (const auto& update_forms_for_realm : update_forms) {
      if (!SetLoginsList(update_forms_for_realm.second,
                         update_forms_for_realm.first, wallet_handle)) {
        return false;
      }
    }
  }
  // We have to read all the entries, and then filter them here.
  forms->reserve(all_forms.size());
  for (std::unique_ptr<PasswordForm>& saved_form : all_forms) {
    if (saved_form->blacklisted_by_user ==
        (options == BlacklistOptions::BLACKLISTED)) {
      forms->push_back(std::move(saved_form));
    }
  }

  return true;
}

bool NativeBackendKWallet::GetAllLoginsInternal(
    int wallet_handle,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  // We could probably also use readEntryList here.
  std::vector<std::string> realm_list;
  KWalletDBus::Error error = kwallet_dbus_.EntryList(
      wallet_handle, folder_name_, app_name_, &realm_list);
  if (error)
    return false;

  forms->clear();
  for (const std::string& signon_realm : realm_list) {
    std::vector<uint8_t> bytes;
    KWalletDBus::Error error = kwallet_dbus_.ReadEntry(
        wallet_handle, folder_name_, signon_realm, app_name_, &bytes);
    if (error)
      return false;
    if (bytes.empty() ||
        !CheckSerializedValue(bytes.data(), bytes.size(), signon_realm))
      continue;

    // Can't we all just agree on whether bytes are signed or not? Please?
    base::Pickle pickle(reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
    std::vector<std::unique_ptr<PasswordForm>> from_pickle =
        DeserializeValue(signon_realm, pickle);
    forms->reserve(forms->size() + from_pickle.size());
    std::move(from_pickle.begin(), from_pickle.end(),
              std::back_inserter(*forms));
  }
  return true;
}

bool NativeBackendKWallet::SetLoginsList(
    const std::vector<std::unique_ptr<PasswordForm>>& forms,
    const std::string& signon_realm,
    int wallet_handle) {
  if (forms.empty()) {
    int ret = 0;
    KWalletDBus::Error error = kwallet_dbus_.RemoveEntry(
        wallet_handle, folder_name_, signon_realm, app_name_, &ret);
    if (error)
      return false;
    if (ret != 0)
      LOG(ERROR) << "Bad return code " << ret << " from KWallet removeEntry";
    return ret == 0;
  }

  base::Pickle value;
  SerializeValue(forms, &value);

  int ret = 0;
  KWalletDBus::Error error = kwallet_dbus_.WriteEntry(
      wallet_handle, folder_name_, signon_realm, app_name_,
      static_cast<const uint8_t*>(value.data()), value.size(), &ret);
  if (error)
    return false;
  if (ret != 0)
    LOG(ERROR) << "Bad return code " << ret << " from KWallet writeEntry";
  return ret == 0;
}

bool NativeBackendKWallet::RemoveLoginsBetween(
    base::Time delete_begin,
    base::Time delete_end,
    TimestampToCompare date_to_compare,
    password_manager::PasswordStoreChangeList* changes) {
  DCHECK(changes);
  changes->clear();
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;

  // We could probably also use readEntryList here.
  std::vector<std::string> realm_list;
  KWalletDBus::Error error = kwallet_dbus_.EntryList(
      wallet_handle, folder_name_, app_name_, &realm_list);
  if (error)
    return false;

  bool ok = true;
  for (size_t i = 0; i < realm_list.size(); ++i) {
    const std::string& signon_realm = realm_list[i];

    std::vector<uint8_t> bytes;
    KWalletDBus::Error error = kwallet_dbus_.ReadEntry(
        wallet_handle, folder_name_, signon_realm, app_name_, &bytes);
    if (error)
      continue;
    if (bytes.size() == 0 ||
        !CheckSerializedValue(bytes.data(), bytes.size(), signon_realm))
      continue;

    // Can't we all just agree on whether bytes are signed or not? Please?
    base::Pickle pickle(reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
    std::vector<std::unique_ptr<PasswordForm>> all_forms =
        DeserializeValue(signon_realm, pickle);

    std::vector<std::unique_ptr<PasswordForm>> kept_forms;
    kept_forms.reserve(all_forms.size());
    base::Time PasswordForm::*date_member =
        date_to_compare == CREATION_TIMESTAMP ? &PasswordForm::date_created
                                              : &PasswordForm::date_synced;
    for (std::unique_ptr<PasswordForm>& saved_form : all_forms) {
      if (delete_begin <= saved_form.get()->*date_member &&
          (delete_end.is_null() ||
           saved_form.get()->*date_member < delete_end)) {
        changes->push_back(password_manager::PasswordStoreChange(
            password_manager::PasswordStoreChange::REMOVE, *saved_form));
      } else {
        kept_forms.push_back(std::move(saved_form));
      }
    }

    if (!SetLoginsList(kept_forms, signon_realm, wallet_handle)) {
      ok = false;
      changes->clear();
    }
  }
  return ok;
}

// static
std::vector<std::unique_ptr<PasswordForm>>
NativeBackendKWallet::DeserializeValue(const std::string& signon_realm,
                                       const base::Pickle& pickle) {
  base::PickleIterator iter(pickle);

  int version = -1;
  if (!iter.ReadInt(&version) ||
      version < 0 || version > kPickleVersion) {
    LOG(ERROR) << "Failed to deserialize KWallet entry "
               << "(realm: " << signon_realm << ")";
    return std::vector<std::unique_ptr<PasswordForm>>();
  }

  std::vector<std::unique_ptr<PasswordForm>> forms;
  bool success = true;
  if (version > 0) {
    // In current pickles, we expect 64-bit sizes. Failure is an error.
    success = DeserializeValueSize(
        signon_realm, iter, version, false, false, &forms);
    UMALogDeserializationStatus(success);
    return forms;
  }

  const bool size_32 = sizeof(size_t) == sizeof(uint32_t);
  if (!DeserializeValueSize(
          signon_realm, iter, version, size_32, true, &forms)) {
    // We failed to read the pickle using the native architecture of the system.
    // Try again with the opposite architecture. Note that we do this even on
    // 32-bit machines, in case we're reading a 64-bit pickle. (Probably rare,
    // since mostly we expect upgrades, not downgrades, but both are possible.)
    success = DeserializeValueSize(
        signon_realm, iter, version, !size_32, false, &forms);
  }
  UMALogDeserializationStatus(success);
  return forms;
}

int NativeBackendKWallet::WalletHandle() {
  DCHECK(chrome::GetDBusTaskRunner()->RunsTasksInCurrentSequence());

  // Open the wallet.
  // TODO(mdm): Are we leaking these handles? Find out.
  int32_t handle = kInvalidKWalletHandle;
  KWalletDBus::Error error =
      kwallet_dbus_.Open(wallet_name_, app_name_, &handle);
  if (error)
    return kInvalidKWalletHandle;
  if (handle == kInvalidKWalletHandle) {
    LOG(ERROR) << "Error obtaining KWallet handle";
    return kInvalidKWalletHandle;
  }

  // Check if our folder exists.
  bool has_folder = false;
  error = kwallet_dbus_.HasFolder(handle, folder_name_, app_name_, &has_folder);
  if (error)
    return kInvalidKWalletHandle;

  // Create it if it didn't.
  if (!has_folder) {
    bool success = false;
    error =
        kwallet_dbus_.CreateFolder(handle, folder_name_, app_name_, &success);
    if (error)
      return kInvalidKWalletHandle;
    if (!success) {
      LOG(ERROR) << "Error creating KWallet folder";
      return kInvalidKWalletHandle;
    }
  }

  return handle;
}

std::string NativeBackendKWallet::GetProfileSpecificFolderName() const {
  // Originally, the folder name was always just "Chrome Form Data".
  // Now we use it to distinguish passwords for different profiles.
  return base::StringPrintf("%s (%d)", kKWalletFolder, profile_id_);
}
