// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/supervised/supervised_user_constants.h"

#include "chromeos/cryptohome/cryptohome_parameters.h"

namespace chromeos {

const char kCryptohomeSupervisedUserKeyLabel[] = "managed";
const char kLegacyCryptohomeSupervisedUserKeyLabel[] = "default-0";

const int kCryptohomeSupervisedUserKeyPrivileges =
    cryptohome::PRIV_AUTHORIZED_UPDATE | cryptohome::PRIV_MOUNT;
const int kCryptohomeSupervisedUserIncompleteKeyPrivileges =
    cryptohome::PRIV_MIGRATE | cryptohome::PRIV_MOUNT;

}  // namespace chromeos
