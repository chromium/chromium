// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ARC_ARC_ICON_CACHE_H_
#define CHROME_BROWSER_LACROS_ARC_ARC_ICON_CACHE_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "components/arc/common/intent_helper/activity_icon_loader.h"
#include "components/arc/common/intent_helper/link_handler_model_delegate.h"
#include "mojo/public/cpp/bindings/receiver.h"

// This class receives arc icon info updates from Ash. It can only be used on
// the main thread.
class ArcIconCache : public arc::LinkHandlerModelDelegate,
                     public crosapi::mojom::ArcObserver {
 public:
  ArcIconCache();
  ArcIconCache(const ArcIconCache&) = delete;
  ArcIconCache& operator=(const ArcIconCache&) = delete;
  ~ArcIconCache() override;

  // Start observing ARC in ash-chrome.
  void Start();

  // arc::LinkHandlerModelDelegate:
  // Retrieves icons for the |activities| and calls |cb|.
  // See ActivityIconLoader::GetActivityIcons() for more details.
  GetResult GetActivityIcons(const std::vector<ActivityName>& activities,
                             OnIconsReadyCallback cb) override;
  // Calls RequestUrlHandlerList mojo API.
  bool RequestUrlHandlerList(const std::string& url,
                             RequestUrlHandlerListCallback callback) override;
  // Calls HandleUrl mojo API.
  bool HandleUrl(const std::string& url,
                 const std::string& package_name) override;

 private:
  THREAD_CHECKER(thread_checker_);

  // crosapi::mojom::ArcObserver
  void OnIconInvalidated(const std::string& package_name) override;

  // Convert vector of crosapi::mojom::IntentHandlerInfoPtr to vector of
  // ArcIconCacheDelegate::IntentHandlerInfo.
  void OnRequestUrlHandlerList(
      RequestUrlHandlerListCallback callback,
      std::vector<crosapi::mojom::IntentHandlerInfoPtr> handlers,
      crosapi::mojom::RequestUrlHandlerListStatus status);

  // Cached activity icons.
  ActivityIconLoader icon_loader_;

  // Receives mojo messages from ash-chrome.
  mojo::Receiver<crosapi::mojom::ArcObserver> receiver_{this};

  // This must come last to make sure weak pointers are invalidated first.
  base::WeakPtrFactory<ArcIconCache> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_ARC_ARC_ICON_CACHE_H_
