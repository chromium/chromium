// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_FILE_SUGGESTION_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_FILE_SUGGESTION_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// Handles requests of drive modules sent from JS.
class FileSuggestionHandler
    : public file_suggestion::mojom::FileSuggestionHandler {
 public:
  FileSuggestionHandler(
      mojo::PendingReceiver<file_suggestion::mojom::FileSuggestionHandler>
          handler,
      Profile* profile);
  ~FileSuggestionHandler() override;

  // file_suggestion::mojom::FileSuggestionHandler:
  void GetFiles(GetFilesCallback callback) override;
  void DismissModule() override;
  void RestoreModule() override;

 private:
  mojo::Receiver<file_suggestion::mojom::FileSuggestionHandler> handler_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_FILE_SUGGESTION_HANDLER_H_
