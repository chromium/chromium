// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_DRIVE_DRIVE_HANDLER_H_
#define CHROME_BROWSER_SEARCH_DRIVE_DRIVE_HANDLER_H_

#include "chrome/browser/search/drive/drive.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Handles requests of drive modules sent from JS.
class DriveHandler : public drive::mojom::DriveHandler {
 public:
  explicit DriveHandler(
      mojo::PendingReceiver<drive::mojom::DriveHandler> handler);
  ~DriveHandler() override;

  // drive::mojom::DriveHandler:
  void GetTestString(GetTestStringCallback callback) override;

 private:
  mojo::Receiver<drive::mojom::DriveHandler> handler_;
};

#endif  // CHROME_BROWSER_SEARCH_DRIVE_DRIVE_HANDLER_H_
