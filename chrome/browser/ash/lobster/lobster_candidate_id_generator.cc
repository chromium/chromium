// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_candidate_id_generator.h"

LobsterCandidateIdGenerator::LobsterCandidateIdGenerator() = default;

LobsterCandidateIdGenerator::~LobsterCandidateIdGenerator() = default;

uint32_t LobsterCandidateIdGenerator::GenerateNextId() {
  return current_id_++;
}
