// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/installable/installable_logging.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

// This struct contains the results of an InstallableManager::GetData call and
// is passed to an InstallableCallback. Each pointer and reference is owned by
// InstallableManager, and callers should copy any objects which they wish to
// use later. Fields not requested in GetData may or may not be set.
struct InstallableData {
  InstallableData(std::vector<InstallableStatusCode> errors,
                  const GURL& manifest_url,
                  const blink::Manifest* manifest,
                  const GURL& primary_icon_url,
                  const SkBitmap* primary_icon,
                  bool has_maskable_primary_icon,
                  const GURL& badge_icon_url,
                  const SkBitmap* badge_icon,
                  bool valid_manifest,
                  bool has_worker);
  ~InstallableData();

  // Contains all errors encountered during the InstallableManager::GetData
  // call. Empty if no errors were encountered.
  std::vector<InstallableStatusCode> errors;

  // The URL of the the web app manifest. Empty if the site has no
  // <link rel="manifest"> tag.
  const GURL& manifest_url;

  // The parsed web app manifest. nullptr if the site has an unparseable
  // manifest.
  const blink::Manifest* manifest;

  // The URL of the chosen primary icon.
  const GURL& primary_icon_url;

  // nullptr if the most appropriate primary icon couldn't be determined or
  // downloaded. The underlying primary icon is owned by the InstallableManager;
  // clients must copy the bitmap if they want to to use it.
  const SkBitmap* primary_icon;

  // Whether the primary icon had the 'maskable' purpose, meaningless if no
  // primary_icon was requested.
  const bool has_maskable_primary_icon;

  // The URL of the chosen badge icon.
  const GURL& badge_icon_url;

  // nullptr if the most appropriate badge icon couldn't be determined or
  // downloaded. The underlying badge icon is owned by the InstallableManager;
  // clients must copy the bitmap if they want to to use it. Since the badge
  // icon is optional, no error code is set if it cannot be fetched, and clients
  // specifying |valid_badge_icon| must check that the bitmap exists before
  // using it.
  const SkBitmap* badge_icon;

  // true if the site has a valid, installable web app manifest. If
  // |valid_manifest| or |has_worker| was true and the site isn't installable,
  // the reason will be in |errors|.
  const bool valid_manifest = false;

  // true if the site has a service worker with a fetch handler.
  const bool has_worker = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallableData);
};

using InstallableCallback = base::OnceCallback<void(const InstallableData&)>;

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_
