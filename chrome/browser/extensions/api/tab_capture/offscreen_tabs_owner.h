// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_OFFSCREEN_TABS_OWNER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_OFFSCREEN_TABS_OWNER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/media/offscreen_tab.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/size.h"

class OffscreenTab;

namespace extensions {

// Creates, owns, and manages all OffscreenTab instances created by the same
// extension background page.  When the extension background page's WebContents
// is about to be destroyed, its associated OffscreenTabsOwner and all of its
// OffscreenTab instances are destroyed.
//
// Usage:
//
//   OffscreenTabsOwner::Get(extension_contents)
//       ->OpenNewTab(start_url, size, std::string());
//
// This class operates exclusively on the UI thread and so is not thread-safe.
class OffscreenTabsOwner final
    : public OffscreenTab::Owner,
      public content::WebContentsUserData<OffscreenTabsOwner> {
 public:
  ~OffscreenTabsOwner() final;

  // Returns the OffscreenTabsOwner instance associated with the given extension
  // background page's WebContents.  Never returns nullptr.
  static OffscreenTabsOwner* Get(content::WebContents* extension_web_contents);

  // Instantiate a new offscreen tab and navigate it to |start_url|.  The new
  // tab's main frame will start out with the given |initial_size| in DIP
  // coordinates.  If too many offscreen tabs are already running, nothing
  // happens and nullptr is returned.
  //
  // If |optional_presentation_id| is non-empty, the offscreen tab is registered
  // for use by the Media Router (chrome/browser/media/router/...) as the
  // receiving browsing context for the W3C Presentation API.
  OffscreenTab* OpenNewTab(const GURL& start_url,
                           const gfx::Size& initial_size,
                           const std::string& optional_presentation_id);

 private:
  friend class content::WebContentsUserData<OffscreenTabsOwner>;

  explicit OffscreenTabsOwner(content::WebContents* extension_web_contents);

  // OffscreenTab::Owner implementation.
  void RequestMediaAccessPermission(
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  void DestroyTab(OffscreenTab* tab) override;

  content::WebContents* const extension_web_contents_;
  std::vector<std::unique_ptr<OffscreenTab>> tabs_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(OffscreenTabsOwner);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_OFFSCREEN_TABS_OWNER_H_
