// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_APP_DEFINED_WEBSITES_H_
#define ANDROID_WEBVIEW_BROWSER_AW_APP_DEFINED_WEBSITES_H_

#include <memory>
#include <string>
#include <vector>

#include "android_webview/browser/aw_asset_domain_list_include_handler.h"
#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/origin.h"

namespace android_webview {

// Used to determine which sources to retrieve related websites from.
// This enum is used to set the values for feature parameter
// `features::kWebViewIpProtectionExclusionCriteria`.
// Do not modify/reorder the enum without ensuring that the above mentioned
// feature is compatible with the change.
enum class AppDefinedDomainCriteria {
  // Return nothing.
  kNone = 0,
  // Return domains defined in the `asset_statements` meta-data tag in the
  // app's manifest.
  kAndroidAssetStatements = 1,
  // For API >= 31, return domains defined in Android App Links and verified
  // by DomainVerificationManager.
  // For API < 31, return nothing.
  kAndroidVerifiedAppLinks = 2,
  // For API >= 31, return domains defined in Web Links (including Android
  // App Links).
  // For API < 31, return nothing.
  kAndroidWebLinks = 3,
  // Union of kAndroidAssetStatements, kAndroidVerifiedAppLinks and
  // kAndroidVerifiedAppLinks.
  kAndroidAssetStatementsAndWebLinks = 4,
};

// AppDefinedWebsites provides access to domain lists defined in the embedding
// app's manifest file. As the domain lists are static, they will be cached for
// future access once they have been loaded once.
// This class should only be accessed on the UI thread.
class AppDefinedWebsites {
 public:
  using AppDomainCallbackFunctionType = void(const std::vector<std::string>&);
  using AppDomainCallback = base::OnceCallback<AppDomainCallbackFunctionType>;
  using AppDomainSetCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;

  // Get the global instance of AppDefinedWebsites.
  // May only be called on the UI thread.
  static AppDefinedWebsites* GetInstance();

  // Get the specified list of domains from the app manifest.
  // The list will be fetched on a background thread if it is not already
  // cached. The `callback` will be executed on the calling sequence.
  void GetAppDefinedDomains(AppDefinedDomainCriteria criteria,
                            AppDomainCallback callback);

  // Get the list of Android Asset Statement domains, including any domains
  // refenced through "include" statements.
  // This method may cause network requests if there are any include statements
  // in the asset list and they have not been loaded yet.
  // The `domain_list_loader` will be used to load included references from the
  // network. The `callback` is executed on the calling sequence.
  void GetAssetStatmentsWithIncludes(
      std::unique_ptr<AssetDomainListIncludeHandler> domain_list_loader,
      AppDomainSetCallback callback);

  // Check if the provided `origin` is defined by the app's asset statement
  // domains. This method may cause network requests if there are any include
  // statements in the asset list and they have not been loaded yet. The
  // `domain_list_loader` will be used to load included references from the
  // network. The `callback` is executed on the calling sequence.
  void AppDeclaresDomainInAssetStatements(
      std::unique_ptr<AssetDomainListIncludeHandler> domain_list_loader,
      const url::Origin& origin,
      base::OnceCallback<void(bool)> callback);

 private:
  friend class base::NoDestructor<AppDefinedWebsites>;
  friend class AppDefinedWebsitesTest;

  using AppDomainProvider = base::RepeatingCallback<std::vector<std::string>(
      AppDefinedDomainCriteria)>;
  using IncludeLinkProvider =
      base::RepeatingCallback<std::vector<std::string>()>;
  using AppDomainCallbackList =
      base::OnceCallbackList<AppDomainCallbackFunctionType>;

  AppDefinedWebsites(AppDomainProvider provider,
                     IncludeLinkProvider include_link_provider);
  ~AppDefinedWebsites();

  AppDomainCallbackList& GetCallbackList(AppDefinedDomainCriteria criteria);

  void DomainsReturnedFromManifest(AppDefinedDomainCriteria criteria,
                                   const std::vector<std::string>& data);

  void AssetIncludeStatementsReturned(
      std::unique_ptr<AssetDomainListIncludeHandler> domain_list_loader,
      std::vector<std::string> data);

  void OnAssetStatementsWithIncludesLoaded(
      std::unique_ptr<AssetDomainListIncludeHandler> domain_list_handler,
      std::vector<std::vector<std::string>> all_domains);

  SEQUENCE_CHECKER(sequence_checker_);
  AppDomainProvider provider_;
  IncludeLinkProvider include_link_provider_;
  // Cache of already-fetched domains. A nullptr value means the domains have
  // not been fetched yet.
  base::flat_map<AppDefinedDomainCriteria,
                 std::unique_ptr<std::vector<std::string>>>
      domains_cache_;

  // Lists of callbacks that wait for a particular criteria to be fetched.
  // Using OnceCallbackList to handle multiple concurrent calls waiting.
  base::flat_map<AppDefinedDomainCriteria,
                 std::unique_ptr<AppDomainCallbackList>>
      on_domains_returned_callbacks_;

  std::unique_ptr<std::vector<std::string>> asset_statements_with_includes_;
  AppDomainCallbackList asset_statements_with_includes_callbacks_;

  base::WeakPtrFactory<AppDefinedWebsites> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_APP_DEFINED_WEBSITES_H_
