// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ARC_ARC_ICON_CACHE_H_
#define CHROME_BROWSER_LACROS_ARC_ARC_ICON_CACHE_H_

#include <string>
#include <vector>

#include "base/threading/thread_checker.h"
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "components/arc/common/intent_helper/activity_icon_loader.h"
#include "components/arc/common/intent_helper/arc_icon_cache_delegate.h"
#include "mojo/public/cpp/bindings/receiver.h"

// This class receives arc icon info updates from Ash. It can only be used on
// the main thread.
class ArcIconCache : public arc::ArcIconCacheDelegate,
                     public crosapi::mojom::ArcObserver {
 public:
  ArcIconCache();
  ArcIconCache(const ArcIconCache&) = delete;
  ArcIconCache& operator=(const ArcIconCache&) = delete;
  ~ArcIconCache() override;

  // Start observing ARC in ash-chrome.
  void Start();

  // arc::ArcIconCacheDelegate:
  // Retrieves icons for the |activities| and calls |cb|.
  // See ActivityIconLoader::GetActivityIcons() for more details.
  GetResult GetActivityIcons(const std::vector<ActivityName>& activities,
                             OnIconsReadyCallback cb) override;

 private:
  THREAD_CHECKER(thread_checker_);

  // crosapi::mojom::ArcObserver
  void OnIconInvalidated(const std::string& package_name) override;

  // Cached activity icons.
  ActivityIconLoader icon_loader_;

  // Receives mojo messages from ash-chrome.
  mojo::Receiver<crosapi::mojom::ArcObserver> receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_ARC_ARC_ICON_CACHE_H_
