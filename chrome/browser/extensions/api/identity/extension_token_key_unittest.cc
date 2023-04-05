// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/extension_token_key.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

TEST(IdentityExtensionTokenKeyTest, Ordering) {
  std::string extension_id1("ext_id_1");
  std::string extension_id2("ext_id_2");
  std::set<std::string> scopes1;
  std::set<std::string> scopes2;
  std::set<std::string> scopes3;

  scopes1.insert("a");
  scopes1.insert("b");
  scopes2.insert("a");

  std::vector<std::string> extension_ids;
  extension_ids.push_back(extension_id1);
  extension_ids.push_back(extension_id2);

  std::vector<CoreAccountInfo> user_infos;

  CoreAccountInfo user_1;
  user_1.account_id = CoreAccountId::FromGaiaId("user_id_1");
  user_1.gaia = "user_id_1";
  user_1.email = "user_email_1";

  CoreAccountInfo user_2;
  user_2.account_id = CoreAccountId::FromGaiaId("user_id_2");
  user_2.gaia = "user_id_2";
  user_2.email = "user_email_2";

  user_infos.push_back(user_1);
  user_infos.push_back(user_2);

  std::vector<std::set<std::string> > scopesets;
  scopesets.push_back(scopes1);
  scopesets.push_back(scopes2);
  scopesets.push_back(scopes3);

  std::vector<extensions::ExtensionTokenKey> keys;
  typedef std::vector<extensions::ExtensionTokenKey>::const_iterator
      ExtensionTokenKeyIterator;

  for (const auto& extension_id : extension_ids) {
    for (const auto& user_info : user_infos) {
      for (const auto& scopes : scopesets) {
        keys.emplace_back(extension_id, user_info, scopes);
      }
    }
  }

  // keys should not be less than themselves
  for (ExtensionTokenKeyIterator it = keys.begin(); it != keys.end(); ++it) {
    EXPECT_FALSE(*it < *it);
  }

  // comparison should establish an ordering
  std::sort(keys.begin(), keys.end());
  for (ExtensionTokenKeyIterator it1 = keys.begin(); it1 != keys.end(); ++it1) {
    auto it2 = it1;
    for (++it2; it2 != keys.end(); ++it2) {
      EXPECT_LT(*it1, *it2);
      EXPECT_FALSE(it2 < it1);
    }
  }
}
