// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_SESSION_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_SESSION_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"

namespace ash {

class ASH_PUBLIC_EXPORT LobsterSession {
 public:
  using StatusCallback = base::OnceCallback<void(bool)>;

  virtual ~LobsterSession() = default;

  virtual void DownloadCandidate(int candidate_id, StatusCallback) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_SESSION_H_
