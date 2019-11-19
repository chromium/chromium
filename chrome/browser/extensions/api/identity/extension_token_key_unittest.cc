// Copyright 2013 The Chromium Authors. All rights reserved.
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

  std::vector<CoreAccountId> user_ids;
  user_ids.push_back(CoreAccountId("user_id_1"));
  user_ids.push_back(CoreAccountId("user_id_2"));

  std::vector<std::set<std::string> > scopesets;
  scopesets.push_back(scopes1);
  scopesets.push_back(scopes2);
  scopesets.push_back(scopes3);

  std::vector<extensions::ExtensionTokenKey> keys;
  typedef std::vector<extensions::ExtensionTokenKey>::const_iterator
      ExtensionTokenKeyIterator;

  std::vector<std::string>::const_iterator extension_it;
  std::vector<CoreAccountId>::const_iterator user_it;
  std::vector<std::set<std::string> >::const_iterator scope_it;

  for (extension_it = extension_ids.begin();
       extension_it != extension_ids.end();
       ++extension_it) {
    for (user_it = user_ids.begin(); user_it != user_ids.end(); ++user_it) {
      for (scope_it = scopesets.begin(); scope_it != scopesets.end();
           ++scope_it) {
        keys.push_back(
            extensions::ExtensionTokenKey(*extension_it, *user_it, *scope_it));
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
