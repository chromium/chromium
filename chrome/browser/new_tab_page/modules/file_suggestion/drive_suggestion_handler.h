// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_DRIVE_SUGGESTION_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_DRIVE_SUGGESTION_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// Handles requests of drive modules sent from JS.
class DriveSuggestionHandler
    : public file_suggestion::mojom::DriveSuggestionHandler {
 public:
  DriveSuggestionHandler(
      mojo::PendingReceiver<file_suggestion::mojom::DriveSuggestionHandler>
          handler,
      Profile* profile);
  ~DriveSuggestionHandler() override;

  // file_suggestion::mojom::DriveSuggestionHandler:
  void GetFiles(GetFilesCallback callback) override;
  void DismissModule() override;
  void RestoreModule() override;

 private:
  mojo::Receiver<file_suggestion::mojom::DriveSuggestionHandler> handler_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_DRIVE_SUGGESTION_HANDLER_H_
