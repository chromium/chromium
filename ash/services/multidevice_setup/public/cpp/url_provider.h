// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_URL_PROVIDER_H_
#define ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_URL_PROVIDER_H_

#include "url/gurl.h"

namespace ash {

namespace multidevice_setup {

GURL GetBoardSpecificBetterTogetherSuiteLearnMoreUrl();
GURL GetBoardSpecificMessagesLearnMoreUrl();

}  // namespace multidevice_setup

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos::multidevice_setup {
using ::ash::multidevice_setup::GetBoardSpecificBetterTogetherSuiteLearnMoreUrl;
using ::ash::multidevice_setup::GetBoardSpecificMessagesLearnMoreUrl;
}  // namespace chromeos::multidevice_setup

#endif  // ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_URL_PROVIDER_H_
