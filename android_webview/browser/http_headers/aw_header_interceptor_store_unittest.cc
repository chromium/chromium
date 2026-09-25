// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/http_headers/aw_header_interceptor_store.h"

#include <string>
#include <utility>

#include "android_webview/browser/request_matcher/aw_request_matcher.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_expected_support.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android_webview {

class AwHeaderInterceptorStoreTest : public testing::Test {
 public:
  AwHeaderInterceptorStoreTest() = default;
  ~AwHeaderInterceptorStoreTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<AwRequestMatcher> CreateMatcher(
      const std::string& url_pattern) {
    auto result = AwRequestMatcher::Create({url_pattern});
    EXPECT_OK(result);
    return std::move(result.value());
  }

  base::android::ScopedJavaGlobalRef<jobject> CreateJavaRef() {
    return base::android::ScopedJavaGlobalRef<jobject>();
  }
};

TEST_F(AwHeaderInterceptorStoreTest,
       AddInterceptor_MonotonicallyIncreasingIds) {
  AwHeaderInterceptorStore store;

  int32_t id0 = store.AddInterceptor(CreateJavaRef(),
                                     CreateMatcher("https://example.com/*"));
  int32_t id1 = store.AddInterceptor(CreateJavaRef(),
                                     CreateMatcher("https://google.com/*"));
  int32_t id2 = store.AddInterceptor(CreateJavaRef(),
                                     CreateMatcher("https://chromium.org/*"));

  EXPECT_EQ(0, id0);
  EXPECT_EQ(1, id1);
  EXPECT_EQ(2, id2);
  const auto& interceptors = store.GetInterceptors();
  ASSERT_EQ(3u, interceptors.size());
  EXPECT_EQ(0, interceptors[0]->id);
  EXPECT_EQ(1, interceptors[1]->id);
  EXPECT_EQ(2, interceptors[2]->id);
}

TEST_F(AwHeaderInterceptorStoreTest,
       AddInterceptor_MonotonicallyIncreasingIdsAfterRemoval) {
  AwHeaderInterceptorStore store;

  int32_t id0 = store.AddInterceptor(CreateJavaRef(),
                                     CreateMatcher("https://example.com/*"));
  store.RemoveInterceptor(id0);
  int32_t id1 = store.AddInterceptor(CreateJavaRef(),
                                     CreateMatcher("https://google.com/*"));

  EXPECT_EQ(0, id0);
  EXPECT_EQ(1, id1);
  ASSERT_EQ(1u, store.GetInterceptors().size());
  EXPECT_EQ(1, store.GetInterceptors()[0]->id);
}

TEST_F(AwHeaderInterceptorStoreTest, RemoveInterceptor) {
  AwHeaderInterceptorStore store;

  int32_t id0 = store.AddInterceptor(CreateJavaRef(),
                                     CreateMatcher("https://example.com/*"));
  int32_t id1 = store.AddInterceptor(CreateJavaRef(),
                                     CreateMatcher("https://google.com/*"));
  int32_t id2 = store.AddInterceptor(CreateJavaRef(),
                                     CreateMatcher("https://chromium.org/*"));

  // Remove middle interceptor.
  store.RemoveInterceptor(id1);
  const auto& interceptors1 = store.GetInterceptors();
  ASSERT_EQ(2u, interceptors1.size());
  EXPECT_EQ(id0, interceptors1[0]->id);
  EXPECT_EQ(id2, interceptors1[1]->id);

  // Remove first interceptor.
  store.RemoveInterceptor(id0);
  const auto& interceptors2 = store.GetInterceptors();
  ASSERT_EQ(1u, interceptors2.size());
  EXPECT_EQ(id2, interceptors2[0]->id);

  // Remove last interceptor.
  store.RemoveInterceptor(id2);
  EXPECT_TRUE(store.GetInterceptors().empty());
}

TEST_F(AwHeaderInterceptorStoreTest, RemoveInterceptor_NonExistentId_NoOp) {
  AwHeaderInterceptorStore store;

  int32_t id0 = store.AddInterceptor(CreateJavaRef(),
                                     CreateMatcher("https://example.com/*"));
  store.RemoveInterceptor(999);

  ASSERT_EQ(1u, store.GetInterceptors().size());
  EXPECT_EQ(id0, store.GetInterceptors()[0]->id);
}

TEST_F(AwHeaderInterceptorStoreTest, RegistersMatcherAndInterceptorCorrectly) {
  AwHeaderInterceptorStore store;

  auto matcher = CreateMatcher("https://example.com/foo");
  AwRequestMatcher* matcher_unsafe_ptr = matcher.get();
  auto javaRef = CreateJavaRef();
  store.AddInterceptor(javaRef, std::move(matcher));
  const auto& interceptors = store.GetInterceptors();

  ASSERT_EQ(1u, interceptors.size());
  EXPECT_EQ(matcher_unsafe_ptr, interceptors[0]->request_matcher.get());
  EXPECT_EQ(javaRef.obj(), interceptors[0]->interceptor.obj());
}

TEST_F(AwHeaderInterceptorStoreTest, ClearInterceptors) {
  AwHeaderInterceptorStore store;
  store.AddInterceptor(CreateJavaRef(), CreateMatcher("https://example.com/*"));
  store.AddInterceptor(CreateJavaRef(), CreateMatcher("https://google.com/*"));

  store.ClearInterceptors();

  EXPECT_TRUE(store.GetInterceptors().empty());
}

}  // namespace android_webview
