// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_process_manager.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/chromeos/nearby/nearby_process_manager_impl.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/webrtc_signaling_messenger.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sharing/webrtc/ice_config_fetcher.h"
#include "chrome/browser/sharing/webrtc/sharing_mojo_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/network_context_client_base.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "device/bluetooth/adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"

namespace {

ProfileAttributesEntry* GetStoredNearbyProfile() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return nullptr;

  base::FilePath advertising_profile_path =
      local_state->GetFilePath(::prefs::kNearbySharingActiveProfilePrefName);
  if (advertising_profile_path.empty())
    return nullptr;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  ProfileAttributesEntry* entry;
  if (!storage.GetProfileAttributesWithPath(advertising_profile_path, &entry)) {
    // Stored profile path is invalid so remove it.
    local_state->ClearPref(::prefs::kNearbySharingActiveProfilePrefName);
    return nullptr;
  }
  return entry;
}

void SetStoredNearbyProfile(Profile* profile) {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return;

  if (profile) {
    local_state->SetFilePath(::prefs::kNearbySharingActiveProfilePrefName,
                             profile->GetPath());
  } else {
    local_state->ClearPref(::prefs::kNearbySharingActiveProfilePrefName);
  }
}

bool IsStoredNearbyProfile(Profile* profile) {
  ProfileAttributesEntry* entry = GetStoredNearbyProfile();
  if (!entry)
    return profile == nullptr;
  return profile && entry->GetPath() == profile->GetPath();
}

}  // namespace

// static
NearbyProcessManager& NearbyProcessManager::GetInstance() {
  static base::NoDestructor<NearbyProcessManager> instance;
  return *instance;
}

void NearbyProcessManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyProcessManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ProfileAttributesEntry* NearbyProcessManager::GetActiveProfile() const {
  return GetStoredNearbyProfile();
}

bool NearbyProcessManager::IsActiveProfile(Profile* profile) const {
  // If the active profile is not loaded yet, try looking in prefs.
  if (!active_profile_)
    return IsStoredNearbyProfile(profile);

  return active_profile_ == profile;
}

bool NearbyProcessManager::IsAnyProfileActive() const {
  return !IsActiveProfile(/*profile=*/nullptr);
}

void NearbyProcessManager::SetActiveProfile(Profile* profile) {
  if (IsActiveProfile(profile))
    return;

  active_profile_ = profile;
  SetStoredNearbyProfile(active_profile_);
  StopProcess(active_profile_);

  for (auto& observer : observers_)
    observer.OnNearbyProfileChanged(profile);
}

void NearbyProcessManager::ClearActiveProfile() {
  SetActiveProfile(/*profile=*/nullptr);
}

location::nearby::connections::mojom::NearbyConnections*
NearbyProcessManager::GetOrStartNearbyConnections(Profile* profile) {
  if (!IsActiveProfile(profile))
    return nullptr;

  EnsureProcessIsRunning();
  return reference_->GetNearbyConnections().get();
}

sharing::mojom::NearbySharingDecoder*
NearbyProcessManager::GetOrStartNearbySharingDecoder(Profile* profile) {
  if (!IsActiveProfile(profile))
    return nullptr;

  EnsureProcessIsRunning();
  return reference_->GetNearbySharingDecoder().get();
}

void NearbyProcessManager::StopProcess(Profile* profile) {
  if (!IsActiveProfile(profile))
    return;

  EnsureNearbyProcessReferenceReleased();
}

void NearbyProcessManager::OnProfileAdded(Profile* profile) {
  // Cache active |profile| once it loads so we don't have to check prefs.
  if (IsActiveProfile(profile))
    active_profile_ = profile;
}

void NearbyProcessManager::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {
  if (IsActiveProfile(profile))
    SetActiveProfile(nullptr);
}

NearbyProcessManager::NearbyProcessManager() {
  // profile_manager() might be null in tests or during shutdown.
  if (auto* manager = g_browser_process->profile_manager())
    manager->AddObserver(this);
}

NearbyProcessManager::~NearbyProcessManager() {
  if (auto* manager = g_browser_process->profile_manager())
    manager->RemoveObserver(this);
}

void NearbyProcessManager::EnsureProcessIsRunning() {
  DCHECK(IsAnyProfileActive());

  // A reference already exists; the process is active.
  if (reference_)
    return;

  chromeos::nearby::NearbyProcessManager* process_manager =
      chromeos::nearby::NearbyProcessManagerFactory::GetForProfile(
          active_profile_);
  DCHECK(process_manager);

  NS_LOG(INFO) << "Initializing Nearby Share process reference.";

  // Note: base::Unretained(this) is used because this is a singleton.
  reference_ = process_manager->GetNearbyProcessReference(base::BindOnce(
      &NearbyProcessManager::OnNearbyProcessStopped, base::Unretained(this)));
  DCHECK(reference_);

  for (auto& observer : observers_)
    observer.OnNearbyProcessStarted();
}

void NearbyProcessManager::OnNearbyProcessStopped() {
  NS_LOG(INFO) << "Nearby process has stopped.";
  EnsureNearbyProcessReferenceReleased();
}

void NearbyProcessManager::EnsureNearbyProcessReferenceReleased() {
  if (!reference_)
    return;

  NS_LOG(INFO) << "Releasing Nearby Share process reference.";
  reference_.reset();

  for (auto& observer : observers_)
    observer.OnNearbyProcessStopped();
}
