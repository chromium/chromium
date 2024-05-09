// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_DOCUMENT_ICON_FETCHER_H_
#define CHROME_BROWSER_SHORTCUTS_DOCUMENT_ICON_FETCHER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/shortcuts/fetch_icons_from_document_task.h"
#include "content/public/browser/document_user_data.h"

namespace content {
class WebContents;
}

namespace shortcuts {

// This object is responsible for fetching all available icons from a given
// document.
class DocumentIconFetcher
    : public content::DocumentUserData<DocumentIconFetcher> {
 public:
  // Fetches all icons for the top level primary frame of the given web
  // contents. `callback` will always be called (even on document destruction),
  // and always called asynchronously. If the callback is not called with an
  // error, it is guaranteed to include at least one icon (i.e. it is not
  // possible for fetching to succeed but not return any icons. If no icons
  // were found, a fallback icon is generated).
  static void FetchIcons(content::WebContents& web_contents,
                         FetchIconsFromDocumentCallback callback);

  ~DocumentIconFetcher() override;

 private:
  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit DocumentIconFetcher(content::RenderFrameHost* rfh);

  void RunTask(char32_t fallback_letter,
               FetchIconsFromDocumentCallback callback);

  void OnTaskComplete(int id,
                      char32_t fallback_letter,
                      FetchIconsFromDocumentCallback original_callback,
                      FetchIconsFromDocumentResult result);

  bool in_destruction_ = false;
  int next_task_id_ = 0;
  base::flat_map<int, std::unique_ptr<FetchIconsFromDocumentTask>> fetch_tasks_;

  base::WeakPtrFactory<DocumentIconFetcher> weak_factory_{this};
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_DOCUMENT_ICON_FETCHER_H_
