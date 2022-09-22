// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_STORAGE_H_
#define CHROME_BROWSER_DIPS_DIPS_STORAGE_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_database.h"
#include "chrome/browser/dips/dips_state.h"

class GURL;

// Manages the storage of DIPSState values.
class DIPSStorage {
 public:
  DIPSStorage();
  ~DIPSStorage();

  void Init(const absl::optional<base::FilePath>& path);

  DIPSState Read(const GURL& url);

  // DIPS Helper Method Impls --------------------------------------------------

  // Record that |url| wrote to storage.
  void RecordStorage(const GURL& url, base::Time time, DIPSCookieMode mode);
  // Record that the user interacted on |url|.
  void RecordInteraction(const GURL& url, base::Time time, DIPSCookieMode mode);

  // Empty method intended for testing use only.
  void DoNothing() {}

 private:
  friend class DIPSState;
  void Write(const DIPSState& state);

  std::unique_ptr<DIPSDatabase> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_DIPS_DIPS_STORAGE_H_
