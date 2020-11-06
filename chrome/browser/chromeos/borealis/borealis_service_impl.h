// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_IMPL_H_

#include "chrome/browser/chromeos/borealis/borealis_app_launcher.h"
#include "chrome/browser/chromeos/borealis/borealis_service.h"

#include "chrome/browser/chromeos/borealis/borealis_features.h"

namespace borealis {

class BorealisServiceImpl : public BorealisService {
 public:
  explicit BorealisServiceImpl(Profile* profile);

  ~BorealisServiceImpl() override;

 private:
  // BorealisService overrides.
  BorealisFeatures& Features() override;
  BorealisAppLauncher& AppLauncher() override;

  Profile* const profile_;

  BorealisFeatures features_;
  BorealisAppLauncher app_launcher_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_IMPL_H_
