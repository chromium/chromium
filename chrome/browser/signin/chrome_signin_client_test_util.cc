// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client_test_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

std::unique_ptr<KeyedService> BuildChromeSigninClientWithURLLoader(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto signin_client = std::make_unique<ChromeSigninClient>(profile);
  signin_client->SetURLLoaderFactoryForTest(
      test_url_loader_factory->GetSafeWeakWrapper());
  return signin_client;
}
