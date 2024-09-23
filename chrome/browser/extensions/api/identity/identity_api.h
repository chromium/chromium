// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_

#include <optional>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/extensions/api/identity/identity_clear_all_cached_auth_tokens_function.h"
#include "chrome/browser/extensions/api/identity/identity_get_accounts_function.h"
#include "chrome/browser/extensions/api/identity/identity_get_auth_token_function.h"
#include "chrome/browser/extensions/api/identity/identity_get_profile_user_info_function.h"
#include "chrome/browser/extensions/api/identity/identity_launch_web_auth_flow_function.h"
#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"
#include "chrome/browser/extensions/api/identity/identity_remove_cached_auth_token_function.h"
#include "chrome/browser/extensions/api/identity/identity_token_cache.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace extensions {

class IdentityAPI : public BrowserContextKeyedAPI,
                    public signin::IdentityManager::Observer {
 public:
  explicit IdentityAPI(content::BrowserContext* context);
  ~IdentityAPI() override;

  // Request serialization queue for getAuthToken.
  IdentityMintRequestQueue* mint_queue();

  IdentityTokenCache* token_cache();

  // GAIA id cache.
  void SetGaiaIdForExtension(const std::string& extension_id,
                             const std::string& gaia_id);
  // Returns |std::nullopt| if no GAIA id is saved for |extension_id|.
  // Otherwise, returns GAIA id previously saved via SetGaiaIdForExtension().
  std::optional<std::string> GetGaiaIdForExtension(
      const std::string& extension_id);
  void EraseGaiaIdForExtension(const std::string& extension_id);
  // If refresh tokens have been loaded, erases GAIA ids of accounts that are no
  // longer signed in to Chrome for all extensions.
  void EraseStaleGaiaIdsForAllExtensions();

  // BrowserContextKeyedAPI:
  void Shutdown() override;
  static BrowserContextKeyedAPIFactory<IdentityAPI>* GetFactoryInstance();

  base::CallbackListSubscription RegisterOnShutdownCallback(
      base::OnceClosure cb);

  // Callback that is used in testing contexts to test the implementation of
  // the chrome.identity.onSignInChanged event. Note that the passed-in Event is
  // valid only for the duration of the callback.
  using OnSignInChangedCallback = base::RepeatingCallback<void(Event*)>;
  void set_on_signin_changed_callback_for_testing(
      const OnSignInChangedCallback& callback) {
    on_signin_changed_callback_for_testing_ = callback;
  }

  // Whether the chrome.identity API should use all accounts or the primary
  // account only.
  bool AreExtensionsRestrictedToPrimaryAccount();

  // Returns accounts that extension have access to.
  // This returns empty if Chrome is not signed in. Otherwise, returns all
  // accounts in the `identity_manager_`.
  std::vector<CoreAccountInfo> GetAccountsWithRefreshTokensForExtensions();

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Shows the Chrome sign in dialog for extensions if:
  // - The dialog is not already showing
  // - The user is signed in on the web but not to Chrome
  // `on_complete` is guaranteed to be called.
  void MaybeShowChromeSigninDialog(std::string_view extension_name,
                                   base::OnceClosure on_complete);

  // Callback to be called when the tests triggers showing UI.
  // Should be used in unittests.
  void SetSkipUIForTesting(
      base::OnceCallback<void(base::OnceClosure)> callback) {
    skip_ui_for_testing_callback_ = std::move(callback);
  }
#endif

 private:
  friend class BrowserContextKeyedAPIFactory<IdentityAPI>;
  friend class IdentityAPITest;

  // BrowserContextKeyedAPI:
  static const char* service_name() { return "IdentityAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;

  // This constructor allows to mock keyed services in tests.
  IdentityAPI(Profile* profile,
              signin::IdentityManager* identity_manager,
              ExtensionPrefs* extension_prefs,
              EventRouter* event_router);

  // Returns true if extensions have access to Chrome accounts.
  // Access requires Chrome to be signed in.
  bool HasAccessToChromeAccounts() const;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokensLoaded() override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  // NOTE: This class must listen for this callback rather than
  // OnRefreshTokenRemovedForAccount() to obtain the Gaia ID of the removed
  // account.
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  // Fires the chrome.identity.onSignInChanged event.
  void FireOnAccountSignInChanged(const std::string& gaia_id,
                                  bool is_signed_in);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void OnChromeSigninDialogDestroyed();
  void HandleSkipUIForTesting(base::OnceClosure on_complete);
#endif

  const raw_ptr<Profile> profile_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<ExtensionPrefs> extension_prefs_;
  const raw_ptr<EventRouter> event_router_;

  IdentityMintRequestQueue mint_queue_;
  IdentityTokenCache token_cache_;
  // Contains Gaia Id of accounts known to extensions.
  base::flat_set<std::string> accounts_known_to_extensions_;

  OnSignInChangedCallback on_signin_changed_callback_for_testing_;

  base::OnceCallbackList<void()> on_shutdown_callback_list_;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  bool is_chrome_signin_dialog_open_ = false;
  std::vector<base::OnceClosure> on_chrome_signin_dialog_completed_;
  // Should only be set in unittests.
  base::OnceCallback<void(base::OnceClosure)> skip_ui_for_testing_callback_;
  base::WeakPtrFactory<IdentityAPI> weak_ptr_factory_{this};
#endif
};

template <>
void BrowserContextKeyedAPIFactory<IdentityAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
