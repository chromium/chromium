// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_service_impl.h"

#include "chrome/browser/chromeos/borealis/borealis_features.h"
#include "chrome/browser/profiles/profile.h"

namespace borealis {

BorealisServiceImpl::BorealisServiceImpl(Profile* profile)
    : profile_(profile), features_(profile_) {}

BorealisServiceImpl::~BorealisServiceImpl() = default;

BorealisFeatures& BorealisServiceImpl::Features() {
  return features_;
}

}  // namespace borealis
