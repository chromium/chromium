// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_MICROSOFT_FILES_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_MICROSOFT_FILES_PAGE_HANDLER_H_

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
  void CreateTrendingFiles(GetFilesCallback callback, base::Value::Dict result);
  void CreateRecentlyUsedAndSharedFiles(GetFilesCallback callback,
                                        base::Value::Dict result);
  // Retrieves trending files from the Microsoft Graph API.
  void GetTrendingFiles(GetFilesCallback callback);
  void GetRecentlyUsedAndSharedFiles(GetFilesCallback callback);
  void OnJsonReceived(GetFilesCallback callback,
                      std::unique_ptr<std::string> response_body);
  void OnJsonParsed(GetFilesCallback callback,
                    data_decoder::DataDecoder::ValueOrError result);
  std::vector<file_suggestion::mojom::FilePtr> SortAndRemoveDuplicates(
      std::vector<std::pair<base::Time, file_suggestion::mojom::FilePtr>>
          unsorted_suggestions);
  std::string CreateJustificationTextForRecentFile(base::Time opened_time);
  std::string CreateJustificationTextForSharedFile(std::string shared_by);
  mojo::Receiver<file_suggestion::mojom::MicrosoftFilesPageHandler> handler_;
  raw_ptr<MicrosoftAuthService> microsoft_auth_service_;
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<MicrosoftFilesPageHandler> weak_factory_{this};
};
#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_MICROSOFT_FILES_PAGE_HANDLER_H_
