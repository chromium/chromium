// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_IMAGE_DOWNLOADER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_IMAGE_DOWNLOADER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/macros.h"

class AccountId;
class GURL;

namespace gfx {
class ImageSkia;
}

namespace ash {

// Interface for a class which is responsible for downloading images on behalf
// of the Assistant UI in ash.
class ASH_PUBLIC_EXPORT AssistantImageDownloader {
 public:
  static AssistantImageDownloader* GetInstance();

  using DownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

  // Downloads the image found at |url| for the profile associated with
  // |account_id|. On completion, |callback| is run with the
  // downloaded |image|. In the event that the download attempt fails, a NULL
  // image will be returned.
  virtual void Download(const AccountId& account_id,
                        const GURL& url,
                        DownloadCallback callback) = 0;

 protected:
  AssistantImageDownloader();
  virtual ~AssistantImageDownloader();

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantImageDownloader);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_IMAGE_DOWNLOADER_H_
