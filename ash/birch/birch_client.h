// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_CLIENT_H_
#define ASH_BIRCH_BIRCH_CLIENT_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "ui/base/models/image_model.h"

class GURL;

namespace base {
class FilePath;
}

namespace ui {
class ImageModel;
}

namespace ash {

class BirchDataProvider;

// Interface to communicate with the birch keyed service.
class ASH_EXPORT BirchClient {
 public:
  virtual BirchDataProvider* GetCalendarProvider() = 0;
  virtual BirchDataProvider* GetFileSuggestProvider() = 0;
  virtual BirchDataProvider* GetRecentTabsProvider() = 0;
  virtual BirchDataProvider* GetLastActiveProvider() = 0;
  virtual BirchDataProvider* GetMostVisitedProvider() = 0;
  virtual BirchDataProvider* GetSelfShareProvider() = 0;
  virtual BirchDataProvider* GetLostMediaProvider() = 0;
  virtual BirchDataProvider* GetReleaseNotesProvider() = 0;

  // Waits for refresh tokens to be loaded then calls `callback`. Calls
  // `callback` immediately if tokens are already loaded. Only one waiter
  // at a time is supported.
  virtual void WaitForRefreshTokens(base::OnceClosure callback) = 0;

  // Returns the path on disk where removed items are read from and written to.
  virtual base::FilePath GetRemovedItemsFilePath() = 0;

  // Prevents a file item from showing up in launcher zero suggest. For Google
  // Drive files the path looks like:
  // /media/fuse/drivefs-48de6bc248c2f6d8e809521347ef6190/root/Test doc.gdoc
  virtual void RemoveFileItemFromLauncher(const base::FilePath& path) = 0;

  // Attempts to load the favicon at the `url` with the FaviconService.
  // Invokes the callback either with a valid image (success) or an empty image
  // (failure).
  virtual void GetFaviconImage(
      const GURL& url,
      const bool is_page_url,
      base::OnceCallback<void(const ui::ImageModel&)> callback) = 0;

  // Retrieves the chrome icon to use as a backup icon when favicon loading
  // fails. Used by Birch Coral Item.
  virtual ui::ImageModel GetChromeBackupIcon() = 0;

  virtual ~BirchClient() = default;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_CLIENT_H_
