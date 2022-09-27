// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/apps_model_type_controller.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/model/syncable_service_based_bridge.h"
#include "extensions/browser/extension_system.h"

using syncer::ClientTagBasedModelTypeProcessor;
using syncer::ForwardingModelTypeControllerDelegate;
using syncer::ModelTypeControllerDelegate;
using syncer::SyncableServiceBasedBridge;

// static
std::unique_ptr<AppsModelTypeController> AppsModelTypeController::Create(
    syncer::OnceModelTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service,
    const base::RepeatingClosure& dump_stack,
    Profile* profile) {
  // Create the bridge here so it can be used to construct a forwarding delegate
  // passed to the superclass constructor.
  auto bridge = std::make_unique<SyncableServiceBasedBridge>(
      syncer::APPS, std::move(store_factory),
      std::make_unique<ClientTagBasedModelTypeProcessor>(syncer::APPS,
                                                         dump_stack),
      syncable_service.get());
  ModelTypeControllerDelegate* delegate =
      bridge->change_processor()->GetControllerDelegate().get();
  return base::WrapUnique(new AppsModelTypeController(
      std::move(bridge), /*delegate_for_full_sync_mode=*/
      std::make_unique<ForwardingModelTypeControllerDelegate>(delegate),
      profile));
}

AppsModelTypeController::~AppsModelTypeController() = default;

AppsModelTypeController::AppsModelTypeController(
    std::unique_ptr<syncer::ModelTypeSyncBridge> bridge,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    Profile* profile)
    : syncer::ModelTypeController(syncer::APPS,
                                  std::move(delegate_for_full_sync_mode)),
      bridge_(std::move(bridge)),
      profile_(profile) {
  DCHECK(profile_);
}

void AppsModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  extensions::ExtensionSystem::Get(profile_)->InitForRegularProfile(
      /*extensions_enabled=*/true);
  ModelTypeController::LoadModels(configure_context, model_load_callback);
}
