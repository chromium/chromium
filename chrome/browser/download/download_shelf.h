// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_ui_model.h"

class Browser;
class Profile;

namespace base {
class TimeDelta;
}  // namespace base

namespace offline_items_collection {
struct ContentId;
struct OfflineItem;
}  // namespace offline_items_collection

namespace views {
class View;
}

// This is an abstract base class for platform specific download shelf
// implementations.
class DownloadShelf {
 public:
  DownloadShelf(Browser* browser, Profile* profile);
  DownloadShelf(const DownloadShelf&) = delete;
  DownloadShelf& operator=(const DownloadShelf&) = delete;
  virtual ~DownloadShelf();

  // The browser view needs to know when we are going away to properly return
  // the resize corner size to WebKit so that we don't draw on top of it.
  // This returns the showing state of our animation which is set to true at
  // the beginning Show and false at the beginning of a Hide.
  virtual bool IsShowing() const = 0;

  // Returns whether the download shelf is showing the close animation.
  virtual bool IsClosing() const = 0;

  // A new download has started. Add it to our shelf and show the download
  // started animation.
  //
  // Some downloads are removed from the shelf on completion (See
  // DownloadItemModel::ShouldRemoveFromShelfWhenComplete()). These transient
  // downloads are added to the shelf after a delay. If the download completes
  // before the delay duration, it will not be added to the shelf at all.
  void AddDownload(DownloadUIModel::DownloadUIModelPtr download);

  // Opens the shelf.
  void Open();

  // Closes the shelf.
  void Close();

  // Closes the shelf and prevents it from reopening until Unhide() is called.
  void Hide();

  // Allows the shelf to open after a previous call to Hide().  Opens the shelf
  // if, had Hide() not been called, it would currently be open.
  void Unhide();

  Browser* browser() { return browser_; }
  virtual views::View* GetView() = 0;
  bool is_hidden() const { return is_hidden_; }

 protected:
  virtual void DoShowDownload(DownloadUIModel::DownloadUIModelPtr download) = 0;
  virtual void DoOpen() = 0;
  virtual void DoClose() = 0;
  virtual void DoHide() = 0;
  virtual void DoUnhide() = 0;

  // Time delay to wait before adding a transient download to the shelf.
  // Protected virtual for testing.
  virtual base::TimeDelta GetTransientDownloadShowDelay() const;

  Profile* profile() { return profile_; }

 private:
  // Shows the download on the shelf immediately. Also displays the download
  // started animation if necessary.
  void ShowDownload(DownloadUIModel::DownloadUIModelPtr download);

  // Similar to ShowDownload() but refers to the download using an ID.
  void ShowDownloadById(const offline_items_collection::ContentId& id);

  // Callback used by ShowDownloadById() to trigger ShowDownload() once |item|
  // has been fetched.
  void OnGetDownloadDoneForOfflineItem(
      const std::optional<offline_items_collection::OfflineItem>& item);

  const raw_ptr<Browser> browser_;
  const raw_ptr<Profile> profile_;
  bool should_show_on_unhide_ = false;
  bool is_hidden_ = false;
  base::WeakPtrFactory<DownloadShelf> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
