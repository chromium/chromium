// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client_test_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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

// --- ChromeSigninClientWithURLLoaderHelper -----------------------------------

void ChromeSigninClientWithURLLoaderHelper::SetUp() {
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&ChromeSigninClientWithURLLoaderHelper::
                                      OnWillCreateBrowserContextServices,
                                  base::Unretained(this)));
}

void ChromeSigninClientWithURLLoaderHelper::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  // Clear the previous cookie responses (if any) before using it for a
  // new profile (as test_url_loader_factory() is shared across profiles).
  test_url_loader_factory_.ClearResponses();
  ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                   &test_url_loader_factory_));
}
