// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_

#include "base/feature_list.h"
#include "components/signin/public/base/signin_buildflags.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
extern const base::Feature kDiceWebSigninInterceptionFeature;
#endif  // ENABLE_DICE_SUPPORT

extern const base::Feature kProcessGaiaRemoveLocalAccountHeader;

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_
