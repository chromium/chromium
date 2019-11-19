// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/hash/sha1.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class ListValue;
class Value;
}

// This class represents the content of a supervised user whitelist. It is
// loaded from a JSON file inside the extension bundle, which defines the sites
// on the list.
// All whitelists are combined in the SupervisedUserURLFilter, which can tell
// for a given URL if it is part of any whitelist. Effectively,
// SupervisedUserURLFilter then acts as a big whitelist which is the union of
// all the whitelists.
class SupervisedUserSiteList
    : public base::RefCountedThreadSafe<SupervisedUserSiteList> {
 public:
  class HostnameHash {
   public:
    explicit HostnameHash(const std::string& hostname);
    // |bytes| must have a size of at least |base::kSHA1Length|.
    explicit HostnameHash(const std::vector<uint8_t>& bytes);
    HostnameHash(const HostnameHash& other);

    bool operator==(const HostnameHash& rhs) const;

    // Returns a hash code suitable for putting this into hash maps.
    size_t hash() const;

   private:
    std::array<uint8_t, base::kSHA1Length> bytes_;
    // Copy and assign are allowed.
  };

  using LoadedCallback =
      base::Callback<void(const scoped_refptr<SupervisedUserSiteList>&)>;

  // Asynchronously loads the site list from |file| and calls |callback| with
  // the newly created object.
  static void Load(const std::string& id,
                   const base::string16& title,
                   const base::FilePath& large_icon_path,
                   const base::FilePath& file,
                   const LoadedCallback& callback);

  const std::string& id() const { return id_; }
  const base::string16& title() const { return title_; }
  const GURL& entry_point() const { return entry_point_; }
  const base::FilePath& large_icon_path() const { return large_icon_path_; }
  const std::vector<std::string>& patterns() const { return patterns_; }
  const std::vector<HostnameHash>& hostname_hashes() const {
    return hostname_hashes_;
  }

 private:
  friend class base::RefCountedThreadSafe<SupervisedUserSiteList>;
  friend class SupervisedUserURLFilterTest;
  FRIEND_TEST_ALL_PREFIXES(SupervisedUserURLFilterTest, WhitelistsPatterns);
  FRIEND_TEST_ALL_PREFIXES(SupervisedUserURLFilterTest,
                           WhitelistsHostnameHashes);

  SupervisedUserSiteList(const std::string& id,
                         const base::string16& title,
                         const GURL& entry_point,
                         const base::FilePath& large_icon_path,
                         const base::ListValue* patterns,
                         const base::ListValue* hostname_hashes);
  // Used for testing.
  SupervisedUserSiteList(const std::string& id,
                         const base::string16& title,
                         const GURL& entry_point,
                         const base::FilePath& large_icon_path,
                         const std::vector<std::string>& patterns,
                         const std::vector<std::string>& hostname_hashes);
  ~SupervisedUserSiteList();

  // Static private so it can access the private constructor.
  static void OnJsonLoaded(
      const std::string& id,
      const base::string16& title,
      const base::FilePath& large_icon_path,
      const base::FilePath& path,
      base::TimeTicks start_time,
      const SupervisedUserSiteList::LoadedCallback& callback,
      std::unique_ptr<base::Value> value);

  std::string id_;
  base::string16 title_;
  GURL entry_point_;
  base::FilePath large_icon_path_;

  // A list of URL patterns that should be whitelisted.
  std::vector<std::string> patterns_;

  // A list of SHA1 hashes of hostnames that should be whitelisted.
  std::vector<HostnameHash> hostname_hashes_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserSiteList);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_
