// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_PROCESS_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_PROCESS_MANAGER_H_

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/services/nearby/public/mojom/sharing.mojom.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;
class ProfileAttributesEntry;

// Interfaces with the Nearby utility process.
// TODO(https://crbug.com/1137664): This class should be renamed or deleted
// altogether. It was written before chromeos::nearby::NerabyProcessManager was
// created.
class NearbyProcessManager : public ProfileManagerObserver {
 public:
  using NearbyConnectionsMojom =
      location::nearby::connections::mojom::NearbyConnections;
  using NearbySharingDecoderMojom = sharing::mojom::NearbySharingDecoder;

  // Returns an instance to the singleton of this class. This is used from
  // multiple BCKS and only allows the first one to launch a process.
  static NearbyProcessManager& GetInstance();

  // Observes the global state of the NearbyProcessManager.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the |profile| got set as the active profile.
    virtual void OnNearbyProfileChanged(Profile* profile) = 0;
    // Called when the Nearby process has started. This happens after a profile
    // called one of the GetOrStart*() methods.
    virtual void OnNearbyProcessStarted() = 0;
    // Called when the Nearby process has stopped. This can happen when the
    // process gets stopped to switch to a different profile or when the process
    // gets killed by the system.
    virtual void OnNearbyProcessStopped() = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gets the entry for the currently active profile or nullptr if no profile is
  // set. We return a ProfileAttributesEntry instead of a Profile as the active
  // profile might not be loaded yet and we do not want to load it here.
  ProfileAttributesEntry* GetActiveProfile() const;

  // Returns whether the |profile| is the active profile to use the Nearby
  // process. Convenience method to calling GetActiveProfile() and manually
  // comparing if they match.
  virtual bool IsActiveProfile(Profile* profile) const;

  // Returns if any profile is currently set as the active profile. Note that
  // the active profile might not be loaded yet.
  bool IsAnyProfileActive() const;

  // Starts an exclusive usage of the Nearby process for the given |profile|.
  // This will stop the process if it is currently running for a different
  // profile. After calling this the client may call any of the GetOrStart*()
  // methods below to start up a new sandboxed process.
  void SetActiveProfile(Profile* profile);

  // Removes any stored active profile. This will stop the process if it is
  // currently running for that profile.
  void ClearActiveProfile();

  // Gets a pointer to the Nearby Connections interface. If there is currently
  // no process running this will start a new sandboxed process. This will
  // only work if the |profile| is currently set as the active profile.
  // Returns a mojo interface to the Nearby Connections library inside the
  // sandbox if this |profile| is allowed to access it and nullptr otherwise.
  // Don't store this pointer as it might get invalid if the process gets
  // stopped (via the OS or StopProcess()). That event can be observed via
  // Observer::OnNearbyProcessStopped() and a client can decide to restart the
  // process (e.g. via backoff timer) if it is still the active profile.
  virtual NearbyConnectionsMojom* GetOrStartNearbyConnections(Profile* profile);

  // Gets a pointer to the Nearby Decoder interface. Starts a new process if
  // there is none running already or reuses an existing one. The same
  // limitations around profiles and lifetime in GetOrStartNearbyConnections()
  // apply here as well.
  virtual NearbySharingDecoderMojom* GetOrStartNearbySharingDecoder(
      Profile* profile);

  // Stops the Nearby process if the |profile| is the active profile. This may
  // be used to save resources or to force stop any communication of the
  // Nearby Connections library if it should not be used right now. This will
  // not change the active profile and can be used to temporarily stop the
  // process (e.g. on screen lock) while keeping the active profile.
  virtual void StopProcess(Profile* profile);

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NearbyProcessManagerTest, AddRemoveObserver);
  FRIEND_TEST_ALL_PREFIXES(NearbySharingServiceImplTest,
                           AddsNearbyProcessObserver);
  FRIEND_TEST_ALL_PREFIXES(NearbySharingServiceImplTest,
                           RemovesNearbyProcessObserver);
  friend class base::NoDestructor<NearbyProcessManager>;
  friend class MockNearbyProcessManager;

  // This class is a singleton.
  NearbyProcessManager();
  ~NearbyProcessManager() override;

  void EnsureProcessIsRunning();
  void OnNearbyProcessStopped();
  void EnsureNearbyProcessReferenceReleased();

  // Reference to the Nearby utility process; if null, no reference is currently
  // held.
  std::unique_ptr<
      chromeos::nearby::NearbyProcessManager::NearbyProcessReference>
      reference_;

  // All registered observers, typically one per loaded profile.
  base::ObserverList<Observer> observers_;
  // Profile using the Nearby process. This might be nullptr if the active
  // profile has not been loaded yet.
  Profile* active_profile_ = nullptr;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_PROCESS_MANAGER_H_
