// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_file_system_url_util.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

namespace {

void OnResolveToContentUrlOnUIThread(
    ArcDocumentsProviderRoot::ResolveToContentUrlCallback callback,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), url));
}

void ResolveToContentUrlOnUIThread(
    const storage::FileSystemURL& url,
    ArcDocumentsProviderRoot::ResolveToContentUrlCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnResolveToContentUrlOnUIThread(std::move(callback), GURL());
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnResolveToContentUrlOnUIThread(std::move(callback), GURL());
    return;
  }

  root->ResolveToContentUrl(
      path,
      base::BindOnce(&OnResolveToContentUrlOnUIThread, std::move(callback)));
}

}  // namespace

void ResolveToContentUrlOnIOThread(
    const storage::FileSystemURL& url,
    ArcDocumentsProviderRoot::ResolveToContentUrlCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ResolveToContentUrlOnUIThread, url, std::move(callback)));
}

}  // namespace arc
