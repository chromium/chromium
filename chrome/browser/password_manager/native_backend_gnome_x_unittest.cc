// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <map>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/native_backend_gnome_x.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
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

// What follows is a very simple implementation of the subset of the GNOME
// Keyring API that we actually use. It gets substituted for the real one by
// MockGnomeKeyringLoader, which hooks into the facility normally used to load
// the GNOME Keyring library at runtime to avoid a static dependency on it.

struct MockKeyringItem {
  MockKeyringItem() {}
  MockKeyringItem(const char* keyring,
                  const std::string& display_name,
                  const std::string& password)
    : keyring(keyring ? keyring : "login"),
      display_name(display_name),
      password(password) {}

  struct ItemAttribute {
    ItemAttribute() : type(UINT32), value_uint32(0) {}
    explicit ItemAttribute(uint32_t value)
      : type(UINT32), value_uint32(value) {}
    explicit ItemAttribute(const std::string& value)
      : type(STRING), value_string(value) {}

    bool Equals(const ItemAttribute& x) const {
      if (type != x.type) return false;
      return (type == STRING) ? value_string == x.value_string
                              : value_uint32 == x.value_uint32;
    }

    enum Type { UINT32, STRING } type;
    uint32_t value_uint32;
    std::string value_string;
  };

  typedef std::map<std::string, ItemAttribute> attribute_map;
  typedef std::vector<std::pair<std::string, ItemAttribute> > attribute_query;

  bool Matches(const attribute_query& query) const {
    // The real GNOME Keyring doesn't match empty queries.
    if (query.empty()) return false;
    for (size_t i = 0; i < query.size(); ++i) {
      auto match = attributes.find(query[i].first);
      if (match == attributes.end()) return false;
      if (!match->second.Equals(query[i].second)) return false;
    }
    return true;
  }

  std::string keyring;
  std::string display_name;
  std::string password;

  attribute_map attributes;
};

// The list of all keyring items we have stored.
std::vector<MockKeyringItem> mock_keyring_items;
bool mock_keyring_reject_local_ids = false;

bool IsStringAttribute(const GnomeKeyringPasswordSchema* schema,
                       const std::string& name) {
  for (size_t i = 0; schema->attributes[i].name; ++i)
    if (name == schema->attributes[i].name)
      return schema->attributes[i].type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING;
  NOTREACHED() << "Requested type of nonexistent attribute";
  return false;
}

gboolean mock_gnome_keyring_is_available() {
  return true;
}

gpointer mock_gnome_keyring_store_password(
    const GnomeKeyringPasswordSchema* schema,
    const gchar* keyring,
    const gchar* display_name,
    const gchar* password,
    GnomeKeyringOperationDoneCallback callback,
    gpointer data,
    GDestroyNotify destroy_data,
    ...) {
  mock_keyring_items.push_back(
      MockKeyringItem(keyring, display_name, password));
  MockKeyringItem* item = &mock_keyring_items.back();
  const std::string keyring_desc =
      keyring ? base::StringPrintf("keyring %s", keyring)
              : std::string("default keyring");
  VLOG(1) << "Adding item with origin " << display_name
          << " to " << keyring_desc;
  va_list ap;
  va_start(ap, destroy_data);
  char* name;
  while ((name = va_arg(ap, gchar*))) {
    if (IsStringAttribute(schema, name)) {
      item->attributes[name] =
          MockKeyringItem::ItemAttribute(va_arg(ap, gchar*));
      VLOG(1) << "Adding item attribute " << name
              << ", value '" << item->attributes[name].value_string << "'";
    } else {
      item->attributes[name] =
          MockKeyringItem::ItemAttribute(va_arg(ap, uint32_t));
      VLOG(1) << "Adding item attribute " << name
              << ", value " << item->attributes[name].value_uint32;
    }
  }
  va_end(ap);
  // As a hack to ease testing migration, make it possible to reject the new
  // format for the app string. This way we can add them easily to migrate.
  if (mock_keyring_reject_local_ids) {
    auto it = item->attributes.find("application");
    if (it != item->attributes.end() &&
        it->second.type == MockKeyringItem::ItemAttribute::STRING &&
        base::StringPiece(it->second.value_string).starts_with("chrome-")) {
      mock_keyring_items.pop_back();
      // GnomeKeyringResult, data
      callback(GNOME_KEYRING_RESULT_IO_ERROR, data);
      return nullptr;
    }
  }
  // GnomeKeyringResult, data
  callback(GNOME_KEYRING_RESULT_OK, data);
  return nullptr;
}

gpointer mock_gnome_keyring_delete_password(
    const GnomeKeyringPasswordSchema* schema,
    GnomeKeyringOperationDoneCallback callback,
    gpointer data,
    GDestroyNotify destroy_data,
    ...) {
  MockKeyringItem::attribute_query query;
  va_list ap;
  va_start(ap, destroy_data);
  char* name;
  while ((name = va_arg(ap, gchar*))) {
    if (IsStringAttribute(schema, name)) {
      query.push_back(make_pair(std::string(name),
          MockKeyringItem::ItemAttribute(va_arg(ap, gchar*))));
      VLOG(1) << "Querying with item attribute " << name
              << ", value '" << query.back().second.value_string << "'";
    } else {
      query.push_back(make_pair(std::string(name),
          MockKeyringItem::ItemAttribute(va_arg(ap, uint32_t))));
      VLOG(1) << "Querying with item attribute " << name
              << ", value " << query.back().second.value_uint32;
    }
  }
  va_end(ap);
  bool deleted = false;
  for (size_t i = mock_keyring_items.size(); i > 0; --i) {
    const MockKeyringItem* item = &mock_keyring_items[i - 1];
    if (item->Matches(query)) {
      VLOG(1) << "Deleting item with origin " <<  item->display_name;
      mock_keyring_items.erase(mock_keyring_items.begin() + (i - 1));
      deleted = true;
    }
  }
  // GnomeKeyringResult, data
  callback(deleted ? GNOME_KEYRING_RESULT_OK
                   : GNOME_KEYRING_RESULT_NO_MATCH, data);
  return nullptr;
}

gpointer mock_gnome_keyring_find_items(
    GnomeKeyringItemType type,
    GnomeKeyringAttributeList* attributes,
    GnomeKeyringOperationGetListCallback callback,
    gpointer data,
    GDestroyNotify destroy_data) {
  MockKeyringItem::attribute_query query;
  for (size_t i = 0; i < attributes->len; ++i) {
    GnomeKeyringAttribute attribute =
        g_array_index(attributes, GnomeKeyringAttribute, i);
    if (attribute.type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) {
      query.push_back(
          make_pair(std::string(attribute.name),
                    MockKeyringItem::ItemAttribute(attribute.value.string)));
      VLOG(1) << "Querying with item attribute " << attribute.name
              << ", value '" << query.back().second.value_string << "'";
    } else {
      query.push_back(
          make_pair(std::string(attribute.name),
                    MockKeyringItem::ItemAttribute(attribute.value.integer)));
      VLOG(1) << "Querying with item attribute " << attribute.name << ", value "
              << query.back().second.value_uint32;
    }
  }
  // Find matches and add them to a list of results.
  GList* results = nullptr;
  for (size_t i = 0; i < mock_keyring_items.size(); ++i) {
    const MockKeyringItem* item = &mock_keyring_items[i];
    if (item->Matches(query)) {
      GnomeKeyringFound* found = new GnomeKeyringFound;
      found->keyring = strdup(item->keyring.c_str());
      found->item_id = i;
      found->attributes = gnome_keyring_attribute_list_new();
      for (auto it = item->attributes.begin(); it != item->attributes.end();
           ++it) {
        if (it->second.type == MockKeyringItem::ItemAttribute::STRING) {
          gnome_keyring_attribute_list_append_string(
              found->attributes, it->first.c_str(),
              it->second.value_string.c_str());
        } else {
          gnome_keyring_attribute_list_append_uint32(
              found->attributes, it->first.c_str(),
              it->second.value_uint32);
        }
      }
      found->secret = strdup(item->password.c_str());
      results = g_list_prepend(results, found);
    }
  }
  // GnomeKeyringResult, GList*, data
  callback(results ? GNOME_KEYRING_RESULT_OK
                   : GNOME_KEYRING_RESULT_NO_MATCH, results, data);
  // Now free the list of results.
  GList* element = g_list_first(results);
  while (element) {
    GnomeKeyringFound* found = static_cast<GnomeKeyringFound*>(element->data);
    free(found->keyring);
    gnome_keyring_attribute_list_free(found->attributes);
    free(found->secret);
    delete found;
    element = g_list_next(element);
  }
  g_list_free(results);
  return nullptr;
}

const gchar* mock_gnome_keyring_result_to_message(GnomeKeyringResult res) {
  return "mock keyring simulating failure";
}

// Inherit to get access to protected fields.
class MockGnomeKeyringLoader : public GnomeKeyringLoader {
 public:
  static bool LoadMockGnomeKeyring() {
    // Mocked methods
    gnome_keyring_is_available_ptr = &mock_gnome_keyring_is_available;
    gnome_keyring_store_password_ptr = &mock_gnome_keyring_store_password;
    gnome_keyring_delete_password_ptr = &mock_gnome_keyring_delete_password;
    gnome_keyring_find_items_ptr = &mock_gnome_keyring_find_items;
    gnome_keyring_result_to_message_ptr = &mock_gnome_keyring_result_to_message;
    // Non-mocked methods
    gnome_keyring_attribute_list_free_ptr =
        &::gnome_keyring_attribute_list_free;
    gnome_keyring_attribute_list_new_ptr = &::gnome_keyring_attribute_list_new;
    gnome_keyring_attribute_list_append_string_ptr =
        &::gnome_keyring_attribute_list_append_string;
    gnome_keyring_attribute_list_append_uint32_ptr =
        &::gnome_keyring_attribute_list_append_uint32;

    keyring_loaded = true;
    // Reset the state of the mock library.
    mock_keyring_items.clear();
    mock_keyring_reject_local_ids = false;
    return true;
  }
};

void CheckPasswordChanges(const PasswordStoreChangeList& expected_list,
                          const PasswordStoreChangeList& actual_list) {
  ASSERT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < expected_list.size(); ++i) {
    EXPECT_EQ(expected_list[i].type(), actual_list[i].type());
    const PasswordForm& expected = expected_list[i].form();
    const PasswordForm& actual = actual_list[i].form();

    EXPECT_EQ(expected.origin, actual.origin);
    EXPECT_EQ(expected.password_value, actual.password_value);
    EXPECT_EQ(expected.action, actual.action);
    EXPECT_EQ(expected.username_element, actual.username_element);
    EXPECT_EQ(expected.username_value, actual.username_value);
    EXPECT_EQ(expected.password_element, actual.password_element);
    EXPECT_EQ(expected.submit_element, actual.submit_element);
    EXPECT_EQ(expected.signon_realm, actual.signon_realm);
    EXPECT_EQ(expected.preferred, actual.preferred);
    EXPECT_EQ(expected.date_created, actual.date_created);
    EXPECT_EQ(expected.blacklisted_by_user, actual.blacklisted_by_user);
    EXPECT_EQ(expected.type, actual.type);
    EXPECT_EQ(expected.times_used, actual.times_used);
    EXPECT_EQ(expected.scheme, actual.scheme);
    EXPECT_EQ(expected.date_synced, actual.date_synced);
    EXPECT_EQ(expected.display_name, actual.display_name);
    EXPECT_EQ(expected.icon_url, actual.icon_url);
    EXPECT_EQ(expected.federation_origin.Serialize(),
              actual.federation_origin.Serialize());
    EXPECT_EQ(expected.skip_zero_click, actual.skip_zero_click);
    EXPECT_EQ(expected.generation_upload_status,
              actual.generation_upload_status);
  }
}

void CheckPasswordChangesWithResult(const PasswordStoreChangeList* expected,
                                    const PasswordStoreChangeList* actual,
                                    bool result) {
  EXPECT_TRUE(result);
  CheckPasswordChanges(*expected, *actual);
}

void CheckTrue(bool result) {
  EXPECT_TRUE(result);
}

}  // anonymous namespace

class NativeBackendGnomeTest : public testing::Test {
 protected:
  enum UpdateType {  // Used in CheckPSLUpdate().
    UPDATE_BY_UPDATELOGIN,
    UPDATE_BY_ADDLOGIN,
  };
  enum RemoveBetweenMethod {  // Used in CheckRemoveLoginsBetween().
    CREATED,
    SYNCED,
  };

  NativeBackendGnomeTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    ASSERT_TRUE(MockGnomeKeyringLoader::LoadMockGnomeKeyring());

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

  void CheckUint32Attribute(const MockKeyringItem* item,
                            const std::string& attribute,
                            uint32_t value) {
    auto it = item->attributes.find(attribute);
    EXPECT_NE(item->attributes.end(), it);
    if (it != item->attributes.end()) {
      EXPECT_EQ(MockKeyringItem::ItemAttribute::UINT32, it->second.type);
      EXPECT_EQ(value, it->second.value_uint32);
    }
  }

  void CheckStringAttribute(const MockKeyringItem* item,
                            const std::string& attribute,
                            const std::string& value) {
    auto it = item->attributes.find(attribute);
    EXPECT_NE(item->attributes.end(), it);
    if (it != item->attributes.end()) {
      EXPECT_EQ(MockKeyringItem::ItemAttribute::STRING, it->second.type);
      EXPECT_EQ(value, it->second.value_string);
    }
  }

  void CheckMockKeyringItem(const MockKeyringItem* item,
                            const PasswordForm& form,
                            const std::string& app_string) {
    // We always add items to the login keyring.
    EXPECT_EQ("login", item->keyring);
    EXPECT_EQ(form.origin.spec(), item->display_name);
    EXPECT_EQ(UTF16ToUTF8(form.password_value), item->password);
    EXPECT_EQ(21u, item->attributes.size());
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
    CheckStringAttribute(item, "date_synced", base::Int64ToString(
        form.date_synced.ToInternalValue()));
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
        item->attributes.at("form_data").value_string, &actual);
    EXPECT_TRUE(form.form_data.SameFormAs(actual));
  }

  // Saves |credentials| and then gets logins matching |url| and |scheme|.
  // Returns true when something is found, and in such case copies the result to
  // |result| when |result| is not NULL. (Note that there can be max. 1 result,
  // derived from |credentials|.)
  bool CheckCredentialAvailability(const PasswordForm& credentials,
                                   const GURL& url,
                                   const PasswordForm::Scheme& scheme,
                                   PasswordForm* result) {
    NativeBackendGnome backend(321);
    backend.Init();

    backend.GetBackgroundTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                       base::Unretained(&backend), credentials));

    PasswordStore::FormDigest target_form = {scheme, url.spec(), url};
    if (scheme != PasswordForm::SCHEME_HTML) {
      // For non-HTML forms, the realm used for authentication
      // (http://tools.ietf.org/html/rfc1945#section-10.2) is appended to the
      // signon_realm. Just use a default value for now.
      target_form.signon_realm.append("Realm");
    }
    std::vector<std::unique_ptr<PasswordForm>> form_list;
    base::PostTaskAndReplyWithResult(
        backend.GetBackgroundTaskRunner().get(), FROM_HERE,
        base::Bind(&NativeBackendGnome::GetLogins, base::Unretained(&backend),
                   target_form, &form_list),
        base::Bind(&CheckTrue));

    scoped_task_environment_.RunUntilIdle();

    EXPECT_EQ(1u, mock_keyring_items.size());
    if (mock_keyring_items.size() > 0)
      CheckMockKeyringItem(&mock_keyring_items[0], credentials, "chrome-321");
    mock_keyring_items.clear();

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
    NativeBackendGnome backend(321);
    backend.Init();

    // Add |form_facebook_| to saved logins.
    base::PostTaskAndReplyWithResult(
        backend.GetBackgroundTaskRunner().get(), FROM_HERE,
        base::Bind(&NativeBackendGnome::AddLogin, base::Unretained(&backend),
                   form_facebook_),
        base::Bind(&CheckPasswordChanges,
                   PasswordStoreChangeList(
                       1, PasswordStoreChange(PasswordStoreChange::ADD,
                                              form_facebook_))));

    // Get the PSL-matched copy of the saved login for m.facebook.
    const GURL kMobileURL("http://m.facebook.com/");
    PasswordStore::FormDigest m_facebook_lookup = {
        PasswordForm::SCHEME_HTML, kMobileURL.spec(), kMobileURL};
    std::vector<std::unique_ptr<PasswordForm>> form_list;
    base::PostTaskAndReplyWithResult(
        backend.GetBackgroundTaskRunner().get(), FROM_HERE,
        base::Bind(&NativeBackendGnome::GetLogins, base::Unretained(&backend),
                   m_facebook_lookup, &form_list),
        base::Bind(&CheckTrue));

    scoped_task_environment_.RunUntilIdle();

    EXPECT_EQ(1u, mock_keyring_items.size());
    EXPECT_EQ(1u, form_list.size());
    PasswordForm m_facebook = *form_list[0];
    form_list.clear();
    m_facebook.origin = kMobileURL;
    m_facebook.signon_realm = kMobileURL.spec();

    // Add the PSL-matched copy to saved logins.
    backend.GetBackgroundTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                       base::Unretained(&backend), m_facebook));

    scoped_task_environment_.RunUntilIdle();

    EXPECT_EQ(2u, mock_keyring_items.size());

    // Update www.facebook.com login.
    PasswordForm new_facebook(form_facebook_);
    const base::string16 kOldPassword(form_facebook_.password_value);
    const base::string16 kNewPassword(UTF8ToUTF16("new_b"));
    EXPECT_NE(kOldPassword, kNewPassword);
    new_facebook.password_value = kNewPassword;
    PasswordStoreChangeList changes;
    PasswordStoreChangeList expected_changes;
    switch (update_type) {
      case UPDATE_BY_UPDATELOGIN:
        expected_changes.push_back(
            PasswordStoreChange(PasswordStoreChange::UPDATE, new_facebook));
        base::PostTaskAndReplyWithResult(
            backend.GetBackgroundTaskRunner().get(), FROM_HERE,
            base::Bind(&NativeBackendGnome::UpdateLogin,
                       base::Unretained(&backend), new_facebook, &changes),
            base::Bind(&CheckPasswordChangesWithResult, &expected_changes,
                       &changes));
        break;
      case UPDATE_BY_ADDLOGIN:
        expected_changes.push_back(
            PasswordStoreChange(PasswordStoreChange::REMOVE, form_facebook_));
        expected_changes.push_back(
            PasswordStoreChange(PasswordStoreChange::ADD, new_facebook));
        base::PostTaskAndReplyWithResult(
            backend.GetBackgroundTaskRunner().get(), FROM_HERE,
            base::Bind(&NativeBackendGnome::AddLogin,
                       base::Unretained(&backend), new_facebook),
            base::Bind(&CheckPasswordChanges, expected_changes));
        break;
    }

    scoped_task_environment_.RunUntilIdle();

    EXPECT_EQ(2u, mock_keyring_items.size());

    // Check that m.facebook.com login was not modified by the update.
    base::PostTaskAndReplyWithResult(
        backend.GetBackgroundTaskRunner().get(), FROM_HERE,
        base::Bind(&NativeBackendGnome::GetLogins, base::Unretained(&backend),
                   m_facebook_lookup, &form_list),
        base::Bind(&CheckTrue));

    scoped_task_environment_.RunUntilIdle();

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
    base::PostTaskAndReplyWithResult(
        backend.GetBackgroundTaskRunner().get(), FROM_HERE,
        base::Bind(&NativeBackendGnome::GetLogins, base::Unretained(&backend),
                   PasswordStore::FormDigest(form_facebook_), &form_list),
        base::Bind(&CheckTrue));

    scoped_task_environment_.RunUntilIdle();

    // There should be two results -- the exact one, and the PSL-matched one.
    EXPECT_EQ(2u, form_list.size());
    index_non_psl = 0;
    if (form_list[index_non_psl]->is_public_suffix_match)
      index_non_psl = 1;
    EXPECT_EQ(form_facebook_.origin, form_list[index_non_psl]->origin);
    EXPECT_EQ(form_facebook_.signon_realm,
              form_list[index_non_psl]->signon_realm);
    EXPECT_EQ(kNewPassword, form_list[index_non_psl]->password_value);
  }

  void CheckMatchingWithScheme(const PasswordForm::Scheme& scheme) {
    other_auth_.scheme = scheme;

    // Don't match a non-HTML form with an HTML form.
    EXPECT_FALSE(CheckCredentialAvailability(
        other_auth_, GURL("http://www.example.com"),
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
    NativeBackendGnome backend(42);
    backend.Init();

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

    backend.GetBackgroundTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                       base::Unretained(&backend), form_google_));
    backend.GetBackgroundTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                       base::Unretained(&backend), form_isc_));

    PasswordStoreChangeList expected_changes;
    expected_changes.push_back(
        PasswordStoreChange(PasswordStoreChange::REMOVE, form_google_));
    PasswordStoreChangeList changes;
    bool (NativeBackendGnome::*method)(
        base::Time, base::Time, password_manager::PasswordStoreChangeList*) =
        date_to_test == CREATED
            ? &NativeBackendGnome::RemoveLoginsCreatedBetween
            : &NativeBackendGnome::RemoveLoginsSyncedBetween;
    base::PostTaskAndReplyWithResult(
        backend.GetBackgroundTaskRunner().get(), FROM_HERE,
        base::Bind(method, base::Unretained(&backend), base::Time(), next_day,
                   &changes),
        base::Bind(&CheckPasswordChangesWithResult, &expected_changes,
                   &changes));

    scoped_task_environment_.RunUntilIdle();

    EXPECT_EQ(1u, mock_keyring_items.size());
    if (mock_keyring_items.size() > 0)
      CheckMockKeyringItem(&mock_keyring_items[0], form_isc_, "chrome-42");

    // Remove form_isc_.
    expected_changes.clear();
    expected_changes.push_back(
        PasswordStoreChange(PasswordStoreChange::REMOVE, form_isc_));
    base::PostTaskAndReplyWithResult(
        backend.GetBackgroundTaskRunner().get(), FROM_HERE,
        base::Bind(method, base::Unretained(&backend), next_day, base::Time(),
                   &changes),
        base::Bind(&CheckPasswordChangesWithResult, &expected_changes,
                   &changes));

    scoped_task_environment_.RunUntilIdle();

    EXPECT_EQ(0u, mock_keyring_items.size());
  }

  // Create the ScopedTaskEnvirnment first to ensure that
  // CreateSequencedTaskRunnerWithTraits will work correctly. Then create also
  // TestBrowserThreadBundle so that BrowserThread::UI has an associated
  // TaskRunner. The order is important because the bundle can detect that the
  // MessageLoop used by the environment exists and reuse it, but not vice
  // versa.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  // Provide some test forms to avoid having to set them up in each test.
  PasswordForm form_google_;
  PasswordForm form_facebook_;
  PasswordForm form_isc_;
  PasswordForm other_auth_;
};

TEST_F(NativeBackendGnomeTest, BasicAddLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::AddLogin, base::Unretained(&backend),
                 form_google_),
      base::Bind(
          &CheckPasswordChanges,
          PasswordStoreChangeList(
              1, PasswordStoreChange(PasswordStoreChange::ADD, form_google_))));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, BasicListLogins) {
  NativeBackendGnome backend(42);
  backend.Init();

  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_google_));

  std::vector<std::unique_ptr<PasswordForm>> form_list;
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::GetAutofillableLogins,
                 base::Unretained(&backend), &form_list),
      base::Bind(&CheckTrue));

  scoped_task_environment_.RunUntilIdle();

  // Quick check that we got something back.
  EXPECT_EQ(1u, form_list.size());

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

// Save a password for www.facebook.com and see it suggested for m.facebook.com.
TEST_F(NativeBackendGnomeTest, PSLMatchingPositive) {
  PasswordForm result;
  const GURL kMobileURL("http://m.facebook.com/");
  EXPECT_TRUE(CheckCredentialAvailability(
      form_facebook_, kMobileURL, PasswordForm::SCHEME_HTML, &result));
  EXPECT_EQ(form_facebook_.origin, result.origin);
  EXPECT_EQ(form_facebook_.signon_realm, result.signon_realm);
}

// Save a password for www.facebook.com and see it not suggested for
// m-facebook.com.
TEST_F(NativeBackendGnomeTest, PSLMatchingNegativeDomainMismatch) {
  EXPECT_FALSE(CheckCredentialAvailability(
      form_facebook_, GURL("http://m-facebook.com/"),
      PasswordForm::SCHEME_HTML, nullptr));
}

// Test PSL matching is off for domains excluded from it.
TEST_F(NativeBackendGnomeTest, PSLMatchingDisabledDomains) {
  EXPECT_FALSE(CheckCredentialAvailability(
      form_google_, GURL("http://one.google.com/"),
      PasswordForm::SCHEME_HTML, nullptr));
}

// Make sure PSL matches aren't available for non-HTML forms.
TEST_F(NativeBackendGnomeTest, PSLMatchingDisabledForNonHTMLForms) {
  CheckMatchingWithScheme(PasswordForm::SCHEME_BASIC);
  CheckMatchingWithScheme(PasswordForm::SCHEME_DIGEST);
  CheckMatchingWithScheme(PasswordForm::SCHEME_OTHER);
}

TEST_F(NativeBackendGnomeTest, PSLUpdatingStrictUpdateLogin) {
  CheckPSLUpdate(UPDATE_BY_UPDATELOGIN);
}

TEST_F(NativeBackendGnomeTest, PSLUpdatingStrictAddLogin) {
  // TODO(vabr): if AddLogin becomes no longer valid for existing logins, then
  // just delete this test.
  CheckPSLUpdate(UPDATE_BY_ADDLOGIN);
}

TEST_F(NativeBackendGnomeTest, FetchFederatedCredentialOnHTTPS) {
  other_auth_.signon_realm = "federation://www.example.com/google.com";
  other_auth_.origin = GURL("https://www.example.com/");
  other_auth_.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  EXPECT_TRUE(CheckCredentialAvailability(other_auth_,
                                          GURL("https://www.example.com/"),
                                          PasswordForm::SCHEME_HTML, nullptr));
}

TEST_F(NativeBackendGnomeTest, FetchFederatedCredentialOnLocalhost) {
  other_auth_.signon_realm = "federation://localhost/google.com";
  other_auth_.origin = GURL("http://localhost:8080/");
  other_auth_.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  EXPECT_TRUE(CheckCredentialAvailability(other_auth_,
                                          GURL("http://localhost:8080/"),
                                          PasswordForm::SCHEME_HTML, nullptr));
}

TEST_F(NativeBackendGnomeTest, DontFetchFederatedCredentialOnHTTP) {
  other_auth_.signon_realm = "federation://www.example.com/google.com";
  other_auth_.origin = GURL("https://www.example.com/");
  other_auth_.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  EXPECT_FALSE(CheckCredentialAvailability(other_auth_,
                                           GURL("http://www.example.com/"),
                                           PasswordForm::SCHEME_HTML, nullptr));
}

TEST_F(NativeBackendGnomeTest, FetchPSLMatchedFederatedCredentialOnHTTPS) {
  other_auth_.signon_realm = "federation://www.sub.example.com/google.com";
  other_auth_.origin = GURL("https://www.sub.example.com/");
  other_auth_.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  EXPECT_TRUE(CheckCredentialAvailability(other_auth_,
                                          GURL("https://www.example.com/"),
                                          PasswordForm::SCHEME_HTML, nullptr));
}

TEST_F(NativeBackendGnomeTest, DontFetchPSLMatchedFederatedCredentialOnHTTP) {
  other_auth_.signon_realm = "federation://www.sub.example.com/google.com";
  other_auth_.origin = GURL("https://www.sub.example.com/");
  other_auth_.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  EXPECT_FALSE(CheckCredentialAvailability(other_auth_,
                                           GURL("http://www.example.com/"),
                                           PasswordForm::SCHEME_HTML, nullptr));
}

TEST_F(NativeBackendGnomeTest, BasicUpdateLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  // First add google login.
  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_google_));

  scoped_task_environment_.RunUntilIdle();

  PasswordForm new_form_google(form_google_);
  new_form_google.times_used = 1;
  new_form_google.action = GURL("http://www.google.com/different/login");

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");

  // Update login
  PasswordStoreChangeList changes;
  PasswordStoreChangeList expected_changes(
      1, PasswordStoreChange(PasswordStoreChange::UPDATE, new_form_google));
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::UpdateLogin, base::Unretained(&backend),
                 new_form_google, &changes),
      base::Bind(&CheckPasswordChangesWithResult, &expected_changes, &changes));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], new_form_google, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, BasicRemoveLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_google_));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");

  PasswordStoreChangeList changes;
  PasswordStoreChangeList expected_changes(
      1, PasswordStoreChange(PasswordStoreChange::REMOVE, form_google_));
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::RemoveLogin, base::Unretained(&backend),
                 form_google_, &changes),
      base::Bind(&CheckPasswordChangesWithResult, &expected_changes, &changes));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, mock_keyring_items.size());
}

// Verify fix for http://crbug.com/408783.
TEST_F(NativeBackendGnomeTest, RemoveLoginActionMismatch) {
  NativeBackendGnome backend(42);
  backend.Init();

  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_google_));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");

  // Action url match not required for removal.
  form_google_.action = GURL("https://some.other.url.com/path");

  PasswordStoreChangeList changes;
  PasswordStoreChangeList expected_changes(
      1, PasswordStoreChange(PasswordStoreChange::REMOVE, form_google_));
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::RemoveLogin, base::Unretained(&backend),
                 form_google_, &changes),
      base::Bind(&CheckPasswordChangesWithResult, &expected_changes, &changes));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, mock_keyring_items.size());
}

TEST_F(NativeBackendGnomeTest, RemoveNonexistentLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  // First add an unrelated login.
  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_google_));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");

  // Attempt to remove a login that doesn't exist.
  PasswordStoreChangeList changes;
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::RemoveLogin, base::Unretained(&backend),
                 form_isc_, &changes),
      base::Bind(&CheckPasswordChangesWithResult,
                 base::Owned(new PasswordStoreChangeList), &changes));

  // Make sure we can still get the first form back.
  std::vector<std::unique_ptr<PasswordForm>> form_list;
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::GetAutofillableLogins,
                 base::Unretained(&backend), &form_list),
      base::Bind(&CheckTrue));

  scoped_task_environment_.RunUntilIdle();

  // Quick check that we got something back.
  EXPECT_EQ(1u, form_list.size());

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, UpdateNonexistentLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  // First add an unrelated login.
  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_google_));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");

  // Attempt to update a login that doesn't exist.
  PasswordStoreChangeList changes;
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::UpdateLogin, base::Unretained(&backend),
                 form_isc_, &changes),
      base::Bind(&CheckPasswordChangesWithResult,
                 base::Owned(new PasswordStoreChangeList), &changes));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, UpdateSameLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  // First add an unrelated login.
  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_google_));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");

  // Attempt to update the same login without changing anything.
  PasswordStoreChangeList changes;
  PasswordStoreChangeList expected_changes;
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::UpdateLogin, base::Unretained(&backend),
                 form_google_, &changes),
      base::Bind(&CheckPasswordChangesWithResult, &expected_changes, &changes));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, AddDuplicateLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  PasswordStoreChangeList changes;
  changes.push_back(PasswordStoreChange(PasswordStoreChange::ADD,
                                        form_google_));
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::AddLogin, base::Unretained(&backend),
                 form_google_),
      base::Bind(&CheckPasswordChanges, changes));

  changes.clear();
  changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE,
                                        form_google_));
  form_google_.times_used++;
  form_google_.submit_element = UTF8ToUTF16("submit2");
  changes.push_back(PasswordStoreChange(PasswordStoreChange::ADD,
                                        form_google_));

  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::AddLogin, base::Unretained(&backend),
                 form_google_),
      base::Bind(&CheckPasswordChanges, changes));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, AndroidCredentials) {
  NativeBackendGnome backend(42);
  backend.Init();

  PasswordForm saved_android_form;
  saved_android_form.scheme = PasswordForm::SCHEME_HTML;
  saved_android_form.signon_realm =
      "android://7x7IDboo8u9YKraUsbmVkuf1-@net.rateflix.app/";
  saved_android_form.username_value = base::UTF8ToUTF16("randomusername");
  saved_android_form.password_value = base::UTF8ToUTF16("password");
  saved_android_form.date_created = base::Time::Now();

  PasswordStore::FormDigest observed_android_form(saved_android_form);
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::AddLogin, base::Unretained(&backend),
                 saved_android_form),
      base::Bind(&CheckPasswordChanges,
                 PasswordStoreChangeList(
                     1, PasswordStoreChange(PasswordStoreChange::ADD,
                                            saved_android_form))));

  std::vector<std::unique_ptr<PasswordForm>> form_list;
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::GetLogins, base::Unretained(&backend),
                 observed_android_form, &form_list),
      base::Bind(&CheckTrue));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, form_list.size());
  EXPECT_EQ(saved_android_form, *form_list[0]);
}

TEST_F(NativeBackendGnomeTest, RemoveLoginsCreatedBetween) {
  CheckRemoveLoginsBetween(CREATED);
}

TEST_F(NativeBackendGnomeTest, RemoveLoginsSyncedBetween) {
  CheckRemoveLoginsBetween(SYNCED);
}

TEST_F(NativeBackendGnomeTest, DisableAutoSignInForOrigins) {
  NativeBackendGnome backend(42);
  backend.Init();
  form_google_.skip_zero_click = false;
  form_facebook_.skip_zero_click = false;

  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_google_));
  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_facebook_));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(2u, mock_keyring_items.size());
  for (const auto& item : mock_keyring_items)
    CheckUint32Attribute(&item, "should_skip_zero_click", 0);

  // Set the canonical forms to the updated value for the following comparison.
  form_google_.skip_zero_click = true;
  form_facebook_.skip_zero_click = true;
  PasswordStoreChangeList expected_changes;
  expected_changes.push_back(
      PasswordStoreChange(PasswordStoreChange::UPDATE, form_facebook_));

  PasswordStoreChangeList changes;
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(
          &NativeBackendGnome::DisableAutoSignInForOrigins,
          base::Unretained(&backend),
          base::Bind(
              static_cast<bool (*)(const GURL&, const GURL&)>(operator==),
              form_facebook_.origin),
          &changes),
      base::Bind(&CheckPasswordChangesWithResult, &expected_changes, &changes));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(2u, mock_keyring_items.size());
  CheckStringAttribute(
      &mock_keyring_items[0], "origin_url", form_google_.origin.spec());
  CheckUint32Attribute(&mock_keyring_items[0], "should_skip_zero_click", 0);
  CheckStringAttribute(
      &mock_keyring_items[1], "origin_url", form_facebook_.origin.spec());
  CheckUint32Attribute(&mock_keyring_items[1], "should_skip_zero_click", 1);
}

TEST_F(NativeBackendGnomeTest, ReadDuplicateForms) {
  NativeBackendGnome backend(42);
  backend.Init();

  // Add 2 slightly different password forms.
  const char unique_string[] = "unique_unique_string";
  const char unique_string_replacement[] = "uniKue_unique_string";
  form_google_.origin =
      GURL(std::string("http://www.google.com/") + unique_string);
  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_google_));
  form_google_.origin =
      GURL(std::string("http://www.google.com/") + unique_string_replacement);
  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_google_));

  scoped_task_environment_.RunUntilIdle();

  // Read the raw value back. Change the |unique_string| to
  // |unique_string_replacement| so the forms become unique.
  ASSERT_EQ(2u, mock_keyring_items.size());
  auto it = mock_keyring_items[0].attributes.find("origin_url");
  ASSERT_NE(mock_keyring_items[0].attributes.end(), it);
  size_t position = it->second.value_string.find(unique_string);
  ASSERT_NE(std::string::npos, position) << it->second.value_string;
  it->second.value_string.replace(
      position, std::string(unique_string_replacement).length(),
      unique_string_replacement);

  // Now test that GetAutofillableLogins returns only one form.
  std::vector<std::unique_ptr<PasswordForm>> form_list;
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::GetAutofillableLogins,
                 base::Unretained(&backend), &form_list),
      base::Bind(&CheckTrue));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, form_list.size());
  EXPECT_EQ(form_google_, *form_list[0]);

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, GetAllLogins) {
  NativeBackendGnome backend(42);
  backend.Init();

  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_google_));

  backend.GetBackgroundTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                     base::Unretained(&backend), form_facebook_));

  std::vector<std::unique_ptr<PasswordForm>> form_list;
  base::PostTaskAndReplyWithResult(
      backend.GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&NativeBackendGnome::GetAllLogins, base::Unretained(&backend),
                 &form_list),
      base::Bind(&CheckTrue));

  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(2u, form_list.size());
  EXPECT_THAT(form_list, UnorderedElementsAre(Pointee(form_google_),
                                              Pointee(form_facebook_)));
}

// TODO(mdm): add more basic tests here at some point.
