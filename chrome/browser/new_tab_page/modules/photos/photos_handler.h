// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/modules/photos/photos.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// Handles requests of photos modules sent from JS.
class PhotosHandler : public photos::mojom::PhotosHandler {
 public:
  PhotosHandler(mojo::PendingReceiver<photos::mojom::PhotosHandler> handler,
                Profile* profile,
                content::WebContents* web_contents);
  ~PhotosHandler() override;

  // photos::mojom::PhotosHandler:
  void GetMemories(GetMemoriesCallback callback) override;
  void DismissModule() override;
  void RestoreModule() override;
  void ShouldShowOptInScreen(ShouldShowOptInScreenCallback callback) override;
  void OnUserOptIn(bool accept) override;
  void OnMemoryOpen() override;
  void ShouldShowSoftOptOutButton(
      ShouldShowSoftOptOutButtonCallback callback) override;
  void SoftOptOut() override;
  void GetOptInTitleText(std::vector<photos::mojom::MemoryPtr> memories,
                         GetOptInTitleTextCallback callback) override;

 private:
  mojo::Receiver<photos::mojom::PhotosHandler> handler_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_HANDLER_H_
