// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_ai_util.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/browser_context.h"

namespace extensions {
namespace {
template <class T>
base::Value::List ToValueList(const std::vector<T>& values) {
  base::Value::List list;
  list.reserve(values.size());
  for (const auto& value : values) {
    list.Append(value.ToValue());
  }
  return list;
}
}  // namespace

AutofillPrivateEventRouter::AutofillPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  event_router_ = EventRouter::Get(context_);
  if (!event_router_)
    return;

  personal_data_ =
      autofill::PersonalDataManagerFactory::GetForBrowserContext(context_);
  if (personal_data_) {
    pdm_observer_.Observe(personal_data_);
  }

  autofill::EntityDataManager* entity_data_manager =
      autofill::AutofillEntityDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(context_));
  if (entity_data_manager) {
    entity_data_manager_observer_.Observe(entity_data_manager);
  }

  if (syncer::SyncService* sync = SyncServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context_))) {
    sync_observer_.Observe(sync);
  }
}

AutofillPrivateEventRouter::~AutofillPrivateEventRouter() = default;

void AutofillPrivateEventRouter::Shutdown() {
  pdm_observer_.Reset();
  entity_data_manager_observer_.Reset();
  sync_observer_.Reset();
}

void AutofillPrivateEventRouter::RebindPersonalDataManagerForTesting(
    autofill::PersonalDataManager* personal_data) {
  pdm_observer_.Reset();
  personal_data_ = personal_data;
  if (personal_data_) {
    pdm_observer_.Observe(personal_data_);
  }
}

void AutofillPrivateEventRouter::UnbindPersonalDataManagerForTesting() {
  pdm_observer_.Reset();
  personal_data_ = nullptr;
}

void AutofillPrivateEventRouter::OnPersonalDataChanged() {
  BroadcastCurrentData();
}

void AutofillPrivateEventRouter::OnEntityInstancesChanged() {
  base::Value::List args;
  args.Append(ToValueList(
      extensions::autofill_ai_util::
          EntityInstancesToPrivateApiEntityInstancesWithLabels(
              entity_data_manager_observer_.GetSource()->GetEntityInstances(),
              g_browser_process->GetApplicationLocale())));

  std::unique_ptr<Event> extension_event = std::make_unique<Event>(
      events::AUTOFILL_PRIVATE_ON_ENTITY_INSTANCES_CHANGED,
      api::autofill_private::OnEntityInstancesChanged::kEventName,
      std::move(args));

  event_router_->BroadcastEvent(std::move(extension_event));
}

void AutofillPrivateEventRouter::OnStateChanged(syncer::SyncService*) {
  BroadcastCurrentData();
}

void AutofillPrivateEventRouter::BroadcastCurrentData() {
  // Ignore any updates before data is loaded. This can happen in tests.
  if (!(personal_data_ && personal_data_->IsDataLoaded()))
    return;

  autofill_util::AddressEntryList address_list =
      extensions::autofill_util::GenerateAddressList(
          personal_data_->address_data_manager());

  autofill_util::CreditCardEntryList credit_card_list =
      extensions::autofill_util::GenerateCreditCardList(
          personal_data_->payments_data_manager());

  autofill_util::IbanEntryList iban_list =
      extensions::autofill_util::GenerateIbanList(
          personal_data_->payments_data_manager());

  autofill_util::PayOverTimeIssuerEntryList pay_over_time_issuer_list =
      extensions::autofill_util::GeneratePayOverTimeIssuerList(
          personal_data_->payments_data_manager());

  std::optional<api::autofill_private::AccountInfo> account_info =
      extensions::autofill_util::GetAccountInfo(
          personal_data_->address_data_manager());

  base::Value::List args;
  args.Append(ToValueList(address_list));
  args.Append(ToValueList(credit_card_list));
  args.Append(ToValueList(iban_list));
  args.Append(ToValueList(pay_over_time_issuer_list));
  if (account_info.has_value()) {
    args.Append(account_info->ToValue());
  }

  std::unique_ptr<Event> extension_event = std::make_unique<Event>(
      events::AUTOFILL_PRIVATE_ON_PERSONAL_DATA_CHANGED,
      api::autofill_private::OnPersonalDataChanged::kEventName,
      std::move(args));

  event_router_->BroadcastEvent(std::move(extension_event));
}

}  // namespace extensions
