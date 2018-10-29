// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/chrome_resource_dispatcher_host_delegate.h"

#include <memory>

#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/loader/chrome_resource_dispatcher_host_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ChromeResourceDispatcherHostDelegateTest : public testing::Test {
 public:
  ChromeResourceDispatcherHostDelegateTest()
      : profile_manager_(
            new TestingProfileManager(TestingBrowserProcess::GetGlobal())) {}
  ~ChromeResourceDispatcherHostDelegateTest() override {}

  void SetUp() override { ASSERT_TRUE(profile_manager_->SetUp()); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  // Set up TestingProfileManager for extensions::UserScriptListener.
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(ChromeResourceDispatcherHostDelegateTest,
       GetNavigationDataWithDataReductionProxyData) {
  std::unique_ptr<net::URLRequestContext> context =
      std::make_unique<net::URLRequestContext>();
  std::unique_ptr<net::URLRequest> fake_request(
      context->CreateRequest(GURL("google.com"), net::RequestPriority::IDLE,
                             nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
  // Add DataReductionProxyData to URLRequest
  data_reduction_proxy::DataReductionProxyData* data_reduction_proxy_data =
      data_reduction_proxy::DataReductionProxyData::GetDataAndCreateIfNecessary(
          fake_request.get());
  data_reduction_proxy_data->set_used_data_reduction_proxy(true);
  std::unique_ptr<ChromeResourceDispatcherHostDelegate> delegate =
      std::make_unique<ChromeResourceDispatcherHostDelegate>();
  ChromeNavigationData* chrome_navigation_data =
      static_cast<ChromeNavigationData*>(
          delegate->GetNavigationData(fake_request.get()));
  data_reduction_proxy::DataReductionProxyData* data_reduction_proxy_data_copy =
      chrome_navigation_data->GetDataReductionProxyData();
  // The DataReductionProxyData should be a copy of the one on URLRequest
  EXPECT_NE(data_reduction_proxy_data_copy, data_reduction_proxy_data);
  // Make sure DataReductionProxyData was copied.
  EXPECT_TRUE(data_reduction_proxy_data_copy->used_data_reduction_proxy());
  EXPECT_EQ(
      chrome_navigation_data,
      ChromeNavigationData::GetDataAndCreateIfNecessary(fake_request.get()));
}

TEST_F(ChromeResourceDispatcherHostDelegateTest,
       GetNavigationDataWithoutDataReductionProxyData) {
  std::unique_ptr<net::URLRequestContext> context =
      std::make_unique<net::URLRequestContext>();
  std::unique_ptr<net::URLRequest> fake_request(
      context->CreateRequest(GURL("google.com"), net::RequestPriority::IDLE,
                             nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<ChromeResourceDispatcherHostDelegate> delegate =
      std::make_unique<ChromeResourceDispatcherHostDelegate>();
  ChromeNavigationData* chrome_navigation_data =
      static_cast<ChromeNavigationData*>(
          delegate->GetNavigationData(fake_request.get()));
  EXPECT_FALSE(chrome_navigation_data->GetDataReductionProxyData());
}
