// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_data.h"

#include <utility>

InstallableData::InstallableData(std::vector<InstallableStatusCode> errors,
                                 const GURL& manifest_url,
                                 const blink::Manifest* manifest,
                                 const GURL& primary_icon_url,
                                 const SkBitmap* primary_icon,
                                 bool has_maskable_primary_icon,
                                 const GURL& badge_icon_url,
                                 const SkBitmap* badge_icon,
                                 bool valid_manifest,
                                 bool has_worker)
    : errors(std::move(errors)),
      manifest_url(manifest_url),
      manifest(manifest),
      primary_icon_url(primary_icon_url),
      primary_icon(primary_icon),
      has_maskable_primary_icon(has_maskable_primary_icon),
      badge_icon_url(badge_icon_url),
      badge_icon(badge_icon),
      valid_manifest(valid_manifest),
      has_worker(has_worker) {}

InstallableData::~InstallableData() = default;
