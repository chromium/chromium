// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_service.h"

#include "chrome/browser/ash/borealis/borealis_service_factory.h"

namespace borealis {

BorealisService* BorealisService::GetForProfile(Profile* profile) {
  return BorealisServiceFactory::GetForProfile(profile);
}

}  // namespace borealis
