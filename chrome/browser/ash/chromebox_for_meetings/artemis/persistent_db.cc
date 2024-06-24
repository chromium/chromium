// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/artemis/persistent_db.h"

#include "base/check.h"

namespace ash::cfm {

namespace {

static PersistentDb* g_persistent_db = nullptr;

}  // namespace

// static
void PersistentDb::Initialize() {
  CHECK(!g_persistent_db);
  g_persistent_db = new PersistentDb();
}

// static
void PersistentDb::InitializeForTesting(PersistentDb* db) {
  CHECK(!g_persistent_db);
  g_persistent_db = db;
}

// static
void PersistentDb::Shutdown() {
  CHECK(g_persistent_db);
  delete g_persistent_db;
  g_persistent_db = nullptr;
}

// static
void PersistentDb::ShutdownForTesting() {
  CHECK(g_persistent_db);
  // Assumes db was cleaned up by caller!
  g_persistent_db = nullptr;
}

// static
PersistentDb* PersistentDb::Get() {
  CHECK(g_persistent_db) << "PersistentDb::Get() called before Initialize()";
  return g_persistent_db;
}

// static
bool PersistentDb::IsInitialized() {
  return g_persistent_db;
}

PersistentDb::PersistentDb() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PersistentDb::~PersistentDb() = default;

int PersistentDb::GetValueFromKey(int key, int default_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/322505142)
  return default_value;
}

void PersistentDb::SaveValueToKey(int key, int value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/322505142)
  (void)key;
  (void)value;
}

void PersistentDb::DeleteKeyIfExists(int key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/322505142)
  (void)key;
}

size_t PersistentDb::GetSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/322505142)
  return 0u;
}

}  // namespace ash::cfm
