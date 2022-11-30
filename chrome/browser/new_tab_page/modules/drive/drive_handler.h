// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_DRIVE_DRIVE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_DRIVE_DRIVE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/modules/drive/drive.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// Handles requests of drive modules sent from JS.
class DriveHandler : public drive::mojom::DriveHandler {
 public:
  DriveHandler(mojo::PendingReceiver<drive::mojom::DriveHandler> handler,
               Profile* profile);
  ~DriveHandler() override;

  // drive::mojom::DriveHandler:
  void GetFiles(GetFilesCallback callback) override;
  void DismissModule() override;
  void RestoreModule() override;

 private:
  mojo::Receiver<drive::mojom::DriveHandler> handler_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_DRIVE_DRIVE_HANDLER_H_
