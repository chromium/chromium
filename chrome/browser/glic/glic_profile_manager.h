// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_
#define CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class Profile;

namespace glic {

// GlicProfileManager is a GlobalFeature that manages multi-profile Glic state.
// Among other things it is used for determining which profile to launch from an
// OS Entry point and ensuring that just one panel is shown across all profiles.
class GlicProfileManager : public ProfileManagerObserver {
 public:
  GlicProfileManager();
  ~GlicProfileManager() override;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLastActiveGlicProfileChanged(Profile* profile) = 0;
  };

  // Returns the global instance.
  static GlicProfileManager* GetInstance();

  GlicProfileManager(const GlicProfileManager&) = delete;
  GlicProfileManager& operator=(const GlicProfileManager&) = delete;

  // Return the profile that should be used to open glic. May be null if there
  // is no eligible profile.
  Profile* GetProfileForLaunch() const;

  // Called by GlicKeyedService.
  void SetActiveGlic(GlicKeyedService* glic);

  // Called by GlicKeyedService.
  void OnServiceShutdown(GlicKeyedService* glic);

  // Called by GlobalFeatures.
  void Shutdown();

  // Called when the web client for the GlicWindowController or the FRE
  // controller will be torn down.
  void OnLoadingClientForService(GlicKeyedService* glic);

  // Called by GlicWindowController and the GlicFreController when their
  // respective web clients are being torn down.
  void OnUnloadingClientForService(GlicKeyedService* glic);

  using ShouldPreloadCallback = base::OnceCallback<void(Profile*, bool)>;

  // Callback will be invoked with true if the given profile should be
  // considered for preloading.
  void ShouldPreloadForProfile(Profile* profile,
                               ShouldPreloadCallback callback);

  // Callback will be invoked with true if the given profile should be
  // considered for preloading the FRE.
  void ShouldPreloadFreForProfile(Profile* profile,
                                  ShouldPreloadCallback callback);

  // Returns the active Glic service, nullptr if there is none.
  GlicKeyedService* GetLastActiveGlic() const;

  // Opens the panel if the "glic-open-on-startup" command line switch was used
  // and glic has not already opened like this.
  void MaybeAutoOpenGlicPanel();

  void ShowProfilePicker();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsShowing() const;

  // ProfileManagerObserver:
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;

  // Static in order to permit setting forced values before the manager is
  // constructed.
  static void ForceProfileForLaunchForTesting(std::optional<Profile*> profile);
  static void ForceMemoryPressureForTesting(
      std::optional<base::MemoryPressureMonitor::MemoryPressureLevel> level);
  static void ForceConnectionTypeForTesting(
      std::optional<network::mojom::ConnectionType> type);

  base::WeakPtr<GlicProfileManager> GetWeakPtr();

 private:
  // Callback from ProfilePicker::Show().
  void DidSelectProfile(Profile* profile);

  bool IsUnderMemoryPressure() const;

  // Checks whether preloading is possible for the profile for either the fre
  // or the glic panel (i.e., this excludes specific checks for those two
  // surfaces).
  void CanPreloadForProfile(Profile* profile, ShouldPreloadCallback callback);

  bool IsLastActiveGlicProfile(Profile* profile) const;
  bool IsLastLoadedGlicProfile(Profile* profile) const;

  base::ObserverList<Observer> observers_;
  base::WeakPtr<GlicKeyedService> last_active_glic_;
  base::WeakPtr<GlicKeyedService> last_loaded_glic_;
  bool did_auto_open_ = false;
  base::WeakPtrFactory<GlicProfileManager> weak_ptr_factory_{this};
};
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_
