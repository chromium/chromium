// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_MOCK_QUICK_PAIR_BROWSER_DELEGATE_H_
#define ASH_QUICK_PAIR_COMMON_MOCK_QUICK_PAIR_BROWSER_DELEGATE_H_

#include <string>

#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash {
namespace quick_pair {

class MockQuickPairBrowserDelegate : public QuickPairBrowserDelegate {
 public:
  MockQuickPairBrowserDelegate();
  MockQuickPairBrowserDelegate(const MockQuickPairBrowserDelegate&) = delete;
  MockQuickPairBrowserDelegate& operator=(const MockQuickPairBrowserDelegate&) =
      delete;
  ~MockQuickPairBrowserDelegate() override;

  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(std::unique_ptr<image_fetcher::ImageFetcher>,
              GetImageFetcher,
              (),
              (override));
  MOCK_METHOD(PrefService*, GetActivePrefService, (), (override));
  MOCK_METHOD(void,
              RequestService,
              (mojo::PendingReceiver<mojom::QuickPairService>),
              (override));
  MOCK_METHOD(bool,
              CompanionAppInstalled,
              (const std::string& app_id),
              (override));
  MOCK_METHOD(void,
              LaunchCompanionApp,
              (const std::string& app_id),
              (override));
  MOCK_METHOD(void, OpenPlayStorePage, (GURL play_store_uri), (override));
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_MOCK_QUICK_PAIR_BROWSER_DELEGATE_H_
