// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_MICROSOFT_FILES_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_MICROSOFT_FILES_PAGE_HANDLER_H_

#include <string>

#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

class MicrosoftFilesPageHandler
    : public file_suggestion::mojom::MicrosoftFilesPageHandler {
 public:
  explicit MicrosoftFilesPageHandler(
      mojo::PendingReceiver<file_suggestion::mojom::MicrosoftFilesPageHandler>
          handler,
      Profile* profile);
  ~MicrosoftFilesPageHandler() override;

  // file_suggestion::mojom::MicrosoftFilesPageHandler
  void GetFiles(GetFilesCallback callback) override;

 private:
  // Retrieves trending files from the Microsoft Graph API.
  void GetTrendingFiles(GetFilesCallback callback);
  void OnJsonReceived(GetFilesCallback callback,
                      std::unique_ptr<std::string> response_body);
  void OnJsonParsed(GetFilesCallback callback,
                    data_decoder::DataDecoder::ValueOrError result);
  mojo::Receiver<file_suggestion::mojom::MicrosoftFilesPageHandler> handler_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<MicrosoftFilesPageHandler> weak_factory_{this};
};
#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_MICROSOFT_FILES_PAGE_HANDLER_H_
