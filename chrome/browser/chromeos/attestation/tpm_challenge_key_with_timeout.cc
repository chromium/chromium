// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/tpm_challenge_key_with_timeout.h"

#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace chromeos {
namespace attestation {

TpmChallengeKeyWithTimeout::TpmChallengeKeyWithTimeout() = default;
TpmChallengeKeyWithTimeout::~TpmChallengeKeyWithTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TpmChallengeKeyWithTimeout::BuildResponse(
    base::TimeDelta timeout,
    AttestationKeyType key_type,
    Profile* profile,
    TpmChallengeKeyCallback callback,
    const std::string& challenge,
    bool register_key,
    const std::string& key_name_for_spkac) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TpmChallengeKeyWithTimeout::ResolveCallback,
                 weak_factory_.GetWeakPtr(),
                 TpmChallengeKeyResult::MakeError(
                     TpmChallengeKeyResultCode::kTimeoutError)),
      timeout);

  challenger_ = TpmChallengeKeyFactory::Create();
  challenger_->BuildResponse(
      key_type, profile,
      base::BindOnce(&TpmChallengeKeyWithTimeout::ResolveCallback,
                     weak_factory_.GetWeakPtr()),
      challenge, register_key, key_name_for_spkac);
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
}  // namespace chromeos
