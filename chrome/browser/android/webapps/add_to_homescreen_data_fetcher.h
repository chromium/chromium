// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_DATA_FETCHER_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_DATA_FETCHER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace favicon_base {
struct FaviconRawBitmapResult;
}

class InstallableManager;
struct InstallableData;
struct WebApplicationInfo;

// Aysnchronously fetches and processes data needed to create a shortcut for an
// Android Home screen launcher.
class AddToHomescreenDataFetcher : public content::WebContentsObserver {
 public:
  class Observer {
   public:
    // Called when the homescreen icon title (and possibly information from the
    // web manifest) is available.
    virtual void OnUserTitleAvailable(const base::string16& title,
                                      const GURL& url,
                                      bool is_webapk_compatible) = 0;

    // Called when all the data needed to prompt the user to add to home screen
    // is available.
    virtual void OnDataAvailable(const ShortcutInfo& info,
                                 const SkBitmap& primary_icon,
                                 const SkBitmap& badge_icon) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Initialize the fetcher by requesting the information about the page from
  // the renderer process. The initialization is asynchronous and
  // OnDidGetWebApplicationInfo is expected to be called when finished.
  // |observer| must outlive AddToHomescreenDataFetcher.
  AddToHomescreenDataFetcher(content::WebContents* web_contents,
                             int data_timeout_ms,
                             Observer* observer);

  ~AddToHomescreenDataFetcher() override;

  // IPC message received when the initialization is finished.
  void OnDidGetWebApplicationInfo(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame,
      const WebApplicationInfo& web_app_info);

  // Accessors, etc.
  const SkBitmap& badge_icon() const { return badge_icon_; }
  const SkBitmap& primary_icon() const { return primary_icon_; }
  ShortcutInfo& shortcut_info() { return shortcut_info_; }
  bool has_maskable_primary_icon() const { return has_maskable_primary_icon_; }

 private:
  // Called to stop the timeout timer.
  void StopTimer();

  // Called if either InstallableManager or the favicon fetch takes too long.
  void OnDataTimedout();

  // Called when InstallableManager finishes looking for a manifest and icon.
  void OnDidGetManifestAndIcons(const InstallableData& data);

  // Called when InstallableManager finishes checking for installability.
  void OnDidPerformInstallableCheck(const InstallableData& data);

  // Grabs the favicon for the current URL.
  void FetchFavicon();
  void OnFaviconFetched(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Creates an icon to display to the user to confirm the add to home screen
  // from the given |base_icon|. If |use_for_launcher| is true, the created icon
  // will also be used as the launcher icon.
  void CreateIconForView(const SkBitmap& base_icon, bool use_for_launcher);

  // Notifies the observer that the shortcut data is all available.
  void OnIconCreated(bool use_for_launcher,
                     const SkBitmap& icon_for_view,
                     bool is_icon_generated);

  InstallableManager* installable_manager_;
  Observer* observer_;

  // The icons must only be set on the UI thread for thread safety.
  SkBitmap raw_primary_icon_;
  SkBitmap badge_icon_;
  SkBitmap primary_icon_;
  ShortcutInfo shortcut_info_;
  bool has_maskable_primary_icon_;

  base::CancelableTaskTracker favicon_task_tracker_;
  base::OneShotTimer data_timeout_timer_;
  base::TimeTicks start_time_;

  const base::TimeDelta data_timeout_ms_;

  bool is_waiting_for_manifest_;

  base::WeakPtrFactory<AddToHomescreenDataFetcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AddToHomescreenDataFetcher);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_DATA_FETCHER_H_
