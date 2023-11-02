// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CHROME_HISTORY_BACKEND_CLIENT_H_
#define CHROME_BROWSER_HISTORY_CHROME_HISTORY_BACKEND_CLIENT_H_

#include "base/memory/scoped_refptr.h"
#include "components/history/core/browser/history_backend_client.h"

namespace bookmarks {
class ModelLoader;
}

// ChromeHistoryBackendClient implements history::HistoryBackendClient interface
// to provides access to embedder-specific features.
class ChromeHistoryBackendClient : public history::HistoryBackendClient {
 public:
  explicit ChromeHistoryBackendClient(
      scoped_refptr<bookmarks::ModelLoader> model_loader);

  ChromeHistoryBackendClient(const ChromeHistoryBackendClient&) = delete;
  ChromeHistoryBackendClient& operator=(const ChromeHistoryBackendClient&) =
      delete;

  ~ChromeHistoryBackendClient() override;

  // history::HistoryBackendClient implementation.
  bool IsPinnedURL(const GURL& url) override;
  std::vector<history::URLAndTitle> GetPinnedURLs() override;
  bool IsWebSafe(const GURL& url) override;

 private:
  // ModelLoader is used to access bookmarks. May be null during testing.
  scoped_refptr<bookmarks::ModelLoader> model_loader_;
};

#endif  // CHROME_BROWSER_HISTORY_CHROME_HISTORY_BACKEND_CLIENT_H_
