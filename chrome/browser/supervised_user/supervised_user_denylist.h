// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_DENYLIST_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_DENYLIST_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/hash/sha1.h"
#include "base/memory/weak_ptr.h"

namespace base {
class FilePath;
}

class GURL;

// Compact list of (SHA1 hashes of) blocked hosts.
// Checking for URLs is thread-safe, loading is not.
class SupervisedUserDenylist {
 public:
  struct Hash {
    Hash() {}
    explicit Hash(const std::string& host);
    bool operator<(const Hash& rhs) const;

    unsigned char data[base::kSHA1Length];
  };

  SupervisedUserDenylist();

  SupervisedUserDenylist(const SupervisedUserDenylist&) = delete;
  SupervisedUserDenylist& operator=(const SupervisedUserDenylist&) = delete;

  ~SupervisedUserDenylist();

  // Asynchronously read a denylist from the given file, replacing any previous
  // entries. |done_callback| will be run after reading finishes (successfully
  // or not), but not if the SupervisedUserDenylist is destroyed before that.
  void ReadFromFile(const base::FilePath& path,
                    const base::RepeatingClosure& done_callback);

  bool HasURL(const GURL& url) const;

  size_t GetEntryCount() const;

 private:
  void OnReadFromFileCompleted(const base::RepeatingClosure& done_callback,
                               std::unique_ptr<std::vector<Hash>> host_hashes);

  std::vector<Hash> host_hashes_;

  base::WeakPtrFactory<SupervisedUserDenylist> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_DENYLIST_H_
