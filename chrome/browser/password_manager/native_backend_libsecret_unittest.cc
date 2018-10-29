// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "base/location.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/native_backend_libsecret.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::UTF8ToUTF16;
using base::UTF16ToUTF8;
using password_manager::PasswordStore;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using testing::Pointee;
using testing::UnorderedElementsAre;

namespace {

// What follows is a very simple implementation of the subset of the Libsecret
// API that we actually use. It gets substituted for the real one by
// MockLibsecretLoader, which hooks into the facility normally used to load
// the libsecret library at runtime to avoid a static dependency on it.

struct MockSecretValue {
  gchar* password;
  explicit MockSecretValue(gchar* password) : password(password) {}
  ~MockSecretValue() { g_free(password); }
};

struct MockSecretItem {
  std::unique_ptr<MockSecretValue> value;
  GHashTable* attributes;

  MockSecretItem(std::unique_ptr<MockSecretValue> value, GHashTable* attributes)
      : value(std::move(value)), attributes(attributes) {}
  ~MockSecretItem() {
    g_hash_table_destroy(attributes);
  }

  void RemoveAttribute(const char* keyname) {
    g_hash_table_remove(attributes, keyname);
  }
};

bool Matches(MockSecretItem* item, GHashTable* query) {
  GHashTable* attributes = item->attributes;
  GHashTableIter iter;
  gchar* name;
  gchar* query_value;
  g_hash_table_iter_init(&iter, query);

  while (g_hash_table_iter_next(&iter, reinterpret_cast<gpointer*>(&name),
                                reinterpret_cast<gpointer*>(&query_value))) {
    gchar* value = static_cast<gchar*>(g_hash_table_lookup(attributes, name));
    if (value == nullptr || strcmp(value, query_value) != 0)
      return false;
  }
  return true;
}

bool IsStringAttribute(const SecretSchema* schema, const std::string& name) {
  for (size_t i = 0; schema->attributes[i].name; ++i)
    if (name == schema->attributes[i].name)
      return schema->attributes[i].type == SECRET_SCHEMA_ATTRIBUTE_STRING;
  NOTREACHED() << "Requested type of nonexistent attribute";
  return false;
}

// The list of all libsecret items we have stored.
std::vector<std::unique_ptr<MockSecretItem>>* global_mock_libsecret_items;
std::unordered_map<GObject*, MockSecretItem*>* global_map_object_to_secret_item;
bool global_mock_libsecret_reject_local_ids = false;

GObject* MakeNewObject(MockSecretItem* item) {
  // Create an object with a ref-count of 2. The caller is expected to release
  // one reference, and the second reference is released during test tear down
  // along with checks that the ref count is correct.
  GObject* o = static_cast<GObject*>(g_object_new(G_TYPE_OBJECT, nullptr));
  g_object_ref(o);
  (*global_map_object_to_secret_item)[o] = item;
  return o;
}

gboolean mock_secret_password_store_sync(const SecretSchema* schema,
                                         const gchar* collection,
                                         const gchar* label,
                                         const gchar* password,
                                         GCancellable* cancellable,
                                         GError** error,
                                         ...) {
  // TODO(crbug.com/660005) We don't read the dummy we store to unlock keyring.
  if (strcmp("_chrome_dummy_schema_for_unlocking", schema->name) == 0)
    return true;

  GHashTable* attributes =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  va_list ap;
  va_start(ap, error);
  char* name;
  while ((name = va_arg(ap, gchar*))) {
    char* value;
    if (IsStringAttribute(schema, name)) {
      value = g_strdup(va_arg(ap, gchar*));
      VLOG(1) << "Adding item attribute " << name << ", value '" << value
              << "'";
    } else {
      uint32_t intvalue = va_arg(ap, uint32_t);
      VLOG(1) << "Adding item attribute " << name << ", value " << intvalue;
      value = g_strdup_printf("%u", intvalue);
    }
    g_hash_table_insert(attributes, g_strdup(name), value);
  }
  va_end(ap);
  global_mock_libsecret_items->push_back(std::make_unique<MockSecretItem>(
      std::make_unique<MockSecretValue>(g_strdup(password)), attributes));
  return true;
}

GList* mock_secret_service_search_sync(SecretService* service,
                                       const SecretSchema* schema,
                                       GHashTable* attributes,
                                       SecretSearchFlags flags,
                                       GCancellable* cancellable,
                                       GError** error) {
  EXPECT_TRUE(flags & SECRET_SEARCH_UNLOCK);
  GList* result = nullptr;
  for (std::unique_ptr<MockSecretItem>& item : *global_mock_libsecret_items) {
    if (Matches(item.get(), attributes)) {
      result = g_list_append(result, MakeNewObject(item.get()));
    }
  }
  return result;
}

gboolean mock_secret_password_clear_sync(const SecretSchema* schema,
                                         GCancellable* cancellable,
                                         GError** error,
                                         ...) {
  GHashTable* attributes =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  va_list ap;
  va_start(ap, error);
  char* name;
  while ((name = va_arg(ap, gchar*))) {
    char* value;
    if (IsStringAttribute(schema, name)) {
      value = g_strdup(va_arg(ap, gchar*));
      VLOG(1) << "Adding item attribute " << name << ", value '" << value
              << "'";
    } else {
      uint32_t intvalue = va_arg(ap, uint32_t);
      VLOG(1) << "Adding item attribute " << name << ", value " << intvalue;
      value = g_strdup_printf("%u", intvalue);
    }
    g_hash_table_insert(attributes, g_strdup(name), value);
  }
  va_end(ap);

  std::vector<std::unique_ptr<MockSecretItem>> kept_mock_libsecret_items;
  kept_mock_libsecret_items.reserve(global_mock_libsecret_items->size());
  for (std::unique_ptr<MockSecretItem>& item : *global_mock_libsecret_items) {
    if (!Matches(item.get(), attributes)) {
      kept_mock_libsecret_items.push_back(std::move(item));
    }
  }
  global_mock_libsecret_items->swap(kept_mock_libsecret_items);

  g_hash_table_unref(attributes);
  return
      global_mock_libsecret_items->size() != kept_mock_libsecret_items.size();
}

SecretValue* mock_secret_item_get_secret(SecretItem* self) {
  GObject* o = reinterpret_cast<GObject*>(self);
  MockSecretValue* mock_value =
      (*global_map_object_to_secret_item)[o]->value.get();
  return reinterpret_cast<SecretValue*>(mock_value);
}

const gchar* mock_secret_value_get_text(SecretValue* value) {
  return reinterpret_cast<MockSecretValue*>(value)->password;
}

GHashTable* mock_secret_item_get_attributes(SecretItem* self) {
  // Libsecret backend will make unreference of received attributes, so in
  // order to save them we need to increase their reference number.
  GObject* o = reinterpret_cast<GObject*>(self);
  MockSecretItem* mock_ptr = (*global_map_object_to_secret_item)[o];
  g_hash_table_ref(mock_ptr->attributes);
  return mock_ptr->attributes;
}

gboolean mock_secret_item_load_secret_sync(SecretItem* self,
                                           GCancellable* cancellable,
                                           GError** error) {
  return true;
}

void mock_secret_value_unref(gpointer value) {
}

// Inherit to get access to protected fields.
class MockLibsecretLoader : public LibsecretLoader {
 public:
  static bool LoadMockLibsecret() {
    secret_password_store_sync = &mock_secret_password_store_sync;
    secret_service_search_sync = &mock_secret_service_search_sync;
    secret_password_clear_sync = &mock_secret_password_clear_sync;
    secret_item_get_secret = &mock_secret_item_get_secret;
    secret_value_get_text = &mock_secret_value_get_text;
    secret_item_get_attributes = &mock_secret_item_get_attributes;
    secret_item_load_secret_sync = &mock_secret_item_load_secret_sync;
    secret_value_unref = &mock_secret_value_unref;
    libsecret_loaded_ = true;
    // Reset the state of the mock library.
    global_mock_libsecret_items->clear();
    global_map_object_to_secret_item->clear();
    global_mock_libsecret_reject_local_ids = false;
    return true;
  }
};

void CheckPasswordChanges(const PasswordStoreChangeList& expected_list,
                          const PasswordStoreChangeList& actual_list) {
  ASSERT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < expected_list.size(); ++i) {
    EXPECT_EQ(expected_list[i].type(), actual_list[i].type());
    EXPECT_EQ(expected_list[i].form(), actual_list[i].form());
  }
}

void VerifiedAdd(NativeBackendLibsecret* backend, const PasswordForm& form) {
  SCOPED_TRACE("VerifiedAdd");
  PasswordStoreChangeList changes = backend->AddLogin(form);
  PasswordStoreChangeList expected(1,
                                   PasswordStoreChange(PasswordStoreChange::ADD,
                                                       form));
  CheckPasswordChanges(expected, changes);
}

void VerifiedUpdate(NativeBackendLibsecret* backend, const PasswordForm& form) {
  SCOPED_TRACE("VerifiedUpdate");
  PasswordStoreChangeList changes;
  EXPECT_TRUE(backend->UpdateLogin(form, &changes));
  PasswordStoreChangeList expected(1, PasswordStoreChange(
      PasswordStoreChange::UPDATE, form));
  CheckPasswordChanges(expected, changes);
}

void VerifiedRemove(NativeBackendLibsecret* backend, const PasswordForm& form) {
  SCOPED_TRACE("VerifiedRemove");
  PasswordStoreChangeList changes;
  EXPECT_TRUE(backend->RemoveLogin(form, &changes));
  CheckPasswordChanges(PasswordStoreChangeList(1, PasswordStoreChange(
      PasswordStoreChange::REMOVE, form)), changes);
}

}  // anonymous namespace

class NativeBackendLibsecretTest : public testing::Test {
 protected:
  enum UpdateType {  // Used in CheckPSLUpdate().
    UPDATE_BY_UPDATELOGIN,
    UPDATE_BY_ADDLOGIN,
  };
  enum RemoveBetweenMethod {  // Used in CheckRemoveLoginsBetween().
    CREATED,
    SYNCED,
  };

  NativeBackendLibsecretTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    ASSERT_FALSE(global_mock_libsecret_items);
    ASSERT_FALSE(global_map_object_to_secret_item);
    global_mock_libsecret_items = &mock_libsecret_items_;
    global_map_object_to_secret_item = &mock_map_object_to_secret_item_;

    ASSERT_TRUE(MockLibsecretLoader::LoadMockLibsecret());

    form_google_.origin = GURL("http://www.google.com/");
    form_google_.action = GURL("http://www.google.com/login");
    form_google_.username_element = UTF8ToUTF16("user");
    form_google_.username_value = UTF8ToUTF16("joeschmoe");
    form_google_.password_element = UTF8ToUTF16("pass");
    form_google_.password_value = UTF8ToUTF16("seekrit");
    form_google_.submit_element = UTF8ToUTF16("submit");
    form_google_.signon_realm = "http://www.google.com/";
    form_google_.type = PasswordForm::TYPE_GENERATED;
    form_google_.date_created = base::Time::Now();
    form_google_.date_synced = base::Time::Now();
    form_google_.display_name = UTF8ToUTF16("Joe Schmoe");
    form_google_.icon_url = GURL("http://www.google.com/icon");
    form_google_.federation_origin =
        url::Origin::Create(GURL("http://www.google.com/"));
    form_google_.skip_zero_click = true;
    form_google_.generation_upload_status = PasswordForm::POSITIVE_SIGNAL_SENT;
    form_google_.form_data.name = UTF8ToUTF16("form_name");

    form_facebook_.origin = GURL("http://www.facebook.com/");
    form_facebook_.action = GURL("http://www.facebook.com/login");
    form_facebook_.username_element = UTF8ToUTF16("user");
    form_facebook_.username_value = UTF8ToUTF16("a");
    form_facebook_.password_element = UTF8ToUTF16("password");
    form_facebook_.password_value = UTF8ToUTF16("b");
    form_facebook_.submit_element = UTF8ToUTF16("submit");
    form_facebook_.signon_realm = "http://www.facebook.com/";
    form_facebook_.date_created = base::Time::Now();
    form_facebook_.date_synced = base::Time::Now();
    form_facebook_.display_name = UTF8ToUTF16("Joe Schmoe");
    form_facebook_.icon_url = GURL("http://www.facebook.com/icon");
    form_facebook_.federation_origin =
        url::Origin::Create(GURL("http://www.facebook.com/"));
    form_facebook_.skip_zero_click = true;
    form_facebook_.generation_upload_status = PasswordForm::NO_SIGNAL_SENT;

    form_isc_.origin = GURL("http://www.isc.org/");
    form_isc_.action = GURL("http://www.isc.org/auth");
    form_isc_.username_element = UTF8ToUTF16("id");
    form_isc_.username_value = UTF8ToUTF16("janedoe");
    form_isc_.password_element = UTF8ToUTF16("passwd");
    form_isc_.password_value = UTF8ToUTF16("ihazabukkit");
    form_isc_.submit_element = UTF8ToUTF16("login");
    form_isc_.signon_realm = "http://www.isc.org/";
    form_isc_.date_created = base::Time::Now();
    form_isc_.date_synced = base::Time::Now();

    other_auth_.origin = GURL("http://www.example.com/");
    other_auth_.username_value = UTF8ToUTF16("username");
    other_auth_.password_value = UTF8ToUTF16("pass");
    other_auth_.signon_realm = "http://www.example.com/Realm";
    other_auth_.date_created = base::Time::Now();
    other_auth_.date_synced = base::Time::Now();
  }

  void TearDown() override {
    scoped_task_environment_.RunUntilIdle();
    ASSERT_TRUE(global_mock_libsecret_items);
    global_mock_libsecret_items = nullptr;
    for (auto& pair : *global_map_object_to_secret_item) {
      ASSERT_EQ(pair.first->ref_count, 1u);
      g_object_unref(pair.first);
    }
    global_map_object_to_secret_item->clear();
    global_map_object_to_secret_item = nullptr;
  }

  void CheckUint32Attribute(const MockSecretItem* item,
                            const std::string& attribute,
                            uint32_t value) {
    gpointer item_value =
        g_hash_table_lookup(item->attributes, attribute.c_str());
    EXPECT_TRUE(item_value) << " in attribute " << attribute;
    if (item_value) {
      uint32_t int_value;
      bool conversion_ok =
          base::StringToUint(static_cast<char*>(item_value), &int_value);
      EXPECT_TRUE(conversion_ok);
      EXPECT_EQ(value, int_value);
    }
  }

  void CheckStringAttribute(const MockSecretItem* item,
                            const std::string& attribute,
                            const std::string& value) {
    gpointer item_value =
        g_hash_table_lookup(item->attributes, attribute.c_str());
    EXPECT_TRUE(item_value) << " in attribute " << attribute;
    if (item_value) {
      EXPECT_EQ(value, static_cast<char*>(item_value));
    }
  }

  void CheckMockSecretItem(const MockSecretItem* item,
                           const PasswordForm& form,
                           const std::string& app_string) {
    EXPECT_EQ(UTF16ToUTF8(form.password_value), item->value->password);
    EXPECT_EQ(21u, g_hash_table_size(item->attributes));
    CheckStringAttribute(item, "origin_url", form.origin.spec());
    CheckStringAttribute(item, "action_url", form.action.spec());
    CheckStringAttribute(item, "username_element",
                         UTF16ToUTF8(form.username_element));
    CheckStringAttribute(item, "username_value",
                         UTF16ToUTF8(form.username_value));
    CheckStringAttribute(item, "password_element",
                         UTF16ToUTF8(form.password_element));
    CheckStringAttribute(item, "submit_element",
                         UTF16ToUTF8(form.submit_element));
    CheckStringAttribute(item, "signon_realm", form.signon_realm);
    CheckUint32Attribute(item, "preferred", form.preferred);
    // We don't check the date created. It varies.
    CheckUint32Attribute(item, "blacklisted_by_user", form.blacklisted_by_user);
    CheckUint32Attribute(item, "type", form.type);
    CheckUint32Attribute(item, "times_used", form.times_used);
    CheckUint32Attribute(item, "scheme", form.scheme);
    CheckStringAttribute(
        item, "date_synced",
        base::Int64ToString(form.date_synced.ToInternalValue()));
    CheckStringAttribute(item, "display_name", UTF16ToUTF8(form.display_name));
    CheckStringAttribute(item, "avatar_url", form.icon_url.spec());
    // We serialize unique origins as "", in order to make other systems that
    // read from the login database happy. https://crbug.com/591310
    CheckStringAttribute(item, "federation_url",
                         form.federation_origin.opaque()
                             ? ""
                             : form.federation_origin.Serialize());
    CheckUint32Attribute(item, "should_skip_zero_click", form.skip_zero_click);
    CheckUint32Attribute(item, "generation_upload_status",
                         form.generation_upload_status);
    CheckStringAttribute(item, "application", app_string);
    autofill::FormData actual;
    DeserializeFormDataFromBase64String(
        static_cast<char*>(g_hash_table_lookup(item->attributes, "form_data")),
        &actual);
    EXPECT_TRUE(form.form_data.SameFormAs(actual));
  }

  // Saves |credentials| and then gets logins matching |url| and |scheme|.
  // Returns true when something is found, and in such case copies the result to
  // |result| when |result| is not nullptr. (Note that there can be max. 1
  // result derived from |credentials|.)
  bool CheckCredentialAvailability(const PasswordForm& credentials,
                                   const GURL& url,
                                   const PasswordForm::Scheme& scheme,
                                   PasswordForm* result) {
    NativeBackendLibsecret backend(321);

    VerifiedAdd(&backend, credentials);

    PasswordStore::FormDigest target_form = {PasswordForm::SCHEME_HTML,
                                             url.spec(), url};
    if (scheme != PasswordForm::SCHEME_HTML) {
      // For non-HTML forms, the realm used for authentication
      // (http://tools.ietf.org/html/rfc1945#section-10.2) is appended to the
      // signon_realm. Just use a default value for now.
      target_form.signon_realm.append("Realm");
    }
    std::vector<std::unique_ptr<PasswordForm>> form_list;
    EXPECT_TRUE(backend.GetLogins(target_form, &form_list));

    EXPECT_EQ(1u, global_mock_libsecret_items->size());
    if (!global_mock_libsecret_items->empty())
      CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), credentials,
                          "chrome-321");
    global_mock_libsecret_items->clear();

    if (form_list.empty())
      return false;
    EXPECT_EQ(1u, form_list.size());
    if (result)
      *result = *form_list[0];
    return true;
  }

  // Test that updating does not use PSL matching: Add a www.facebook.com
  // password, then use PSL matching to get a copy of it for m.facebook.com, and
  // add that copy as well. Now update the www.facebook.com password -- the
  // m.facebook.com password should not get updated. Depending on the argument,
  // the credential update is done via UpdateLogin or AddLogin.
  void CheckPSLUpdate(UpdateType update_type) {
    NativeBackendLibsecret backend(321);

    VerifiedAdd(&backend, form_facebook_);

    // Get the PSL-matched copy of the saved login for m.facebook.
    const GURL kMobileURL("http://m.facebook.com/");
    PasswordStore::FormDigest m_facebook_lookup = {
        PasswordForm::SCHEME_HTML, kMobileURL.spec(), kMobileURL};
    std::vector<std::unique_ptr<PasswordForm>> form_list;
    EXPECT_TRUE(backend.GetLogins(m_facebook_lookup, &form_list));

    EXPECT_EQ(1u, global_mock_libsecret_items->size());
    EXPECT_EQ(1u, form_list.size());
    PasswordForm m_facebook = *form_list[0];
    form_list.clear();
    m_facebook.origin = kMobileURL;
    m_facebook.signon_realm = kMobileURL.spec();

    // Add the PSL-matched copy to saved logins.
    VerifiedAdd(&backend, m_facebook);
    EXPECT_EQ(2u, global_mock_libsecret_items->size());

    // Update www.facebook.com login.
    PasswordForm new_facebook(form_facebook_);
    const base::string16 kOldPassword(form_facebook_.password_value);
    const base::string16 kNewPassword(UTF8ToUTF16("new_b"));
    EXPECT_NE(kOldPassword, kNewPassword);
    new_facebook.password_value = kNewPassword;
    switch (update_type) {
      case UPDATE_BY_UPDATELOGIN:
        VerifiedUpdate(&backend, new_facebook);
        break;
      case UPDATE_BY_ADDLOGIN:
        // This is an overwrite call.
        backend.AddLogin(new_facebook);
        break;
    }

    EXPECT_EQ(2u, global_mock_libsecret_items->size());

    // Check that m.facebook.com login was not modified by the update.
    EXPECT_TRUE(backend.GetLogins(m_facebook_lookup, &form_list));

    // There should be two results -- the exact one, and the PSL-matched one.
    EXPECT_EQ(2u, form_list.size());
    size_t index_non_psl = 0;
    if (form_list[index_non_psl]->is_public_suffix_match)
      index_non_psl = 1;
    EXPECT_EQ(kMobileURL, form_list[index_non_psl]->origin);
    EXPECT_EQ(kMobileURL.spec(), form_list[index_non_psl]->signon_realm);
    EXPECT_EQ(kOldPassword, form_list[index_non_psl]->password_value);
    form_list.clear();

    // Check that www.facebook.com login was modified by the update.
    EXPECT_TRUE(backend.GetLogins(PasswordStore::FormDigest(form_facebook_),
                                  &form_list));
    // There should be two results -- the exact one, and the PSL-matched one.
    EXPECT_EQ(2u, form_list.size());
    index_non_psl = 0;
    if (form_list[index_non_psl]->is_public_suffix_match)
      index_non_psl = 1;
    EXPECT_EQ(form_facebook_.origin, form_list[index_non_psl]->origin);
    EXPECT_EQ(form_facebook_.signon_realm,
              form_list[index_non_psl]->signon_realm);
    EXPECT_EQ(kNewPassword, form_list[index_non_psl]->password_value);
    form_list.clear();
  }

  // Checks various types of matching for forms with a non-HTML |scheme|.
  void CheckMatchingWithScheme(const PasswordForm::Scheme& scheme) {
    ASSERT_NE(PasswordForm::SCHEME_HTML, scheme);
    other_auth_.scheme = scheme;

    // Don't match a non-HTML form with an HTML form.
    EXPECT_FALSE(
        CheckCredentialAvailability(other_auth_, GURL("http://www.example.com"),
                                    PasswordForm::SCHEME_HTML, nullptr));
    // Don't match an HTML form with non-HTML auth form.
    EXPECT_FALSE(CheckCredentialAvailability(
        form_google_, GURL("http://www.google.com/"), scheme, nullptr));
    // Don't match two different non-HTML auth forms with different origin.
    EXPECT_FALSE(CheckCredentialAvailability(
        other_auth_, GURL("http://first.example.com"), scheme, nullptr));
    // Do match non-HTML forms from the same origin.
    EXPECT_TRUE(CheckCredentialAvailability(
        other_auth_, GURL("http://www.example.com/"), scheme, nullptr));
  }

  void CheckRemoveLoginsBetween(RemoveBetweenMethod date_to_test) {
    NativeBackendLibsecret backend(42);

    base::Time now = base::Time::Now();
    base::Time next_day = now + base::TimeDelta::FromDays(1);
    form_google_.date_synced = base::Time();
    form_isc_.date_synced = base::Time();
    form_google_.date_created = now;
    form_isc_.date_created = now;
    if (date_to_test == CREATED) {
      form_google_.date_created = now;
      form_isc_.date_created = next_day;
    } else {
      form_google_.date_synced = now;
      form_isc_.date_synced = next_day;
    }

    VerifiedAdd(&backend, form_google_);
    VerifiedAdd(&backend, form_isc_);

    PasswordStoreChangeList expected_changes;
    expected_changes.push_back(
        PasswordStoreChange(PasswordStoreChange::REMOVE, form_google_));
    PasswordStoreChangeList changes;
    bool (NativeBackendLibsecret::*method)(
        base::Time, base::Time, password_manager::PasswordStoreChangeList*) =
        date_to_test == CREATED
            ? &NativeBackendLibsecret::RemoveLoginsCreatedBetween
            : &NativeBackendLibsecret::RemoveLoginsSyncedBetween;

    EXPECT_TRUE(base::Bind(method, base::Unretained(&backend), base::Time(),
                           next_day, &changes).Run());
    CheckPasswordChanges(expected_changes, changes);

    EXPECT_EQ(1u, global_mock_libsecret_items->size());
    if (!global_mock_libsecret_items->empty() > 0)
      CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_isc_,
                          "chrome-42");

    // Remove form_isc_.
    expected_changes.clear();
    expected_changes.push_back(
        PasswordStoreChange(PasswordStoreChange::REMOVE, form_isc_));

    EXPECT_TRUE(base::Bind(method, base::Unretained(&backend), next_day,
                           base::Time(), &changes).Run());
    CheckPasswordChanges(expected_changes, changes);

    EXPECT_TRUE(global_mock_libsecret_items->empty());
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Provide some test forms to avoid having to set them up in each test.
  PasswordForm form_google_;
  PasswordForm form_facebook_;
  PasswordForm form_isc_;
  PasswordForm other_auth_;

  std::vector<std::unique_ptr<MockSecretItem>> mock_libsecret_items_;
  std::unordered_map<GObject*, MockSecretItem*> mock_map_object_to_secret_item_;
};

TEST_F(NativeBackendLibsecretTest, BasicAddLogin) {
  NativeBackendLibsecret backend(42);

  VerifiedAdd(&backend, form_google_);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty())
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");
}

TEST_F(NativeBackendLibsecretTest, BasicListLogins) {
  NativeBackendLibsecret backend(42);

  VerifiedAdd(&backend, form_google_);

  std::vector<std::unique_ptr<PasswordForm>> form_list;
  EXPECT_TRUE(backend.GetAutofillableLogins(&form_list));

  ASSERT_EQ(1u, form_list.size());
  EXPECT_EQ(form_google_, *form_list[0]);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty())
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");
}

TEST_F(NativeBackendLibsecretTest, GetAllLogins) {
  NativeBackendLibsecret backend(42);

  VerifiedAdd(&backend, form_google_);
  VerifiedAdd(&backend, form_facebook_);

  std::vector<std::unique_ptr<PasswordForm>> form_list;
  EXPECT_TRUE(backend.GetAllLogins(&form_list));

  ASSERT_EQ(2u, form_list.size());
  EXPECT_THAT(form_list, UnorderedElementsAre(Pointee(form_google_),
                                              Pointee(form_facebook_)));
}

// Save a password for www.facebook.com and see it suggested for m.facebook.com.
TEST_F(NativeBackendLibsecretTest, PSLMatchingPositive) {
  PasswordForm result;
  const GURL kMobileURL("http://m.facebook.com/");
  EXPECT_TRUE(CheckCredentialAvailability(form_facebook_, kMobileURL,
                                          PasswordForm::SCHEME_HTML, &result));
  EXPECT_EQ(form_facebook_.origin, result.origin);
  EXPECT_EQ(form_facebook_.signon_realm, result.signon_realm);
}

// Save a password for www.facebook.com and see it not suggested for
// m-facebook.com.
TEST_F(NativeBackendLibsecretTest, PSLMatchingNegativeDomainMismatch) {
  EXPECT_FALSE(CheckCredentialAvailability(form_facebook_,
                                           GURL("http://m-facebook.com/"),
                                           PasswordForm::SCHEME_HTML, nullptr));
}

// Test PSL matching is off for domains excluded from it.
TEST_F(NativeBackendLibsecretTest, PSLMatchingDisabledDomains) {
  EXPECT_FALSE(CheckCredentialAvailability(form_google_,
                                           GURL("http://one.google.com/"),
                                           PasswordForm::SCHEME_HTML, nullptr));
}

// Make sure PSL matches aren't available for non-HTML forms.
TEST_F(NativeBackendLibsecretTest, PSLMatchingDisabledForNonHTMLForms) {
  CheckMatchingWithScheme(PasswordForm::SCHEME_BASIC);
  CheckMatchingWithScheme(PasswordForm::SCHEME_DIGEST);
  CheckMatchingWithScheme(PasswordForm::SCHEME_OTHER);
}

TEST_F(NativeBackendLibsecretTest, PSLUpdatingStrictUpdateLogin) {
  CheckPSLUpdate(UPDATE_BY_UPDATELOGIN);
}

TEST_F(NativeBackendLibsecretTest, PSLUpdatingStrictAddLogin) {
  // TODO(vabr): if AddLogin becomes no longer valid for existing logins, then
  // just delete this test.
  CheckPSLUpdate(UPDATE_BY_ADDLOGIN);
}

TEST_F(NativeBackendLibsecretTest, FetchFederatedCredentialOnHTTPS) {
  other_auth_.signon_realm = "federation://www.example.com/google.com";
  other_auth_.origin = GURL("https://www.example.com/");
  other_auth_.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  EXPECT_TRUE(CheckCredentialAvailability(other_auth_,
                                          GURL("https://www.example.com/"),
                                          PasswordForm::SCHEME_HTML, nullptr));
}

TEST_F(NativeBackendLibsecretTest, FetchFederatedCredentialOnLocalhost) {
  other_auth_.signon_realm = "federation://localhost/google.com";
  other_auth_.origin = GURL("http://localhost:8080/");
  other_auth_.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  EXPECT_TRUE(CheckCredentialAvailability(other_auth_,
                                          GURL("http://localhost:8080/"),
                                          PasswordForm::SCHEME_HTML, nullptr));
}

TEST_F(NativeBackendLibsecretTest, DontFetchFederatedCredentialOnHTTP) {
  other_auth_.signon_realm = "federation://www.example.com/google.com";
  other_auth_.origin = GURL("https://www.example.com/");
  other_auth_.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  EXPECT_FALSE(CheckCredentialAvailability(other_auth_,
                                           GURL("http://www.example.com/"),
                                           PasswordForm::SCHEME_HTML, nullptr));
}

TEST_F(NativeBackendLibsecretTest, FetchPSLMatchedFederatedCredentialOnHTTPS) {
  other_auth_.signon_realm = "federation://www.sub.example.com/google.com";
  other_auth_.origin = GURL("https://www.sub.example.com/");
  other_auth_.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  EXPECT_TRUE(CheckCredentialAvailability(other_auth_,
                                          GURL("https://www.example.com/"),
                                          PasswordForm::SCHEME_HTML, nullptr));
}

TEST_F(NativeBackendLibsecretTest,
       DontFetchPSLMatchedFederatedCredentialOnHTTP) {
  other_auth_.signon_realm = "federation://www.sub.example.com/google.com";
  other_auth_.origin = GURL("https://www.sub.example.com/");
  other_auth_.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  EXPECT_FALSE(CheckCredentialAvailability(other_auth_,
                                           GURL("http://www.example.com/"),
                                           PasswordForm::SCHEME_HTML, nullptr));
}

TEST_F(NativeBackendLibsecretTest, BasicUpdateLogin) {
  NativeBackendLibsecret backend(42);

  VerifiedAdd(&backend, form_google_);

  PasswordForm new_form_google(form_google_);
  new_form_google.times_used = 1;
  new_form_google.action = GURL("http://www.google.com/different/login");

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty()) {
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");
  }

  // Update login
  VerifiedUpdate(&backend, new_form_google);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty())
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(),
                        new_form_google, "chrome-42");
}

TEST_F(NativeBackendLibsecretTest, BasicRemoveLogin) {
  NativeBackendLibsecret backend(42);

  VerifiedAdd(&backend, form_google_);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty())
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");

  VerifiedRemove(&backend, form_google_);

  EXPECT_TRUE(global_mock_libsecret_items->empty());
}

// Verify fix for http://crbug.com/408783.
TEST_F(NativeBackendLibsecretTest, RemoveLoginActionMismatch) {
  NativeBackendLibsecret backend(42);

  VerifiedAdd(&backend, form_google_);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty())
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");

  // Action url match not required for removal.
  form_google_.action = GURL("https://some.other.url.com/path");
  VerifiedRemove(&backend, form_google_);

  EXPECT_TRUE(global_mock_libsecret_items->empty());
}

TEST_F(NativeBackendLibsecretTest, RemoveNonexistentLogin) {
  NativeBackendLibsecret backend(42);

  // First add an unrelated login.
  VerifiedAdd(&backend, form_google_);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty())
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");

  // Attempt to remove a login that doesn't exist.
  PasswordStoreChangeList changes;
  EXPECT_TRUE(backend.RemoveLogin(form_isc_, &changes));
  CheckPasswordChanges(PasswordStoreChangeList(), changes);

  // Make sure we can still get the first form back.
  std::vector<std::unique_ptr<PasswordForm>> form_list;
  EXPECT_TRUE(backend.GetAutofillableLogins(&form_list));

  // Quick check that we got something back.
  EXPECT_EQ(1u, form_list.size());
  form_list.clear();

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty())
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");
}

TEST_F(NativeBackendLibsecretTest, UpdateNonexistentLogin) {
  NativeBackendLibsecret backend(42);

  // First add an unrelated login.
  VerifiedAdd(&backend, form_google_);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty()) {
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");
  }

  // Attempt to update a login that doesn't exist.
  PasswordStoreChangeList changes;
  EXPECT_TRUE(backend.UpdateLogin(form_isc_, &changes));
  CheckPasswordChanges(PasswordStoreChangeList(), changes);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty())
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");
}

TEST_F(NativeBackendLibsecretTest, UpdateSameLogin) {
  NativeBackendLibsecret backend(42);

  VerifiedAdd(&backend, form_google_);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty()) {
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");
  }

  // Attempt to update the same login without changing anything.
  PasswordStoreChangeList changes;
  EXPECT_TRUE(backend.UpdateLogin(form_google_, &changes));
  CheckPasswordChanges(PasswordStoreChangeList(), changes);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty()) {
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");
  }
}

TEST_F(NativeBackendLibsecretTest, AddDuplicateLogin) {
  NativeBackendLibsecret backend(42);

  VerifiedAdd(&backend, form_google_);

  PasswordStoreChangeList expected_changes;
  expected_changes.push_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, form_google_));
  form_google_.times_used++;
  form_google_.submit_element = UTF8ToUTF16("submit2");
  expected_changes.push_back(
      PasswordStoreChange(PasswordStoreChange::ADD, form_google_));

  PasswordStoreChangeList actual_changes = backend.AddLogin(form_google_);
  CheckPasswordChanges(expected_changes, actual_changes);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty())
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");
}

TEST_F(NativeBackendLibsecretTest, AndroidCredentials) {
  NativeBackendLibsecret backend(42);
  backend.Init();

  PasswordForm observed_android_form;
  observed_android_form.scheme = PasswordForm::SCHEME_HTML;
  observed_android_form.signon_realm =
      "android://7x7IDboo8u9YKraUsbmVkuf1-@net.rateflix.app/";
  PasswordForm saved_android_form = observed_android_form;
  saved_android_form.username_value = base::UTF8ToUTF16("randomusername");
  saved_android_form.password_value = base::UTF8ToUTF16("password");
  saved_android_form.date_created = base::Time::Now();

  VerifiedAdd(&backend, saved_android_form);

  std::vector<std::unique_ptr<PasswordForm>> form_list;
  EXPECT_TRUE(backend.GetAutofillableLogins(&form_list));

  EXPECT_EQ(1u, form_list.size());
  EXPECT_EQ(saved_android_form, *form_list[0]);
}

TEST_F(NativeBackendLibsecretTest, RemoveLoginsCreatedBetween) {
  CheckRemoveLoginsBetween(CREATED);
}

TEST_F(NativeBackendLibsecretTest, RemoveLoginsSyncedBetween) {
  CheckRemoveLoginsBetween(SYNCED);
}

TEST_F(NativeBackendLibsecretTest, DisableAutoSignInForOrigins) {
  NativeBackendLibsecret backend(42);
  backend.Init();
  form_google_.skip_zero_click = false;
  form_facebook_.skip_zero_click = false;

  VerifiedAdd(&backend, form_google_);
  VerifiedAdd(&backend, form_facebook_);

  EXPECT_EQ(2u, global_mock_libsecret_items->size());
  for (const auto& item : *global_mock_libsecret_items)
    CheckUint32Attribute(item.get(), "should_skip_zero_click", 0);

  // Set the canonical forms to the updated value for the following comparison.
  form_google_.skip_zero_click = true;
  form_facebook_.skip_zero_click = true;
  PasswordStoreChangeList expected_changes;
  expected_changes.push_back(
      PasswordStoreChange(PasswordStoreChange::UPDATE, form_facebook_));

  PasswordStoreChangeList changes;
  EXPECT_TRUE(backend.DisableAutoSignInForOrigins(
      base::Bind(static_cast<bool (*)(const GURL&, const GURL&)>(operator==),
                 form_facebook_.origin),
      &changes));
  CheckPasswordChanges(expected_changes, changes);

  EXPECT_EQ(2u, global_mock_libsecret_items->size());
  CheckStringAttribute((*global_mock_libsecret_items)[0].get(), "origin_url",
                       form_google_.origin.spec());
  CheckUint32Attribute((*global_mock_libsecret_items)[0].get(),
                       "should_skip_zero_click", 0);
  CheckStringAttribute((*global_mock_libsecret_items)[1].get(), "origin_url",
                       form_facebook_.origin.spec());
  CheckUint32Attribute((*global_mock_libsecret_items)[1].get(),
                       "should_skip_zero_click", 1);
}

TEST_F(NativeBackendLibsecretTest, SomeKeyringAttributesAreMissing) {
  // Absent attributes should be filled with default values.
  NativeBackendLibsecret backend(42);

  VerifiedAdd(&backend, form_google_);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  // Remove a string attribute.
  (*global_mock_libsecret_items)[0]->RemoveAttribute("avatar_url");
  // Remove an integer attribute.
  (*global_mock_libsecret_items)[0]->RemoveAttribute("times_used");

  std::vector<std::unique_ptr<PasswordForm>> form_list;
  EXPECT_TRUE(backend.GetAutofillableLogins(&form_list));

  EXPECT_EQ(1u, form_list.size());
  EXPECT_EQ(GURL(""), form_list[0]->icon_url);
  EXPECT_EQ(0, form_list[0]->times_used);
}

TEST_F(NativeBackendLibsecretTest, ReadDuplicateForms) {
  NativeBackendLibsecret backend(42);

  // Add 2 slightly different password forms.
  const char unique_string[] = "unique_unique_string";
  const char unique_string_replacement[] = "uniKue_unique_string";
  form_google_.origin =
      GURL(std::string("http://www.google.com/") + unique_string);
  VerifiedAdd(&backend, form_google_);
  form_google_.origin =
      GURL(std::string("http://www.google.com/") + unique_string_replacement);
  VerifiedAdd(&backend, form_google_);

  // Read the raw value back. Change the |unique_string| to
  // |unique_string_replacement| so the forms become unique.
  ASSERT_EQ(2u, global_mock_libsecret_items->size());
  gpointer item_value = g_hash_table_lookup(
      global_mock_libsecret_items->front()->attributes, "origin_url");
  ASSERT_TRUE(item_value);
  char* substr = strstr(static_cast<char*>(item_value), unique_string);
  ASSERT_TRUE(substr);
  ASSERT_EQ(strlen(unique_string), strlen(unique_string_replacement));
  strncpy(substr, unique_string_replacement, strlen(unique_string));

  // Now test that GetAutofillableLogins returns only one form.
  std::vector<std::unique_ptr<PasswordForm>> form_list;
  EXPECT_TRUE(backend.GetAutofillableLogins(&form_list));

  EXPECT_EQ(1u, form_list.size());
  EXPECT_EQ(form_google_, *form_list[0]);

  EXPECT_EQ(1u, global_mock_libsecret_items->size());
  if (!global_mock_libsecret_items->empty()) {
    CheckMockSecretItem((*global_mock_libsecret_items)[0].get(), form_google_,
                        "chrome-42");
  }
}

// TODO(mdm): add more basic tests here at some point.
