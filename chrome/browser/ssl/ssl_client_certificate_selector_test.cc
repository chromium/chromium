// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_certificate_selector_test.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/request_priority.h"
#include "net/http/http_transaction_factory.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using ::testing::Mock;
using ::testing::StrictMock;
using content::BrowserThread;

SSLClientCertificateSelectorTestBase::SSLClientCertificateSelectorTestBase()
    : io_loop_finished_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED) {
}

SSLClientCertificateSelectorTestBase::~SSLClientCertificateSelectorTestBase() {
}

void SSLClientCertificateSelectorTestBase::SetUpInProcessBrowserTestFixture() {
  cert_request_info_ = new net::SSLCertRequestInfo;
  cert_request_info_->host_and_port = net::HostPortPair("foo", 123);
}

void SSLClientCertificateSelectorTestBase::SetUpOnMainThread() {
  url_request_context_getter_ = browser()->profile()->GetRequestContext();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&SSLClientCertificateSelectorTestBase::SetUpOnIOThread,
                     base::Unretained(this)));

  io_loop_finished_event_.Wait();

  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Have to release our reference to the auth handler during the test to allow
// it to be destroyed while the Browser and its IO thread still exist.
void SSLClientCertificateSelectorTestBase::TearDownOnMainThread() {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&SSLClientCertificateSelectorTestBase::TearDownOnIOThread,
                     base::Unretained(this)));

  io_loop_finished_event_.Wait();

  auth_requestor_ = NULL;
}

void SSLClientCertificateSelectorTestBase::SetUpOnIOThread() {
  url_request_ = MakeURLRequest(url_request_context_getter_.get()).release();

  auth_requestor_ = new StrictMock<SSLClientAuthRequestorMock>(
      url_request_, cert_request_info_.get());

  io_loop_finished_event_.Signal();
}

void SSLClientCertificateSelectorTestBase::TearDownOnIOThread() {
  delete url_request_;

  io_loop_finished_event_.Signal();
}

std::unique_ptr<net::URLRequest>
SSLClientCertificateSelectorTestBase::MakeURLRequest(
    net::URLRequestContextGetter* context_getter) {
  std::unique_ptr<net::URLRequest> request =
      context_getter->GetURLRequestContext()->CreateRequest(
          GURL("https://example"), net::DEFAULT_PRIORITY, NULL,
          TRAFFIC_ANNOTATION_FOR_TESTS);
  return request;
}
