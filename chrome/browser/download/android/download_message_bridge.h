// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MESSAGE_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MESSAGE_BRIDGE_H_

#include "base/functional/callback.h"
#include "components/download/public/common/download_item.h"

namespace content {
class WebContents;
}  // namespace content

// Class for showing message to warn the user about incognito download.
class DownloadMessageBridge : public download::DownloadItem::Observer {
 public:
  using DownloadMessageRequestCallback =
      base::OnceCallback<void(bool /*accepted*/)>;

  DownloadMessageBridge();
  DownloadMessageBridge(const DownloadMessageBridge&) = delete;
  DownloadMessageBridge& operator=(const DownloadMessageBridge&) = delete;

  ~DownloadMessageBridge() override;

  void ShowIncognitoDownloadMessage(DownloadMessageRequestCallback callback);
  virtual void ShowUnsupportedDownloadMessage(
      content::WebContents* web_contents);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MESSAGE_BRIDGE_H_
