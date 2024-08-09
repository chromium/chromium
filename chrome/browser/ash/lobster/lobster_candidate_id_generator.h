// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_CANDIDATE_ID_GENERATOR_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_CANDIDATE_ID_GENERATOR_H_

#include <stdint.h>

class LobsterCandidateIdGenerator {
 public:
  LobsterCandidateIdGenerator();
  ~LobsterCandidateIdGenerator();

  uint32_t GenerateNextId();

 private:
  uint32_t current_id_ = 0;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_CANDIDATE_ID_GENERATOR_H_
