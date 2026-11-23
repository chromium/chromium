// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/network_context_service.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/network_context_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

std::string GetEchoedCookies(network::SharedURLLoaderFactory* loader_factory,
                             const net::EmbeddedTestServer& test_server) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = test_server.GetURL("/echoheader?Cookie");
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  base::test::TestFuture<std::optional<std::string>> future;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(loader_factory,
                                                          future.GetCallback());
  std::optional<std::string> body = future.Take();
  CHECK(body.has_value());
  return *std::move(body);
}

void SetSafeBrowsingCookie(network::SharedURLLoaderFactory* loader_factory,
                           const net::EmbeddedTestServer& test_server,
                           std::string_view cookie) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = test_server.GetURL(base::StrCat(
      {"/set-cookie?", cookie, ";max-age=3600;SameSite=None;Secure"}));
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  base::test::TestFuture<scoped_refptr<net::HttpResponseHeaders>> future;
  loader->DownloadHeadersOnly(loader_factory, future.GetCallback());
  ASSERT_NE(nullptr, future.Get());
}

}  // namespace

using SafeBrowsingNetworkContextServiceBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SafeBrowsingNetworkContextServiceBrowserTest,
                       IsolatesOffTheRecordCookiesAndClearsOnSessionClose) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile* regular_profile = browser()->GetProfile();
  scoped_refptr<network::SharedURLLoaderFactory> regular_sb_factory =
      g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
          regular_profile);
  ASSERT_NE(nullptr, regular_sb_factory);

  BrowserWindowInterface* incognito_browser =
      CreateIncognitoBrowser(regular_profile);
  ASSERT_TRUE(incognito_browser);
  Profile* incognito_profile = incognito_browser->GetProfile();

  // `SafeBrowsingService::GetURLLoaderFactory()` continues redirecting
  // off-the-record profiles to the original profile via
  // `NetworkContextServiceFactory`.
  EXPECT_EQ(regular_sb_factory,
            g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
                incognito_profile));

  NetworkContextService* incognito_otr_service =
      NetworkContextServiceFactory::GetForBrowserContext(
          incognito_profile, /*use_same_instance_in_off_the_record=*/false);
  ASSERT_NE(nullptr, incognito_otr_service);
  scoped_refptr<network::SharedURLLoaderFactory> incognito_otr_factory =
      incognito_otr_service->GetURLLoaderFactory();
  ASSERT_NE(nullptr, incognito_otr_factory);

  // Off-the-record Safe Browsing cookies are isolated from Regular.
  SetSafeBrowsingCookie(incognito_otr_factory.get(), *embedded_test_server(),
                        "incognito_sb=1");
  EXPECT_EQ("incognito_sb=1", GetEchoedCookies(incognito_otr_factory.get(),
                                               *embedded_test_server()));
  EXPECT_EQ("None", GetEchoedCookies(regular_sb_factory.get(),
                                     *embedded_test_server()));

  // Regular profile Safe Browsing cookies remain isolated from Incognito's
  // off-the-record Safe Browsing network context.
  SetSafeBrowsingCookie(regular_sb_factory.get(), *embedded_test_server(),
                        "regular_sb=1");
  EXPECT_EQ("regular_sb=1", GetEchoedCookies(regular_sb_factory.get(),
                                             *embedded_test_server()));
  EXPECT_EQ("incognito_sb=1", GetEchoedCookies(incognito_otr_factory.get(),
                                               *embedded_test_server()));

  // Incognito Safe Browsing cookies disappear when the Incognito session
  // closes, while Regular cookies remain.
  CloseBrowserSynchronously(incognito_browser);
  BrowserWindowInterface* reopened_incognito_browser =
      CreateIncognitoBrowser(regular_profile);
  ASSERT_TRUE(reopened_incognito_browser);
  NetworkContextService* reopened_incognito_otr_service =
      NetworkContextServiceFactory::GetForBrowserContext(
          reopened_incognito_browser->GetProfile(),
          /*use_same_instance_in_off_the_record=*/false);
  ASSERT_NE(nullptr, reopened_incognito_otr_service);
  scoped_refptr<network::SharedURLLoaderFactory> reopened_incognito_factory =
      reopened_incognito_otr_service->GetURLLoaderFactory();
  ASSERT_NE(nullptr, reopened_incognito_factory);
  EXPECT_EQ("None", GetEchoedCookies(reopened_incognito_factory.get(),
                                     *embedded_test_server()));
  EXPECT_EQ("regular_sb=1", GetEchoedCookies(regular_sb_factory.get(),
                                             *embedded_test_server()));
}

}  // namespace safe_browsing
