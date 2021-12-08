// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ARC_ICON_CACHE_LACROS_H_
#define CHROME_BROWSER_LACROS_ARC_ICON_CACHE_LACROS_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "components/arc/common/intent_helper/activity_icon_loader.h"
#include "mojo/public/cpp/bindings/receiver.h"

// This class receives arc icon info updates from Ash. It can only be used on
// the main thread.
class ArcIconCacheLacros : public crosapi::mojom::ArcObserver {
 public:
  ArcIconCacheLacros();
  ArcIconCacheLacros(const ArcIconCacheLacros&) = delete;
  ArcIconCacheLacros& operator=(const ArcIconCacheLacros&) = delete;
  ~ArcIconCacheLacros() override;

  // Start observing ARC in ash-chrome.
  void Start();

 private:
  THREAD_CHECKER(thread_checker_);

  // crosapi::mojom::ArcObserver
  void OnIconInvalidated(const std::string& package_name) override;

  // Cached activity icons.
  arc::internal::ActivityIconLoader icon_loader_;

  // Receives mojo messages from ash-chrome.
  mojo::Receiver<crosapi::mojom::ArcObserver> receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_ARC_ICON_CACHE_LACROS_H_
