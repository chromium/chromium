// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_DRIVE_SERVICE_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_DRIVE_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ItemSuggestRequestResult {
  kSuccess = 0,
  kNetworkError = 1,
  kJsonParseError = 2,
  kContentError = 3,
  kMaxValue = kContentError,
};

// Handles requests for user Google Drive data.
class DriveService : public KeyedService {
 public:
  static const char kLastDismissedTimePrefName[];
  static const base::TimeDelta kDismissDuration;

  DriveService(const DriveService&) = delete;
  DriveService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      segmentation_platform::SegmentationPlatformService*
          segmentation_platform_service,
      const std::string& application_locale,
      PrefService* pref_service);
  ~DriveService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  using GetFilesCallback = file_suggestion::mojom::FileSuggestionHandler::GetFilesCallback;
  // Retrieves Google Drive document suggestions from ItemSuggest API.
  void GetDriveFiles(GetFilesCallback get_files_callback);
  // Retrieves classification result from segmentation platform before
  // retrieving document suggestions.
  // TODO(crbug.com/40925895): Use the classification result to decide when to
  // show the Drive module, instead of ignoring it.
  bool GetDriveModuleSegmentationData();
  void GetDriveFilesInternal();
  // Makes the service not return data for a specified amount of time.
  void DismissModule();
  // Makes the service return data again even if dimiss time is not yet over.
  void RestoreModule();

 private:
  void OnTokenReceived(GoogleServiceAuthError error,
                       signin::AccessTokenInfo token_info);
  void OnJsonReceived(const std::string& token,
                      std::unique_ptr<std::string> json_response);
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);

  // Used for fetching OAuth2 access tokens. Only non-null when a token
  // is made available, or a token is being fetched.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::vector<GetFilesCallback> callbacks_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_platform_service_;
  std::string application_locale_;
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<std::string> cached_json_;
  base::Time cached_json_time_;
  std::string cached_json_token_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DriveService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_DRIVE_SERVICE_H_
