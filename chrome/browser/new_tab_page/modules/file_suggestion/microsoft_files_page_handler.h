// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_MICROSOFT_FILES_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_MICROSOFT_FILES_PAGE_HANDLER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

class MicrosoftAuthService;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MicrosoftFilesRequestResult {
  // Success parsing necessary file data from the response.
  kSuccess = 0,
  kNetworkError = 1,
  kJsonParseError = 2,
  // Error retrieving all the expected file data from the response.
  kContentError = 3,
  kThrottlingError = 4,
  // Unauthorized error.
  kAuthError = 5,
  kMaxValue = kAuthError,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MicrosoftFilesSubstitutionType {
  // No substitution occurred.
  kNone = 0,
  // More non-insight files than it's initial limit were added to the files
  // list.
  kExtraNonInsights = 1,
  // More trending files than it's initial limit were added to the files list.
  kExtraTrending = 2,
  kMaxValue = kExtraTrending,
};

class MicrosoftFilesPageHandler
    : public file_suggestion::mojom::MicrosoftFilesPageHandler {
 public:
  explicit MicrosoftFilesPageHandler(
      mojo::PendingReceiver<file_suggestion::mojom::MicrosoftFilesPageHandler>
          handler,
      Profile* profile);
  ~MicrosoftFilesPageHandler() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // file_suggestion::mojom::MicrosoftFilesPageHandler
  void DismissModule() override;
  void RestoreModule() override;
  void GetFiles(GetFilesCallback callback) override;

 private:
  std::vector<file_suggestion::mojom::FilePtr> GetTrendingFiles(
      base::Value::Dict result);
  std::vector<file_suggestion::mojom::FilePtr> GetRecentlyUsedAndSharedFiles(
      base::Value::Dict result);
  std::vector<file_suggestion::mojom::FilePtr> GetAggregatedFileSuggestions(
      base::Value::Dict result);
  // Returns non-insight files with the time that will be used to sort the
  // files. Note that non-insight files refers to used or shared files.
  std::vector<std::pair<base::Time, file_suggestion::mojom::FilePtr>>
  GetNonInsightFiles(const base::Value::List* values, std::string response_id);
  // Retrieves files based on the variation type enabled.
  void RequestFiles(GetFilesCallback callback,
                    std::string request_url,
                    std::optional<std::string> request_body);
  // Triggers `callback` to be ran with fake file data after parsing.
  void ParseFakeData(GetFilesCallback callback);
  void OnJsonReceived(GetFilesCallback callback,
                      std::unique_ptr<std::string> response_body);
  void OnJsonParsed(GetFilesCallback callback,
                    data_decoder::DataDecoder::ValueOrError result);
  void RecordRequestMetrics();
  std::string CreateJustificationTextForRecentFile(base::Time opened_time);
  std::string CreateJustificationTextForSharedFile(std::string shared_by);
  std::vector<file_suggestion::mojom::FilePtr> SortRecentlyUsedAndSharedFiles(
      std::vector<std::pair<base::Time, file_suggestion::mojom::FilePtr>>
          suggestions);
  // Deduplicates and limits file suggestions.
  std::vector<file_suggestion::mojom::FilePtr> DeduplicateAndLimitSuggestions(
      std::vector<file_suggestion::mojom::FilePtr> suggestions);
  mojo::Receiver<file_suggestion::mojom::MicrosoftFilesPageHandler> handler_;
  raw_ptr<MicrosoftAuthService> microsoft_auth_service_;
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Keeps track of the end state from requesting files.
  std::optional<MicrosoftFilesRequestResult> request_result_;
  // The number of files found in a successful response body. Used to log
  // `NewTabPage.MicrosoftFiles.ResponseResult`
  std::optional<int> num_files_in_response_;
  ntp_features::NtpSharepointModuleDataType variation_type_;
  base::WeakPtrFactory<MicrosoftFilesPageHandler> weak_factory_{this};
};
#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_MICROSOFT_FILES_PAGE_HANDLER_H_
