// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACCOUNT_EXTENSION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_ACCOUNT_EXTENSION_TRACKER_H_

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

  // Called when sync data is applied for the given `extension_id`.
  void OnExtensionSyncDataApplied(const ExtensionId& extension_id);

  AccountExtensionType GetAccountExtensionType(
      const ExtensionId& extension_id) const;

  // Called when the user initiates a signin from a promo that appears after an
  // extension with the given `extension_id` is installed.
  void OnSignInInitiatedFromExtensionPromo(const ExtensionId& extension_id);

  // Whether the given `extension` can be uploaded to/associated with the
  // current signed in user.
  bool CanUploadAsAccountExtension(const Extension& extension) const;

  void SetAccountExtensionTypeForTesting(const ExtensionId& extension_id,
                                         AccountExtensionType type);

 private:
  // Sets the extension's AccountExtensionType. Called when the extension is
  // installed (not updated) or when there is incoming sync data for the
  // extension, which implies that it's associated with a user's account data.
  void SetAccountExtensionType(const ExtensionId& extension_id,
                               AccountExtensionType type);

  // Removes `extension_id` in `extensions_installed_with_signin_promo_`.
  void RemoveExpiredExtension(const ExtensionId& extension_id);

  const raw_ptr<Profile> profile_;

  // Keeps track of extensions for which a signin promo was shown after
  // installation.
  std::vector<ExtensionId> extensions_installed_with_signin_promo_;

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
