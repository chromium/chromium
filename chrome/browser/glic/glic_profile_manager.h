// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_
#define CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class Profile;

namespace glic {

enum class GlicPrewarmingChecksResult;

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

  // Called by GlicKeyedService. Closes any existing active glic in the
  // single-instance implementation, which enforces at most one floaty per
  // profile.
  void SetActiveGlic(GlicKeyedService* glic);

  // Used in GlicMultiInstance. Called when a GlicFloatingUi is shown and closes
  // any previous existing floating glic. Resets the tracked glic if a null
  // profile is passed.
  void SetCurrentDetachedGlic(Profile* profile);

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

  // Callback will be invoked with kSuccess if the given profile should be
  // considered for preloading.
  using ShouldPreloadCallback =
      base::OnceCallback<void(GlicPrewarmingChecksResult)>;
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
      std::optional<base::MemoryPressureLevel> level);
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
  // Used in GlicMultiInstance to track the GlicKeyedService of the current
  // detached glic, if any.
  base::WeakPtr<GlicKeyedService> current_detached_glic_;
  bool did_auto_open_ = false;
  base::WeakPtrFactory<GlicProfileManager> weak_ptr_factory_{this};
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync with
// GlicPrewarmingChecksResult in enums.xml.
// LINT.IfChange(GlicPrewarmingChecksResult)
enum class GlicPrewarmingChecksResult {
  // Preloading is happening.
  kSuccess = 0,

  // Warming was disabled by the feature configuration.
  kWarmingDisabled = 1,

  // The profile doesn't exist or is marked for deletion.
  kProfileGone = 2,

  // The profile is not ready for Glic, for an unknown reason.
  kProfileNotReadyUnknown = 3,

  // The account state is paused, and requires sign in.
  kProfileRequiresSignIn = 4,

  // The profile is not eligible for Glic.
  kProfileNotEligible = 5,

  // Glic is not rolled out to the user.
  kProfileNotRolledOut = 6,

  // The profile is disallowed by admin policy.
  kProfileDisallowedByAdmin = 7,

  // The profile is not enabled for Glic for some other reason.
  kProfileNotEnabledOther = 8,

  // The profile is already the last loaded profile.
  kProfileIsLastLoaded = 9,

  // The profile is already the last active profile.
  kProfileIsLastActive = 10,

  // Preloading is blocked because another Glic is already showing.
  kBlockedByShownGlic = 11,

  // The system is under memory pressure.
  kUnderMemoryPressure = 12,

  // The device has a cellular connection.
  kCellularConnection = 13,

  // The browser is being shutdown.
  kBrowserShuttingDown = 14,

  // The user already went through the Glic FRE (applicable to FRE warming).
  kUserAlreadyWentTroughFre = 15,

  kMaxValue = kUserAlreadyWentTroughFre,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicPrewarmingChecksResult)

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_
