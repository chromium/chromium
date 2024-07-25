// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_PERSISTENT_DB_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_PERSISTENT_DB_H_

#include <stddef.h>

#include "base/sequence_checker.h"

namespace ash::cfm {

// This is an abstraction around a persistent cache that will
// assist in crash recovery for incremental sources (eg logs).
// Note that this Db is explicitly a mapping of ints to ints.
//
// Not sequence safe! Access to the db should be localized to
// a single sequence.
class PersistentDb {
 public:
  PersistentDb();
  virtual ~PersistentDb();
  PersistentDb(const PersistentDb&) = delete;
  PersistentDb& operator=(const PersistentDb&) = delete;

  // Manage singleton instance.
  static void Initialize();
  static void InitializeForTesting(PersistentDb* db);
  static void Shutdown();
  static void ShutdownForTesting();
  static PersistentDb* Get();
  static bool IsInitialized();

  // This db acts like a dict, so provide dict-like get & save methods.
  virtual int GetValueFromKey(int key, int default_value);
  virtual void SaveValueToKey(int key, int value);
  virtual void DeleteKeyIfExists(int key);
  virtual size_t GetSize() const;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_PERSISTENT_DB_H_
