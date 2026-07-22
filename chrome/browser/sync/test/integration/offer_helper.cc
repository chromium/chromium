// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/offer_helper.h"

#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/autofill_offer_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/test/fake_server.h"

using autofill::AutofillOfferData;
using autofill::SetAutofillOfferSpecificsFromOfferData;
using autofill::test::GetCardLinkedOfferData1;
using sync_pb::AutofillOfferSpecifics;
using sync_pb::SyncEntity;

namespace offer_helper {

void SetOfferData(fake_server::FakeServer* fake_server,
                  const std::vector<SyncEntity>& offer_entities) {
  fake_server->DeleteAllEntitiesForDataType(syncer::AUTOFILL_WALLET_OFFER);
  for (const SyncEntity& entity : offer_entities) {
    fake_server->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/entity.name(),
            /*client_tag=*/entity.id_string(), entity.specifics(),
            /*creation_time=*/entity.ctime(),
            /*last_modified_time=*/entity.mtime()));
  }
}

SyncEntity CreateDefaultSyncCardLinkedOffer() {
  return CreateSyncCardLinkedOffer(GetCardLinkedOfferData1());
}

SyncEntity CreateSyncCardLinkedOffer(const AutofillOfferData& offer_data) {
  SyncEntity entity;
  entity.set_name(base::NumberToString(offer_data.GetOfferId()));
  entity.set_id_string(base::NumberToString(offer_data.GetOfferId()));
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  AutofillOfferSpecifics* offer_specifics =
      entity.mutable_specifics()->mutable_autofill_offer();
  SetAutofillOfferSpecificsFromOfferData(offer_data, offer_specifics);
  return entity;
}

}  // namespace offer_helper
