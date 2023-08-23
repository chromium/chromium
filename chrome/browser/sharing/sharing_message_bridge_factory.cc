// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_message_bridge_factory.h"
#include "chrome/browser/sharing/sharing_message_bridge_impl.h"

#include "base/no_destructor.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"

namespace {
constexpr char kServiceName[] = "SharingMessageBridge";
}  // namespace

SharingMessageBridgeFactory::SharingMessageBridgeFactory()
    : ProfileKeyedServiceFactory(
          kServiceName,
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

SharingMessageBridgeFactory::~SharingMessageBridgeFactory() = default;

// static
SharingMessageBridgeFactory* SharingMessageBridgeFactory::GetInstance() {
  static base::NoDestructor<SharingMessageBridgeFactory> instance;
  return instance.get();
}

// static
SharingMessageBridge* SharingMessageBridgeFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SharingMessageBridge*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

std::unique_ptr<KeyedService>
SharingMessageBridgeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::SHARING_MESSAGE,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  return std::make_unique<SharingMessageBridgeImpl>(
      std::move(change_processor));
}
