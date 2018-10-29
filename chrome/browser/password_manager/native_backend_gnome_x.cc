// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/native_backend_gnome_x.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_manager_util_linux.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using autofill::PasswordForm;
using base::UTF8ToUTF16;
using base::UTF16ToUTF8;
using password_manager::MatchResult;
using password_manager::PasswordStore;

namespace {

// Convert the attributes of a given keyring entry into a new PasswordForm.
// Note: does *not* get the actual password, as that is not a key attribute!
// Returns NULL if the attributes are for the wrong application.
std::unique_ptr<PasswordForm> FormFromAttributes(
    GnomeKeyringAttributeList* attrs) {
  // Read the string and int attributes into the appropriate map.
  std::map<std::string, std::string> string_attr_map;
  std::map<std::string, uint32_t> uint_attr_map;
  for (guint i = 0; i < attrs->len; ++i) {
    GnomeKeyringAttribute attr = gnome_keyring_attribute_list_index(attrs, i);
    if (attr.type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING)
      string_attr_map[attr.name] = attr.value.string;
    else if (attr.type == GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32)
      uint_attr_map[attr.name] = attr.value.integer;
  }
  // Check to make sure this is a password we care about.
  const std::string& app_value = string_attr_map["application"];
  if (!base::StringPiece(app_value).starts_with(kLibsecretAndGnomeAppString))
    return std::unique_ptr<PasswordForm>();

  std::unique_ptr<PasswordForm> form(new PasswordForm());
  form->origin = GURL(string_attr_map["origin_url"]);
  form->action = GURL(string_attr_map["action_url"]);
  form->username_element = UTF8ToUTF16(string_attr_map["username_element"]);
  form->username_value = UTF8ToUTF16(string_attr_map["username_value"]);
  form->password_element = UTF8ToUTF16(string_attr_map["password_element"]);
  form->submit_element = UTF8ToUTF16(string_attr_map["submit_element"]);
  form->signon_realm = string_attr_map["signon_realm"];
  form->preferred = uint_attr_map["preferred"];
  int64_t date_created = 0;
  bool date_ok = base::StringToInt64(string_attr_map["date_created"],
                                     &date_created);
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
  form->blacklisted_by_user = uint_attr_map["blacklisted_by_user"];
  form->type = static_cast<PasswordForm::Type>(uint_attr_map["type"]);
  form->times_used = uint_attr_map["times_used"];
  form->scheme = static_cast<PasswordForm::Scheme>(uint_attr_map["scheme"]);
  int64_t date_synced = 0;
  base::StringToInt64(string_attr_map["date_synced"], &date_synced);
  form->date_synced = base::Time::FromInternalValue(date_synced);
  form->display_name = UTF8ToUTF16(string_attr_map["display_name"]);
  form->icon_url = GURL(string_attr_map["avatar_url"]);
  form->federation_origin =
      url::Origin::Create(GURL(string_attr_map["federation_url"]));
  form->skip_zero_click = uint_attr_map.count("should_skip_zero_click")
                              ? uint_attr_map["should_skip_zero_click"]
                              : true;
  form->generation_upload_status =
      static_cast<PasswordForm::GenerationUploadStatus>(
          uint_attr_map["generation_upload_status"]);
  if (!string_attr_map["form_data"].empty()) {
    bool success = DeserializeFormDataFromBase64String(
        string_attr_map["form_data"], &form->form_data);
    password_manager::metrics_util::FormDeserializationStatus status =
        success ? password_manager::metrics_util::GNOME_SUCCESS
                : password_manager::metrics_util::GNOME_FAILURE;
    LogFormDataDeserializationStatus(status);
  }
  return form;
}

// Converts native credentials in |found| to PasswordForms. If not NULL,
// |lookup_form| is used to filter out results -- only credentials with signon
// realms passing the PSL matching against |lookup_form->signon_realm| will be
// kept. PSL matched results get their signon_realm, origin, and action
// rewritten to those of |lookup_form_|, with the original signon_realm saved
// into the result's original_signon_realm data member.
std::vector<std::unique_ptr<PasswordForm>> ConvertFormList(
    GList* found,
    const PasswordStore::FormDigest* lookup_form) {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  password_manager::PSLDomainMatchMetric psl_domain_match_metric =
      password_manager::PSL_DOMAIN_MATCH_NONE;
  for (GList* element = g_list_first(found); element;
       element = g_list_next(element)) {
    GnomeKeyringFound* data = static_cast<GnomeKeyringFound*>(element->data);
    GnomeKeyringAttributeList* attrs = data->attributes;

    std::unique_ptr<PasswordForm> form(FormFromAttributes(attrs));
    if (!form) {
      LOG(WARNING) << "Could not initialize PasswordForm from attributes!";
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

    if (data->secret) {
      form->password_value = UTF8ToUTF16(data->secret);
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

// Schema is analogous to the fields in PasswordForm.
const GnomeKeyringPasswordSchema kGnomeSchema = {
    GNOME_KEYRING_ITEM_GENERIC_SECRET,
    {{"origin_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"action_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"username_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"username_value", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"password_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"submit_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"signon_realm", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"preferred", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32},
     {"date_created", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"blacklisted_by_user", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32},
     {"scheme", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32},
     {"type", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32},
     {"times_used", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32},
     {"date_synced", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"display_name", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"avatar_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"federation_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {"should_skip_zero_click", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32},
     {"generation_upload_status", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32},
     {"form_data", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     // This field is always "chrome" so that we can search for it.
     {"application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING},
     {nullptr}}};

// Sadly, PasswordStore goes to great lengths to switch from the originally
// calling thread to the DB thread, and to provide an asynchronous API to
// callers while using a synchronous (virtual) API provided by subclasses like
// PasswordStoreX -- but GNOME Keyring really wants to be on the GLib main
// thread, which is the UI thread to us. So we end up having to switch threads
// again, possibly back to the very same thread (in case the UI thread is the
// caller, e.g. in the password management UI), and *block* the DB thread
// waiting for a response from the UI thread to provide the synchronous API
// PasswordStore expects of us. (It will then in turn switch back to the
// original caller to send the asynchronous reply to the original request.)

// This class represents a call to a GNOME Keyring method. A RunnableMethod
// should be posted to the UI thread to call one of its action methods, and then
// a WaitResult() method should be called to wait for the result. Each instance
// supports only one outstanding method at a time, though multiple instances may
// be used in parallel.
class GKRMethod : public GnomeKeyringLoader {
 public:
  GKRMethod(scoped_refptr<base::SequencedTaskRunner> main_task_runner,
            scoped_refptr<base::SequencedTaskRunner> background_task_runner)
      : main_task_runner_(std::move(main_task_runner)),
        background_task_runner_(std::move(background_task_runner)),
        event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
               base::WaitableEvent::InitialState::NOT_SIGNALED),
        result_(GNOME_KEYRING_RESULT_CANCELLED) {}

  // Action methods. These call gnome_keyring_* functions. Call from UI thread.
  // See GetProfileSpecificAppString() for more information on the app string.
  void AddLogin(const PasswordForm& form, const char* app_string);
  void LoginSearch(const PasswordForm& form, const char* app_string);
  void RemoveLogin(const PasswordForm& form, const char* app_string);
  void GetLogins(const PasswordStore::FormDigest& form, const char* app_string);
  void GetLoginsList(uint32_t blacklisted_by_user, const char* app_string);
  void GetAllLogins(const char* app_string);

  // Use after AddLogin, RemoveLogin.
  GnomeKeyringResult WaitResult();

  // Use after LoginSearch, GetLogins, GetLoginsList, GetAllLogins. Replaces the
  // content of |forms| with found logins.
  GnomeKeyringResult WaitResult(
      std::vector<std::unique_ptr<PasswordForm>>* forms);

 private:
  struct GnomeKeyringAttributeListFreeDeleter {
    inline void operator()(void* list) const {
      gnome_keyring_attribute_list_free_ptr(
          static_cast<GnomeKeyringAttributeList*>(list));
    }
  };

  typedef std::unique_ptr<GnomeKeyringAttributeList,
                          GnomeKeyringAttributeListFreeDeleter>
      ScopedAttributeList;

  // Helper methods to abbreviate Gnome Keyring long API names.
  static void AppendString(ScopedAttributeList* list,
                           const char* name,
                           const char* value);
  static void AppendString(ScopedAttributeList* list,
                           const char* name,
                           const std::string& value);
  static void AppendUint32(ScopedAttributeList* list,
                           const char* name,
                           guint32 value);

  // All these callbacks are called on UI thread.
  static void OnOperationDone(GnomeKeyringResult result, gpointer data);

  // This is marked as static, but acts on the GKRMethod instance that |data|
  // points to. Saves |result| to |result_|. If the result is OK, overwrites
  // |forms_| with the found credentials. Clears |forms_| otherwise.
  static void OnOperationGetList(GnomeKeyringResult result, GList* list,
                                 gpointer data);

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  base::WaitableEvent event_;
  GnomeKeyringResult result_;
  std::vector<std::unique_ptr<PasswordForm>> forms_;
  // If the credential search is specified by a single form and needs to use
  // PSL matching, then the specifying form is stored in |lookup_form_|. If
  // PSL matching is used to find a result, then the results signon realm and
  // origin are stored are replaced by those of |lookup_form_|. Additionally,
  // |lookup_form_->signon_realm| is also used to narrow down the found logins
  // to those which indeed PSL-match the look-up. And finally, |lookup_form_|
  // set to NULL means that PSL matching is not required.
  std::unique_ptr<const PasswordStore::FormDigest> lookup_form_;
};

void GKRMethod::AddLogin(const PasswordForm& form, const char* app_string) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  int64_t date_created = form.date_created.ToInternalValue();
  // If we are asked to save a password with 0 date, use the current time.
  // We don't want to actually save passwords as though on January 1, 1601.
  if (!date_created)
    date_created = base::Time::Now().ToInternalValue();
  int64_t date_synced = form.date_synced.ToInternalValue();
  std::string form_data;
  SerializeFormDataToBase64String(form.form_data, &form_data);
  // clang-format off
  gnome_keyring_store_password_ptr(
      &kGnomeSchema,
      nullptr,                     // Default keyring.
      form.origin.spec().c_str(),  // Display name.
      UTF16ToUTF8(form.password_value).c_str(), OnOperationDone,
      this,     // data
      nullptr,  // destroy_data
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
      "application", app_string,
      nullptr);
  // clang-format on
}

void GKRMethod::LoginSearch(const PasswordForm& form,
                               const char* app_string) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  lookup_form_.reset(nullptr);
  // Search GNOME Keyring for matching passwords to update.
  ScopedAttributeList attrs(gnome_keyring_attribute_list_new_ptr());
  AppendString(&attrs, "origin_url", form.origin.spec());
  AppendString(&attrs, "username_element", UTF16ToUTF8(form.username_element));
  AppendString(&attrs, "username_value", UTF16ToUTF8(form.username_value));
  AppendString(&attrs, "password_element", UTF16ToUTF8(form.password_element));
  AppendString(&attrs, "signon_realm", form.signon_realm);
  AppendString(&attrs, "application", app_string);
  gnome_keyring_find_items_ptr(GNOME_KEYRING_ITEM_GENERIC_SECRET, attrs.get(),
                               OnOperationGetList,
                               /*data=*/this,
                               /*destroy_data=*/nullptr);
}

void GKRMethod::RemoveLogin(const PasswordForm& form, const char* app_string) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  // We find forms using the same fields as LoginDatabase::RemoveLogin().
  gnome_keyring_delete_password_ptr(
      &kGnomeSchema,
      OnOperationDone,
      this,     // data
      nullptr,  // destroy_data
      "origin_url", form.origin.spec().c_str(),
      "username_element", UTF16ToUTF8(form.username_element).c_str(),
      "username_value", UTF16ToUTF8(form.username_value).c_str(),
      "password_element", UTF16ToUTF8(form.password_element).c_str(),
      "signon_realm", form.signon_realm.c_str(),
      "application", app_string,
      nullptr);
}

void GKRMethod::GetLogins(const PasswordStore::FormDigest& form,
                          const char* app_string) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  lookup_form_.reset(new PasswordStore::FormDigest(form));
  // Search GNOME Keyring for matching passwords.
  ScopedAttributeList attrs(gnome_keyring_attribute_list_new_ptr());
  if (!password_manager::ShouldPSLDomainMatchingApply(
          password_manager::GetRegistryControlledDomain(
              GURL(form.signon_realm))) &&
      form.scheme != PasswordForm::SCHEME_HTML) {
    // Don't retrieve the PSL matched and federated credentials.
    AppendString(&attrs, "signon_realm", form.signon_realm);
  }
  AppendString(&attrs, "application", app_string);
  gnome_keyring_find_items_ptr(GNOME_KEYRING_ITEM_GENERIC_SECRET, attrs.get(),
                               OnOperationGetList,
                               /*data=*/this,
                               /*destroy_data=*/nullptr);
}

void GKRMethod::GetLoginsList(uint32_t blacklisted_by_user,
                              const char* app_string) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  lookup_form_.reset(nullptr);
  // Search GNOME Keyring for matching passwords.
  ScopedAttributeList attrs(gnome_keyring_attribute_list_new_ptr());
  AppendUint32(&attrs, "blacklisted_by_user", blacklisted_by_user);
  AppendString(&attrs, "application", app_string);
  gnome_keyring_find_items_ptr(GNOME_KEYRING_ITEM_GENERIC_SECRET, attrs.get(),
                               OnOperationGetList,
                               /*data=*/this,
                               /*destroy_data=*/nullptr);
}

void GKRMethod::GetAllLogins(const char* app_string) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  lookup_form_.reset(nullptr);
  // We need to search for something, otherwise we get no results - so
  // we search for the fixed application string.
  ScopedAttributeList attrs(gnome_keyring_attribute_list_new_ptr());
  AppendString(&attrs, "application", app_string);
  gnome_keyring_find_items_ptr(GNOME_KEYRING_ITEM_GENERIC_SECRET, attrs.get(),
                               OnOperationGetList,
                               /*data=*/this,
                               /*destroy_data=*/nullptr);
}

GnomeKeyringResult GKRMethod::WaitResult() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  event_.Wait();
  return result_;
}

GnomeKeyringResult GKRMethod::WaitResult(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  event_.Wait();
  *forms = std::move(forms_);
  return result_;
}

// static
void GKRMethod::AppendString(GKRMethod::ScopedAttributeList* list,
                             const char* name,
                             const char* value) {
  gnome_keyring_attribute_list_append_string_ptr(list->get(), name, value);
}

// static
void GKRMethod::AppendString(GKRMethod::ScopedAttributeList* list,
                             const char* name,
                             const std::string& value) {
  AppendString(list, name, value.c_str());
}

// static
void GKRMethod::AppendUint32(GKRMethod::ScopedAttributeList* list,
                             const char* name,
                             guint32 value) {
  gnome_keyring_attribute_list_append_uint32_ptr(list->get(), name, value);
}

// static
void GKRMethod::OnOperationDone(GnomeKeyringResult result, gpointer data) {
  GKRMethod* method = static_cast<GKRMethod*>(data);
  method->result_ = result;
  method->event_.Signal();
}

// static
void GKRMethod::OnOperationGetList(GnomeKeyringResult result, GList* list,
                                   gpointer data) {
  GKRMethod* method = static_cast<GKRMethod*>(data);
  method->result_ = result;
  // |list| will be freed after this callback returns, so convert it now.
  if (result == GNOME_KEYRING_RESULT_OK)
    method->forms_ = ConvertFormList(list, method->lookup_form_.get());
  else
    method->forms_.clear();
  method->lookup_form_.reset();
  method->event_.Signal();
}

}  // namespace

// This uses BrowserThread::UI for the main thread, because the Gnome Keyring
// library needs to run there (see the comment at the top of this file).
// This also uses USER_VISIBLE priority for the background task runner, because
// the passwords obtained through tasks on the background runner influence what
// the user sees. The need for WithBaseSyncPrimitives is caused by calls to
// Wait() to synchronise the background task with its response from
// GnomeKeyring which needs to be produced on the main task runner for library
// limitations. While unfortunate, https://crbug.com/739897 provides the
// discussion which resulted in accepting this until the whole backend is
// deprecated.
NativeBackendGnome::NativeBackendGnome(LocalProfileId id)
    : app_string_(GetProfileSpecificAppString(id)),
      main_task_runner_(base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI})),
      background_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::USER_VISIBLE})) {}

NativeBackendGnome::~NativeBackendGnome() {
}

bool NativeBackendGnome::Init() {
  return LoadGnomeKeyring() && gnome_keyring_is_available_ptr();
}

bool NativeBackendGnome::RawAddLogin(const PasswordForm& form) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  GKRMethod method(main_task_runner_, background_task_runner_);
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GKRMethod::AddLogin, base::Unretained(&method),
                                form, app_string_.c_str()));
  GnomeKeyringResult result = method.WaitResult();
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring save failed: "
               << gnome_keyring_result_to_message_ptr(result);
    return false;
  }
  return true;
}

password_manager::PasswordStoreChangeList NativeBackendGnome::AddLogin(
    const PasswordForm& form) {
  // Based on LoginDatabase::AddLogin(), we search for an existing match based
  // on origin_url, username_element, username_value, password_element, submit
  // element, and signon_realm first, remove that, and then add the new entry.
  // We'd add the new one first, and then delete the original, but then the
  // delete might actually delete the newly-added entry!
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  GKRMethod method(main_task_runner_, background_task_runner_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GKRMethod::LoginSearch, base::Unretained(&method), form,
                     app_string_.c_str()));
  std::vector<std::unique_ptr<PasswordForm>> forms;
  GnomeKeyringResult result = method.WaitResult(&forms);
  if (result != GNOME_KEYRING_RESULT_OK &&
      result != GNOME_KEYRING_RESULT_NO_MATCH) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message_ptr(result);
    return password_manager::PasswordStoreChangeList();
  }
  password_manager::PasswordStoreChangeList changes;
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

bool NativeBackendGnome::UpdateLogin(
    const PasswordForm& form,
    password_manager::PasswordStoreChangeList* changes) {
  // Based on LoginDatabase::UpdateLogin(), we search for forms to update by
  // origin_url, username_element, username_value, password_element, and
  // signon_realm. We then compare the result to the updated form. If they
  // differ in any of the mutable fields, then we remove the original, and
  // then add the new entry. We'd add the new one first, and then delete the
  // original, but then the delete might actually delete the newly-added entry!
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(changes);
  GKRMethod method(main_task_runner_, background_task_runner_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GKRMethod::LoginSearch, base::Unretained(&method), form,
                     app_string_.c_str()));
  std::vector<std::unique_ptr<PasswordForm>> forms;
  GnomeKeyringResult result = method.WaitResult(&forms);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message_ptr(result);
    return false;
  }
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

bool NativeBackendGnome::RemoveLogin(
    const PasswordForm& form,
    password_manager::PasswordStoreChangeList* changes) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(changes);
  GKRMethod method(main_task_runner_, background_task_runner_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GKRMethod::RemoveLogin, base::Unretained(&method), form,
                     app_string_.c_str()));
  GnomeKeyringResult result = method.WaitResult();
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;

  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring delete failed: "
               << gnome_keyring_result_to_message_ptr(result);
    return false;
  }
  changes->push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, form));
  return true;
}

bool NativeBackendGnome::RemoveLoginsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    password_manager::PasswordStoreChangeList* changes) {
  return RemoveLoginsBetween(
      delete_begin, delete_end, CREATION_TIMESTAMP, changes);
}

bool NativeBackendGnome::RemoveLoginsSyncedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    password_manager::PasswordStoreChangeList* changes) {
  return RemoveLoginsBetween(delete_begin, delete_end, SYNC_TIMESTAMP, changes);
}

bool NativeBackendGnome::DisableAutoSignInForOrigins(
    const base::Callback<bool(const GURL&)>& origin_filter,
    password_manager::PasswordStoreChangeList* changes) {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  if (!GetAllLogins(&forms))
    return false;

  for (const std::unique_ptr<PasswordForm>& form : forms) {
    if (origin_filter.Run(form->origin) && !form->skip_zero_click) {
      form->skip_zero_click = true;
      if (!UpdateLogin(*form, changes))
        return false;
    }
  }

  return true;
}

bool NativeBackendGnome::GetLogins(
    const PasswordStore::FormDigest& form,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  GKRMethod method(main_task_runner_, background_task_runner_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GKRMethod::GetLogins, base::Unretained(&method), form,
                     app_string_.c_str()));
  GnomeKeyringResult result = method.WaitResult(forms);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message_ptr(result);
    return false;
  }
  return true;
}

bool NativeBackendGnome::GetAutofillableLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  return GetLoginsList(true, forms);
}

bool NativeBackendGnome::GetBlacklistLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  return GetLoginsList(false, forms);
}

bool NativeBackendGnome::GetLoginsList(
    bool autofillable,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  uint32_t blacklisted_by_user = !autofillable;

  GKRMethod method(main_task_runner_, background_task_runner_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GKRMethod::GetLoginsList, base::Unretained(&method),
                     blacklisted_by_user, app_string_.c_str()));
  GnomeKeyringResult result = method.WaitResult(forms);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message_ptr(result);
    return false;
  }

  // Get rid of the forms with the same sync tags.
  std::vector<std::unique_ptr<PasswordForm>> duplicates;
  std::vector<std::vector<autofill::PasswordForm*>> tag_groups;
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

bool NativeBackendGnome::GetAllLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  GKRMethod method(main_task_runner_, background_task_runner_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GKRMethod::GetAllLogins, base::Unretained(&method),
                     app_string_.c_str()));
  GnomeKeyringResult result = method.WaitResult(forms);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message_ptr(result);
    return false;
  }
  return true;
}

scoped_refptr<base::SequencedTaskRunner>
NativeBackendGnome::GetBackgroundTaskRunner() {
  return background_task_runner_;
}

bool NativeBackendGnome::GetLoginsBetween(
    base::Time get_begin,
    base::Time get_end,
    TimestampToCompare date_to_compare,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  forms->clear();
  // We could walk the list and add items as we find them, but it is much
  // easier to build the list and then filter the results.
  std::vector<std::unique_ptr<PasswordForm>> all_forms;
  if (!GetAllLogins(&all_forms))
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

bool NativeBackendGnome::RemoveLoginsBetween(
    base::Time get_begin,
    base::Time get_end,
    TimestampToCompare date_to_compare,
    password_manager::PasswordStoreChangeList* changes) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(changes);
  changes->clear();
  // We could walk the list and delete items as we find them, but it is much
  // easier to build the list and use RemoveLogin() to delete them.
  std::vector<std::unique_ptr<PasswordForm>> forms;
  if (!GetLoginsBetween(get_begin, get_end, date_to_compare, &forms))
    return false;

  for (size_t i = 0; i < forms.size(); ++i) {
    if (!RemoveLogin(*forms[i], changes))
      return false;
  }
  return true;
}
