// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_BLACKLIST_H_
#define CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_BLACKLIST_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/hash/sha1.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace base {
class FilePath;
}

class GURL;

// Compact list of (SHA1 hashes of) blocked hosts.
// Checking for URLs is thread-safe, loading is not.
class SupervisedUserBlacklist {
 public:
  struct Hash {
    Hash() {}
    explicit Hash(const std::string& host);
    bool operator<(const Hash& rhs) const;

    unsigned char data[base::kSHA1Length];
  };

  SupervisedUserBlacklist();
  ~SupervisedUserBlacklist();

  // Asynchronously read a blacklist from the given file, replacing any previous
  // entries. |done_callback| will be run after reading finishes (successfully
  // or not), but not if the SupervisedUserBlacklist is destroyed before that.
  void ReadFromFile(const base::FilePath& path,
                    const base::Closure& done_callback);

  bool HasURL(const GURL& url) const;

  size_t GetEntryCount() const;

 private:
  void OnReadFromFileCompleted(const base::Closure& done_callback,
                               std::unique_ptr<std::vector<Hash>> host_hashes);

  std::vector<Hash> host_hashes_;

  base::WeakPtrFactory<SupervisedUserBlacklist> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserBlacklist);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_BLACKLIST_H_
