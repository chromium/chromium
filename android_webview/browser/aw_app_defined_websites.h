// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_APP_DEFINED_WEBSITES_H_
#define ANDROID_WEBVIEW_BROWSER_AW_APP_DEFINED_WEBSITES_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"

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

  // Get the global instance of AppDefinedWebsites.
  static AppDefinedWebsites* GetInstance();

  // Get the specified list of domains from the app manifest.
  // The list will be fetched on a background thread if it is not already
  // cached. The `callback` will be executed on the calling sequence.
  void GetAppDefinedDomains(AppDefinedDomainCriteria criteria,
                            AppDomainCallback callback);

 private:
  friend class base::NoDestructor<AppDefinedWebsites>;
  friend class AppDefinedWebsitesTest;

  using AppDomainProvider = base::RepeatingCallback<std::vector<std::string>(
      AppDefinedDomainCriteria)>;
  using AppDomainCallbackList =
      base::OnceCallbackList<AppDomainCallbackFunctionType>;

  explicit AppDefinedWebsites(AppDomainProvider provider);
  ~AppDefinedWebsites();

  AppDomainCallbackList& GetCallbackList(AppDefinedDomainCriteria criteria);

  void DomainsReturnedFromManifest(AppDefinedDomainCriteria criteria,
                                   const std::vector<std::string>& data);

  SEQUENCE_CHECKER(sequence_checker_);
  AppDomainProvider provider_;
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

  base::WeakPtrFactory<AppDefinedWebsites> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_APP_DEFINED_WEBSITES_H_
