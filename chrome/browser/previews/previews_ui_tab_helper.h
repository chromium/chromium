// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_UI_TAB_HELPER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_UI_TAB_HELPER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

typedef base::OnceCallback<void(bool opt_out)> OnDismissPreviewsUICallback;

namespace content {
class NavigationHandle;
}  // namespace content

// Tracks whether a previews UI has been shown for a page. Handles showing
// the UI when the main frame response indicates a Lite Page. Handles tracking
// PreviewsState (and other data) throughout the lifetime of the
// preview/navigation.
class PreviewsUITabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PreviewsUITabHelper> {
 public:
  ~PreviewsUITabHelper() override;

  // Trigger the Previews UI to be shown to the user.
  void ShowUIElement(previews::PreviewsType previews_type,
                     OnDismissPreviewsUICallback on_dismiss_callback);

  // Reloads the content of the page without previews.
  void ReloadWithoutPreviews();

  // Reloads the content of the page without previews for the given preview
  // type.
  void ReloadWithoutPreviews(previews::PreviewsType previews_type);

  // Indicates whether the UI for a preview has been shown for the page.
  bool displayed_preview_ui() const { return displayed_preview_ui_; }

  // Sets whether the UI for a preview has been shown for the page.
  // |displayed_preview_ui_| is reset to false on
  // DidStartProvisionalLoadForFrame for the main frame.
  void set_displayed_preview_ui(bool displayed) {
    displayed_preview_ui_ = displayed;
  }

#if defined(OS_ANDROID)
  // Indicates whether the Android Omnibox badge should be shown as the Previews
  // UI.
  bool should_display_android_omnibox_badge() const {
    return should_display_android_omnibox_badge_;
  }
#endif

  // The Previews information related to the navigation that was most recently
  // finished.
  previews::PreviewsUserData* GetPreviewsUserData() const;

  // Create a PreviewsUserData and associate |navigation_handle| with the
  // PreviewsUserData, so it can be deleted later. |page_id| is the Previews
  // identifier for the load.
  previews::PreviewsUserData* CreatePreviewsUserDataForNavigationHandle(
      content::NavigationHandle* navigation_handle,
      int64_t page_id);

  // Gets the PreviewsUserData associated with |navigation_handle|. It may be
  // null.
  previews::PreviewsUserData* GetPreviewsUserData(
      content::NavigationHandle* navigation_handle);

 private:
  friend class content::WebContentsUserData<PreviewsUITabHelper>;
  friend class PreviewsUITabHelperUnitTest;

  explicit PreviewsUITabHelper(content::WebContents* web_contents);

  // Synchronously removes any data associated with |navigation_handle|.
  void RemovePreviewsUserData(int64_t navigation_id);

  // Overridden from content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Records the time of the navigation if the current navigation is a reload
  // and a preview was shown.
  void MaybeRecordPreviewReload(content::NavigationHandle* navigation_handle);

  // Show the user the Infobar if they need to be to notified that Lite mode
  // now optimizes HTTPS pages.
  void MaybeShowInfoBar(content::NavigationHandle* navigation_handle);

  // True if the UI for a preview has been shown for the page.
  bool displayed_preview_ui_ = false;

#if defined(OS_ANDROID)
  // True if the Android Omnibox badge should be shown as the Previews UI.
  bool should_display_android_omnibox_badge_ = false;
#endif

  // The callback to run when the original page is loaded.
  OnDismissPreviewsUICallback on_dismiss_callback_;

  // The data related to a given navigation ID. Created in the navigation
  // pathway (see chrome_content_browser_client.cc). Removed in a PostTask from
  // DidFinishNavigation for the given navigation ID related to the
  // NavigationHandle.
  std::map<int64_t, previews::PreviewsUserData> inflight_previews_user_datas_;

  base::WeakPtrFactory<PreviewsUITabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PreviewsUITabHelper);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_UI_TAB_HELPER_H_
