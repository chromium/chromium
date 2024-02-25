// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/tpm_challenge_key_with_timeout.h"

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"

namespace ash {
namespace attestation {

TpmChallengeKeyWithTimeout::TpmChallengeKeyWithTimeout() = default;
TpmChallengeKeyWithTimeout::~TpmChallengeKeyWithTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TpmChallengeKeyWithTimeout::BuildResponse(
    base::TimeDelta timeout,
    ::attestation::VerifiedAccessFlow flow_type,
    Profile* profile,
    TpmChallengeKeyCallback callback,
    const std::string& challenge,
    bool register_key,
    ::attestation::KeyType key_crypto_type,
    const std::string& key_name_for_spkac,
    const std::optional<std::string>& signals) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TpmChallengeKeyWithTimeout::ResolveCallback,
                     weak_factory_.GetWeakPtr(),
                     TpmChallengeKeyResult::MakeError(
                         TpmChallengeKeyResultCode::kTimeoutError)),
      timeout);

  challenger_ = TpmChallengeKeyFactory::Create();
  challenger_->BuildResponse(
      flow_type, profile,
      base::BindOnce(&TpmChallengeKeyWithTimeout::ResolveCallback,
                     weak_factory_.GetWeakPtr()),
      challenge, register_key, key_crypto_type, key_name_for_spkac, signals);
}

void TpmChallengeKeyWithTimeout::ResolveCallback(
    const TpmChallengeKeyResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_factory_.InvalidateWeakPtrs();
  challenger_.reset();

  std::move(callback_).Run(result);
  // No more member accesses here: |this| could be deleted at this point.
}

}  // namespace attestation
}  // namespace ash
