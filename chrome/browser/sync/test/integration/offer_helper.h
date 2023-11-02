// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_OFFER_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_OFFER_HELPER_H_

namespace autofill {
class AutofillOfferData;
}  // namespace autofill

namespace sync_pb {
class SyncEntity;
}  // namespace sync_pb

namespace offer_helper {

sync_pb::SyncEntity CreateDefaultSyncCardLinkedOffer();

sync_pb::SyncEntity CreateSyncCardLinkedOffer(
    const autofill::AutofillOfferData& offer_data);

}  // namespace offer_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_OFFER_HELPER_H_
