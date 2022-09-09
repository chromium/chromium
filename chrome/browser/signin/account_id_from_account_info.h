// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_ID_FROM_ACCOUNT_INFO_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_ID_FROM_ACCOUNT_INFO_H_

#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_info.h"

// Returns AccountID populated from |account_info|.
// NOTE: This utility is in //chrome rather than being part of
// //components/signin/public because it is only //chrome that needs to go back
// and forth between AccountId and AccountInfo, and it is outside the scope of
// //components/signin/public to have knowledge about AccountId.
AccountId AccountIdFromAccountInfo(const CoreAccountInfo& account_info);

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_ID_FROM_ACCOUNT_INFO_H_
