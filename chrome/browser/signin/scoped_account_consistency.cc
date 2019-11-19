// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/scoped_account_consistency.h"

#include <map>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "components/signin/public/base/signin_buildflags.h"

using signin::AccountConsistencyMethod;

ScopedAccountConsistency::ScopedAccountConsistency(
    AccountConsistencyMethod method) {
  DCHECK_NE(AccountConsistencyMethod::kDisabled, method);
#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
  DCHECK_NE(AccountConsistencyMethod::kDice, method);
  DCHECK_NE(AccountConsistencyMethod::kDiceMigration, method);
#endif

#if BUILDFLAG(ENABLE_MIRROR)
  DCHECK_EQ(AccountConsistencyMethod::kMirror, method);
  return;
#endif

  // Set up the account consistency method.
  std::string feature_value;
  switch (method) {
    case AccountConsistencyMethod::kDisabled:
      NOTREACHED();
      break;
    case AccountConsistencyMethod::kMirror:
      feature_value = kAccountConsistencyFeatureMethodMirror;
      break;
    case AccountConsistencyMethod::kDiceMigration:
      feature_value = kAccountConsistencyFeatureMethodDiceMigration;
      break;
    case AccountConsistencyMethod::kDice:
      feature_value = kAccountConsistencyFeatureMethodDice;
      break;
  }

  std::map<std::string, std::string> feature_params;
  feature_params[kAccountConsistencyFeatureMethodParameter] = feature_value;

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kAccountConsistencyFeature, feature_params);
}

ScopedAccountConsistency::~ScopedAccountConsistency() {}
