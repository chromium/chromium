// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_QUICK_PAIR_QUICK_PAIR_BROWSER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_QUICK_PAIR_QUICK_PAIR_BROWSER_DELEGATE_IMPL_H_

#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "chromeos/ash/services/quick_pair/public/mojom/quick_pair_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class Profile;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {
namespace quick_pair {

// QuickPairBrowserDelegate implementation which handles all browser dependency
// requests for the Quick Pair system.
class QuickPairBrowserDelegateImpl final : public QuickPairBrowserDelegate {
 public:
  QuickPairBrowserDelegateImpl();
  QuickPairBrowserDelegateImpl(const QuickPairBrowserDelegateImpl&) = delete;
  QuickPairBrowserDelegateImpl& operator=(const QuickPairBrowserDelegateImpl*) =
      delete;
  ~QuickPairBrowserDelegateImpl() override;

  // QuickPairBrowserDelegate:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  signin::IdentityManager* GetIdentityManager() override;
  std::unique_ptr<image_fetcher::ImageFetcher> GetImageFetcher() override;
  PrefService* GetActivePrefService() override;
  void RequestService(
      mojo::PendingReceiver<mojom::QuickPairService> receiver) override;
  bool CompanionAppInstalled(const std::string& app_id) override;
  void LaunchCompanionApp(const std::string& app_id) override;
  void OpenPlayStorePage(GURL play_store_uri) override;

 private:
  Profile* GetActiveProfile();
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_QUICK_PAIR_QUICK_PAIR_BROWSER_DELEGATE_IMPL_H_
