// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_SERVICE_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/photos/photos.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

class PrefRegistrySimple;
class PrefService;

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

// Handles requests for user Google Photos data.
class PhotosService : public KeyedService,
                      public signin::IdentityManager::Observer {
 public:
  // Enumeration of various request results we want to log.
  // This is reported to histogram, please do not change the values.
  enum class RequestResult {
    kError = 0,
    kSuccess = 1,
    kCached = 2,
    kMaxValue = kCached,
  };

  // These values should be in sync with featureParam values in aboutFlags.cc.
  enum class OptInCardTitle {
    kOptInRHTitle = 0,
    kOptInFavoritesTitle = 1,
    kOptInpersonalizedTitle = 2,
    kOptInTripsTitle = 3,
  };

  static const char kLastDismissedTimePrefName[];
  static const char kOptInAcknowledgedPrefName[];
  static const char kLastMemoryOpenTimePrefName[];
  static const char kSoftOptOutCountPrefName[];
  static const char kLastSoftOptedOutTimePrefName[];
  static const base::TimeDelta kDismissDuration;
  static const base::TimeDelta kSoftOptOutDuration;
  static const int kMaxSoftOptOuts;

  PhotosService(const PhotosService&) = delete;
  PhotosService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service);
  ~PhotosService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // IdentityManager::Observer overrides.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  using GetMemoriesCallback = photos::mojom::PhotosHandler::GetMemoriesCallback;
  // Retrieves Google Photos memories from API.
  void GetMemories(GetMemoriesCallback get_memories_callback);
  // Makes the service not return data for a specified amount of time.
  void DismissModule();
  // Makes the service return data again even if dimiss time is not yet over.
  void RestoreModule();
  // Returns whether to show opt-in surface in the module.
  bool ShouldShowOptInScreen();
  // Stores whether the user has opt-in to see the module content.
  void OnUserOptIn(bool accept,
                   content::WebContents* web_contents,
                   Profile* profile);
  // Stores the last time the user opened a memory.
  void OnMemoryOpen();
  // Returns whether to show the soft opt out button.
  bool ShouldShowSoftOptOutButton();
  // Dismisses the module for fixed amount of time before asking the user
  // to opt in again.
  void SoftOptOut();
  // Returns if the user soft opted out.
  bool IsModuleSoftOptedOut();
  // Returns the string which should be shown as the opt-in card title.
  std::string GetOptInTitleText(std::vector<photos::mojom::MemoryPtr> memories);

 private:
  void OnTokenReceived(GoogleServiceAuthError error,
                       signin::AccessTokenInfo token_info);
  void OnJsonReceived(const std::string& token,
                      const std::unique_ptr<std::string> json_response);
  void OnJsonParsed(const std::string& token,
                    data_decoder::DataDecoder::ValueOrError result);
  std::string ConstructPersonalizedString(
      std::vector<photos::mojom::MemoryPtr> memories);

  // Used for fetching OAuth2 access tokens. Only non-null when a token
  // is made available, or a token is being fetched.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::vector<GetMemoriesCallback> callbacks_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<PrefService> pref_service_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PhotosService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_SERVICE_H_
