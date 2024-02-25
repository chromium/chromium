// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_FAKE_QUICK_PAIR_BROWSER_DELEGATE_H_
#define ASH_QUICK_PAIR_COMMON_FAKE_QUICK_PAIR_BROWSER_DELEGATE_H_

#include "ash/quick_pair/common/quick_pair_browser_delegate.h"

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::quick_pair {

class FakeQuickPairBrowserDelegate : public QuickPairBrowserDelegate {
 public:
  FakeQuickPairBrowserDelegate();
  FakeQuickPairBrowserDelegate(const FakeQuickPairBrowserDelegate&) = delete;
  FakeQuickPairBrowserDelegate& operator=(const FakeQuickPairBrowserDelegate&) =
      delete;
  ~FakeQuickPairBrowserDelegate() override;

  static FakeQuickPairBrowserDelegate* Get();

  void SetIdentityManager(signin::IdentityManager* identity_manager);
  void SetCompanionAppInstalled(const std::string& app_id, bool installed);

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
  TestingPrefServiceSimple pref_service_;
  raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_ =
      nullptr;
  std::map<std::string, bool> companion_app_installed_ = {};
};

}  // namespace ash::quick_pair

#endif  // ASH_QUICK_PAIR_COMMON_FAKE_QUICK_PAIR_BROWSER_DELEGATE_H_
