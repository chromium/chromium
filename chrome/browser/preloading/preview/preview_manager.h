// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_MANAGER_H_
#define CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;
class PreviewTab;
class PreviewZoomController;

namespace content {
class PreviewCancelReason;
}  // namespace content

// Handles requests of preview and manages ongoing previews.
class PreviewManager final
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PreviewManager> {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Usage {
    // LinkPreview feature is not used in the page at all.
    kNotUsed = 0,
    // Preview windows are opened for the page at least once,
    // but they are not promoted.
    kUsedButNotPromoted = 1,
    // Preview windows are opened for the page and one of them is promoted.
    kUsedAndPromoted = 2,

    kMaxValue = kUsedAndPromoted,
  };
  PreviewManager(const PreviewManager&) = delete;
  PreviewManager& operator=(const PreviewManager&) = delete;

  ~PreviewManager() override;

  // content::WebContentsObserver implementation:
  void PrimaryPageChanged(content::Page& page) override;

  void InitiatePreview(const GURL& url);
  void Cancel(content::PreviewCancelReason reason);
  void PromoteToNewTab();

  Usage usage() { return usage_; }

  base::WeakPtr<content::WebContents> GetWebContentsForPreviewTab();

  // This method closes a preview page, and used for testing until the primary
  // page navigation closes existing preview pages.
  void CloseForTesting();
  PreviewZoomController* PreviewZoomControllerForTesting() const;

 private:
  friend class content::WebContentsUserData<PreviewManager>;

  explicit PreviewManager(content::WebContents* web_contents);

  Usage usage_ = Usage::kNotUsed;

  std::unique_ptr<PreviewTab> tab_;

  base::WeakPtrFactory<PreviewManager> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_MANAGER_H_
