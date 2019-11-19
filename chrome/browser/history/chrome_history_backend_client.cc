// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/chrome_history_backend_client.h"

#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "components/bookmarks/browser/history_bookmark_model.h"
#include "components/bookmarks/browser/model_loader.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/child_process_security_policy.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/history/android/android_provider_backend.h"
#include "components/history/core/browser/history_backend.h"
#endif

#if defined(OS_ANDROID)
namespace {
const base::FilePath::CharType kAndroidCacheFilename[] =
    FILE_PATH_LITERAL("AndroidCache");
}
#endif

ChromeHistoryBackendClient::ChromeHistoryBackendClient(
    bookmarks::ModelLoader* model_loader)
    : model_loader_(model_loader) {}

ChromeHistoryBackendClient::~ChromeHistoryBackendClient() {
}

bool ChromeHistoryBackendClient::IsPinnedURL(const GURL& url) {
  if (!model_loader_)
    return false;

  // HistoryBackendClient is used to determine if an URL is bookmarked. The data
  // is loaded on a separate thread and may not be done when this method is
  // called, therefore blocks until the bookmarks have finished loading.
  model_loader_->BlockTillLoaded();
  return model_loader_->history_bookmark_model()->IsBookmarked(url);
}

std::vector<history::URLAndTitle> ChromeHistoryBackendClient::GetPinnedURLs() {
  std::vector<history::URLAndTitle> result;
  if (!model_loader_)
    return result;

  // HistoryBackendClient is used to determine the set of bookmarked URLs. The
  // data is loaded on a separate thread and may not be done when this method is
  // called, therefore blocks until the bookmarks have finished loading.
  std::vector<bookmarks::UrlAndTitle> url_and_titles;
  model_loader_->BlockTillLoaded();
  model_loader_->history_bookmark_model()->GetBookmarks(&url_and_titles);

  result.reserve(url_and_titles.size());
  for (const auto& url_and_title : url_and_titles) {
    result.push_back(
        history::URLAndTitle{url_and_title.url, url_and_title.title});
  }
  return result;
}

bool ChromeHistoryBackendClient::IsWebSafe(const GURL& url) {
  return content::ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
      url.scheme());
}

#if defined(OS_ANDROID)
void ChromeHistoryBackendClient::OnHistoryBackendInitialized(
    history::HistoryBackend* history_backend,
    history::HistoryDatabase* history_database,
    history::ThumbnailDatabase* thumbnail_database,
    const base::FilePath& history_dir) {
  DCHECK(history_backend);
  if (thumbnail_database) {
    history_backend->SetUserData(
        history::AndroidProviderBackend::GetUserDataKey(),
        std::make_unique<history::AndroidProviderBackend>(
            history_dir.Append(kAndroidCacheFilename), history_database,
            thumbnail_database, this, history_backend));
  }
}

void ChromeHistoryBackendClient::OnHistoryBackendDestroyed(
    history::HistoryBackend* history_backend,
    const base::FilePath& history_dir) {
  sql::Database::Delete(history_dir.Append(kAndroidCacheFilename));
}
#endif  // defined(OS_ANDROID)
