// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACCOUNT_EXTENSION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_ACCOUNT_EXTENSION_TRACKER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

class BrowserContextKeyedServiceFactory;
class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

// This service manages each extension's AccountExtensionType, which describes
// whether the extension is associated with a signed in user's account data.
class AccountExtensionTracker : public KeyedService,
                                public signin::IdentityManager::Observer {
 public:
  // Maximum delay between initiating a sign in from the extension installed
  // bubble and completing the sign in for the associated extension to still be
  // promoted to an account extension. Beyond this delay, it is assumed that the
  // user did not intend to sign in after installing the extension.
  static constexpr base::TimeDelta kMaxSigninFromExtensionBubbleDelay =
      base::Minutes(50);

  enum AccountExtensionType {
    // The extension is only associated with the current device. This is used
    // for:
    // - all unsyncable extensions
    // - all extensions if there is no signed in user with sync enabled
    // - all extensions installed before the user signed in, that are not part
    //   the user's account's data.
    kLocal = 0,
    // The extension is part of the signed in user's account data but was
    // installed on this device before the user has signed in.
    kAccountInstalledLocally = 1,
    // The extension is part of the signed in user's account data and was
    // installed on this device after the user has signed in.
    kAccountInstalledSignedIn = 2,
    kLast = 2,
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when an extension's eligibility to be uploaded to the user's
    // account may have changed.
    virtual void OnExtensionUploadabilityChanged(const ExtensionId& id) = 0;

    // Called when whether extensions can be uploaded to the user's account may
    // be changed. Usually emitted when the initial sync download completes or
    // when the user is no longer syncing extensions in transport mode.
    virtual void OnExtensionsUploadabilityChanged() = 0;
  };

  explicit AccountExtensionTracker(Profile* profile);

  AccountExtensionTracker(const AccountExtensionTracker&) = delete;
  AccountExtensionTracker& operator=(const AccountExtensionTracker&) = delete;

  ~AccountExtensionTracker() override;

  // Convenience method to get the AccountExtensionTracker for a profile.
  // Creates the tracker for the profile if none exists.
  static AccountExtensionTracker* Get(content::BrowserContext* context);

  // Returns the singleton instance of the factory for this KeyedService.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Computes and sets the AccountExtensionType for `extension` when it is
  // installed. Needs to be called by the sync service before it handles the
  // install.
  void SetAccountExtensionTypeOnExtensionInstalled(const Extension& extension);

  // IdentityManagerObserver implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // Called when sync data is received for the given `extension_id`.
  void OnExtensionSyncDataReceived(const ExtensionId& extension_id);

  // Called just after the initial set of extension sync data is received.
  // i.e. during browser startup (if extensions sync is already enabled), or
  // once the initial download completes after extensions sync gets enabled.
  void OnInitialExtensionsSyncDataReceived();

  AccountExtensionType GetAccountExtensionType(
      const ExtensionId& extension_id) const;

  // Returns all account extensions with type `kAccountInstalledSignedIn`.
  std::vector<const Extension*> GetSignedInAccountExtensions() const;

  // Called when the user initiates a signin from a promo that appears after an
  // extension with the given `extension_id` is installed.
  void OnSignInInitiatedFromExtensionPromo(const ExtensionId& extension_id);

  // Whether the given `extension` can be uploaded to/associated with the
  // current signed in user.
  bool CanUploadAsAccountExtension(const Extension& extension) const;

  // Called when the user initiates an upload for the given `extension_id` to
  // their account.
  void OnAccountUploadInitiatedForExtension(const ExtensionId& extension_id);

  void set_uninstall_account_extensions_on_signout(
      bool uninstall_account_extensions_on_signout) {
    uninstall_account_extensions_on_signout_ =
        uninstall_account_extensions_on_signout;
  }

  void SetAccountExtensionTypeForTesting(const ExtensionId& extension_id,
                                         AccountExtensionType type);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Sets the extension's AccountExtensionType. Called when the extension is
  // installed (not updated) or when there is incoming sync data for the
  // extension, which implies that it's associated with a user's account data.
  void SetAccountExtensionType(const ExtensionId& extension_id,
                               AccountExtensionType type);

  // Removes `extension_id` in `extensions_installed_with_signin_promo_`.
  void RemoveExpiredExtension(const ExtensionId& extension_id);

  // Promotes `extension_id` from a local to an account extension specified by
  // `type`. Unlike just calling `SetAccountExtensionType`, this always alerts
  // observers that the extension's uploadability may have changed.
  void PromoteLocalToAccountExtension(const ExtensionId& extension_id,
                                      AccountExtensionType type);

  // Notifies observers that the eligibility of multiple extensions to be
  // uploaded to the user's account may have changed.
  void NotifyOnExtensionsUploadabilityChanged();

  const raw_ptr<Profile> profile_;

  // Keeps track of extensions for which a signin promo was shown after
  // installation.
  std::vector<ExtensionId> extensions_installed_with_signin_promo_;

  // Whether account extensions with type `kAccountInstalledSignedIn` should be
  // uninstalled when the primary user signs out.
  bool uninstall_account_extensions_on_signout_ = false;

  base::ObserverList<Observer> observers_;

  // IdentityManager observer.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Must be the last member variable. See WeakPtrFactory documentation for
  // details.
  base::WeakPtrFactory<AccountExtensionTracker> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACCOUNT_EXTENSION_TRACKER_H_
