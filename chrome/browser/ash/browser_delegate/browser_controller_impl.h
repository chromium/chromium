// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_IMPL_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class Browser;
class BrowserList;

namespace ash {

class BrowserDelegate;
class BrowserDelegateImpl;

class BrowserControllerImpl : public BrowserController,
                              public BrowserListObserver {
 public:
  BrowserControllerImpl();
  ~BrowserControllerImpl() override;

  // BrowserController:
  BrowserDelegate* NewTabWithPostData(user_manager::User& user,
                                      const GURL& url,
                                      base::span<const uint8_t> post_data,
                                      std::string_view extra_headers) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

 private:
  BrowserDelegate* GetBrowserDelegate(Browser* browser);

  absl::flat_hash_map<Browser*, std::unique_ptr<BrowserDelegateImpl>> browsers_;

  base::ScopedObservation<BrowserList, BrowserListObserver> observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_IMPL_H_
