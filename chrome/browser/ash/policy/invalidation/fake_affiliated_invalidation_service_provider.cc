// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/invalidation/fake_affiliated_invalidation_service_provider.h"

namespace policy {

FakeAffiliatedInvalidationServiceProvider::
FakeAffiliatedInvalidationServiceProvider() {
}

void FakeAffiliatedInvalidationServiceProvider::RegisterConsumer(
    Consumer* consumer) {
}

void FakeAffiliatedInvalidationServiceProvider::UnregisterConsumer(
    Consumer* consumer) {
}

void FakeAffiliatedInvalidationServiceProvider::Shutdown() {
}

}  // namespace policy
