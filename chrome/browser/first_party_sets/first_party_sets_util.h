// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_UTIL_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_UTIL_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"

class FirstPartySetsUtil {
 public:
  static FirstPartySetsUtil* GetInstance();

  // Note that FirstPartySetsUtil is a singleton that's never destroyed.
  ~FirstPartySetsUtil() = delete;
  FirstPartySetsUtil(const FirstPartySetsUtil&) = delete;
  FirstPartySetsUtil& operator=(const FirstPartySetsUtil&) = delete;

  // This method reads the persisted First-Party Sets from the file under
  // `user_data_dir`, then invokes `send_sets` with the read data (could be
  // empty) and with a callback that should eventually be invoked with the
  // current First-Party Sets (encoded as a string). The callback writes the
  // current First-Party Sets to the file in `user_data_dir`.
  void SendAndUpdatePersistedSets(
      const base::FilePath& user_data_dir,
      base::OnceCallback<void(base::OnceCallback<void(const std::string&)>,
                              const std::string&)> send_sets);

 private:
  friend class base::NoDestructor<FirstPartySetsUtil>;

  FirstPartySetsUtil() = default;

  // Called when the instance receives the "current" First-Party Sets.
  // Asynchronously writes those sets to disk.
  void OnGetUpdatedSets(const base::FilePath& path, const std::string& sets);

  // Sends `sets` via `send_sets`, and sets up a callback to overwrite the
  // on-disk sets. `send_sets` takes a callback (which is expected to be invoked
  // with the merged First-Party Sets, when ready) and the persisted sets.
  void SendPersistedSets(
      base::OnceCallback<void(base::OnceCallback<void(const std::string&)>,
                              const std::string&)> send_sets,
      const base::FilePath& path,
      const std::string& sets);

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_UTIL_H_
