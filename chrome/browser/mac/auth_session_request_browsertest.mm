// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/base/mac/url_conversions.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest_mac.h"
#include "ui/base/page_transition_types.h"

using AuthSessionBrowserTest = InProcessBrowserTest;

@interface MockASWebAuthenticationSessionRequest : NSObject {
  base::scoped_nsobject<NSUUID> _uuid;
  base::scoped_nsobject<NSURL> _initialURL;

  base::scoped_nsobject<NSURL> _callbackURL;
  base::scoped_nsobject<NSError> _cancellationError;
}

// ASWebAuthenticationSessionRequest:

@property(readonly, nonatomic) NSURL* URL;
@property(readonly, nonatomic) BOOL shouldUseEphemeralSession;
@property(nullable, readonly, nonatomic, copy) NSString* callbackURLScheme;
@property(readonly, nonatomic) NSUUID* UUID;

- (void)completeWithCallbackURL:(NSURL*)url;
- (void)cancelWithError:(NSError*)error;

// Utilities:

@property(readonly, nonatomic) NSURL* callbackURL;
@property(readonly, nonatomic) NSError* cancellationError;

@end

@implementation MockASWebAuthenticationSessionRequest

- (instancetype)initWithInitialURL:(NSURL*)initialURL {
  if (self = [super init]) {
    _uuid.reset([[NSUUID alloc] init]);
    _initialURL.reset(initialURL, base::scoped_policy::RETAIN);
  }
  return self;
}

- (NSURL*)URL {
  return _initialURL;
}

- (BOOL)shouldUseEphemeralSession {
  return false;
}

- (NSString*)callbackURLScheme {
  return @"makeitso";
}

- (NSUUID*)UUID {
  return _uuid.get();
}

- (void)completeWithCallbackURL:(NSURL*)url {
  _callbackURL.reset(url, base::scoped_policy::RETAIN);
}

- (void)cancelWithError:(NSError*)error {
  _cancellationError.reset(error, base::scoped_policy::RETAIN);
}

- (NSURL*)callbackURL {
  return _callbackURL.get();
}

- (NSError*)cancellationError {
  return _cancellationError.get();
}

@end

// Tests that an OS request to cancel an auth session works.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, OSCancellation) {
  if (@available(macOS 10.15, *)) {
    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:[NSURL URLWithString:@"about:blank"]]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);

    // Ask the app controller to start handling our session request.

    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect a browser window to be opened.

    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_EQ(start_browser_count + 1, browser_list->size());

    // Ask the app controller to stop handling our session request.

    [session_handler cancelWebAuthenticationSessionRequest:request];

    // Expect the browser window to close.

    ui_test_utils::WaitForBrowserToClose(browser);
    EXPECT_EQ(start_browser_count, browser_list->size());

    // Expect there to not have been any callbacks.

    EXPECT_EQ(nil, session_request.get().callbackURL);
    EXPECT_EQ(nil, session_request.get().cancellationError);
  }
}

// Tests that a user request to cancel an auth session works.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, UserCancellation) {
  if (@available(macOS 10.15, *)) {
    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:[NSURL URLWithString:@"about:blank"]]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);

    // Ask the app controller to start handling our session request.

    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect a browser window to be opened.

    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_EQ(start_browser_count + 1, browser_list->size());

    // Simulate the user closing the window.

    browser->window()->Close();

    // Expect the browser window to close.

    ui_test_utils::WaitForBrowserToClose(browser);
    EXPECT_EQ(start_browser_count, browser_list->size());

    // Expect there to have been the user cancellation callback.

    EXPECT_EQ(nil, session_request.get().callbackURL);
    ASSERT_NE(nil, session_request.get().cancellationError);
    EXPECT_EQ(ASWebAuthenticationSessionErrorDomain,
              session_request.get().cancellationError.domain);
    EXPECT_EQ(ASWebAuthenticationSessionErrorCodeCanceledLogin,
              session_request.get().cancellationError.code);
  }
}

// Tests that a successful auth session works via direct navigation.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, UserSuccessDirect) {
  if (@available(macOS 10.15, *)) {
    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:[NSURL URLWithString:@"about:blank"]]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);

    // Ask the app controller to start handling our session request.

    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect a browser window to be opened.

    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_EQ(start_browser_count + 1, browser_list->size());

    // Simulate the user successfully logging in with a non-redirected load of
    // a URL with the expected scheme.

    GURL success_url("makeitso://enterprise");
    browser->tab_strip_model()->GetWebContentsAt(0)->GetController().LoadURL(
        success_url, content::Referrer(), ui::PAGE_TRANSITION_GENERATED,
        std::string());

    // Expect the browser window to close.

    ui_test_utils::WaitForBrowserToClose(browser);
    EXPECT_EQ(start_browser_count, browser_list->size());

    // Expect there to have been the success callback.

    ASSERT_NE(nil, session_request.get().callbackURL);
    EXPECT_EQ(nil, session_request.get().cancellationError);
    EXPECT_NSEQ(net::NSURLWithGURL(success_url),
                session_request.get().callbackURL);
  }
}

namespace {

std::unique_ptr<net::test_server::HttpResponse> RedirectionRequestHandler(
    const GURL& redirection_url,
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_FOUND);
  http_response->AddCustomHeader("Location", redirection_url.spec());
  return http_response;
}

}  // namespace

// Tests that a successful auth session works via a redirect.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, UserSuccessEventualRedirect) {
  if (@available(macOS 10.15, *)) {
    GURL success_url("makeitso://cerritos");

    net::EmbeddedTestServer embedded_test_server;
    embedded_test_server.RegisterRequestHandler(
        base::BindRepeating(RedirectionRequestHandler, success_url));
    ASSERT_TRUE(embedded_test_server.Start());

    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:[NSURL URLWithString:@"about:blank"]]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);

    // Ask the app controller to start handling our session request.

    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect a browser window to be opened.

    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_EQ(start_browser_count + 1, browser_list->size());

    // Simulate the user successfully logging in with a redirected load of a URL
    // with the expected scheme.

    GURL url = embedded_test_server.GetURL("/something");
    browser->tab_strip_model()->GetWebContentsAt(0)->GetController().LoadURL(
        url, content::Referrer(), ui::PAGE_TRANSITION_GENERATED, std::string());

    // Expect the browser window to close.

    ui_test_utils::WaitForBrowserToClose(browser);
    EXPECT_EQ(start_browser_count, browser_list->size());

    // Expect there to have been the success callback.

    ASSERT_NE(nil, session_request.get().callbackURL);
    EXPECT_EQ(nil, session_request.get().cancellationError);
    EXPECT_NSEQ(net::NSURLWithGURL(success_url),
                session_request.get().callbackURL);
  }
}

// Tests that a successful auth session works if the success scheme comes on a
// redirect from the initial navigation.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, UserSuccessInitialRedirect) {
  if (@available(macOS 10.15, *)) {
    GURL success_url("makeitso://titan");

    net::EmbeddedTestServer embedded_test_server;
    embedded_test_server.RegisterRequestHandler(
        base::BindRepeating(RedirectionRequestHandler, success_url));
    ASSERT_TRUE(embedded_test_server.Start());

    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    GURL url = embedded_test_server.GetURL("/something");
    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:net::NSURLWithGURL(url)]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);

    // Ask the app controller to start handling our session request.

    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect a browser window to be opened.

    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_EQ(start_browser_count + 1, browser_list->size());

    // Expect the browser window to close.

    ui_test_utils::WaitForBrowserToClose(browser);
    EXPECT_EQ(start_browser_count, browser_list->size());

    // Expect there to have been the success callback.

    ASSERT_NE(nil, session_request.get().callbackURL);
    EXPECT_EQ(nil, session_request.get().cancellationError);
    EXPECT_NSEQ(net::NSURLWithGURL(success_url),
                session_request.get().callbackURL);
  }
}
