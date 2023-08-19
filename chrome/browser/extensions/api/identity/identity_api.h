// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_

#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/feature_list.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  // Returns |absl::nullopt| if no GAIA id is saved for |extension_id|.
  // Otherwise, returns GAIA id previously saved via SetGaiaIdForExtension().
  absl::optional<std::string> GetGaiaIdForExtension(
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

  // signin::IdentityManager::Observer:
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

  const raw_ptr<Profile> profile_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<ExtensionPrefs> extension_prefs_;
  const raw_ptr<EventRouter> event_router_;

  IdentityMintRequestQueue mint_queue_;
  IdentityTokenCache token_cache_;

  OnSignInChangedCallback on_signin_changed_callback_for_testing_;

  base::OnceCallbackList<void()> on_shutdown_callback_list_;
};

template <>
void BrowserContextKeyedAPIFactory<IdentityAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
