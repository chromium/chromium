// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/continuous_search/internal/search_result_extractor_producer.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "chrome/browser/continuous_search/internal/search_result_extractor_producer_interface.h"
#include "chrome/browser/continuous_search/page_category.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/continuous_search/browser/test/fake_search_result_extractor.h"
#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/android/gurl_android.h"

namespace continuous_search {

namespace {

const char kUrl[] = "https://www.google.com/search?q=test";

mojom::CategoryResultsPtr GenerateValidResults(const GURL& document_url) {
  mojom::CategoryResultsPtr expected_results = mojom::CategoryResults::New();
  expected_results->document_url = document_url;
  expected_results->category_type = mojom::Category::kOrganic;
  {
    mojom::ResultGroupPtr result_group = mojom::ResultGroup::New();
    result_group->label = "Group 1";
    result_group->is_ad_group = false;
    {
      mojom::SearchResultPtr result = mojom::SearchResult::New();
      result->link = GURL("https://www.bar.com/");
      result->title = u"Bar";
      result_group->results.push_back(std::move(result));
    }
    expected_results->groups.push_back(std::move(result_group));
  }
  return expected_results;
}

class MockSearchResultExtractorProducerInterface
    : public SearchResultExtractorProducerInterface {
 public:
  MockSearchResultExtractorProducerInterface() = default;
  ~MockSearchResultExtractorProducerInterface() override = default;

  MOCK_METHOD(void,
              OnError,
              (JNIEnv * env,
               const base::android::JavaRef<jobject>& obj,
               jint status_code),
              (override));
  MOCK_METHOD(void,
              OnResultsAvailable,
              (JNIEnv * env,
               const base::android::JavaRef<jobject>& obj,
               const base::android::JavaRef<jobject>& url,
               const base::android::JavaRef<jstring>& query,
               jint result_type,
               const base::android::JavaRef<jobjectArray>& group_label,
               const base::android::JavaRef<jbooleanArray>& is_ad_group,
               const base::android::JavaRef<jintArray>& group_size,
               const base::android::JavaRef<jobjectArray>& titles,
               const base::android::JavaRef<jobjectArray>& urls),
              (override));
};

}  // namespace

class SearchResultExtractorProducerRenderViewHostTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SearchResultExtractorProducerRenderViewHostTest() = default;
  ~SearchResultExtractorProducerRenderViewHostTest() override = default;

  SearchResultExtractorProducerRenderViewHostTest(
      const SearchResultExtractorProducerRenderViewHostTest&) = delete;
  SearchResultExtractorProducerRenderViewHostTest& operator=(
      const SearchResultExtractorProducerRenderViewHostTest&) = delete;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               GURL(kUrl));
  }

  // Overrides the `mojom::SearchResultExtractor` on the main frame with
  // `extractor`. Note `extractor` should outlive any calls made to the
  // interface.
  void OverrideInterface(FakeSearchResultExtractor* extractor) {
    web_contents()
        ->GetMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::SearchResultExtractor::Name_,
            base::BindRepeating(&FakeSearchResultExtractor::BindRequest,
                                base::Unretained(extractor)));
  }
};

ACTION_P(Quit, quit) {
  std::move(quit).Run();
}

MATCHER_P(EqualsJavaGURL, url, "") {
  return url == *url::GURLAndroid::ToNativeGURL(
                    base::android::AttachCurrentThread(), arg);
}

MATCHER_P(EqualsJavaString, string, "") {
  return string == base::android::ConvertJavaStringToUTF8(
                       base::android::AttachCurrentThread(), arg);
}

MATCHER_P(EqualsJavaStringArray, string_array, "") {
  std::vector<std::string> out;
  base::android::AppendJavaStringArrayToStringVector(
      base::android::AttachCurrentThread(), arg, &out);
  return string_array.size() == out.size() &&
         std::equal(string_array.begin(), string_array.end(), out.begin());
}

MATCHER_P(EqualsJavaBooleanArray, boolean_array, "") {
  std::vector<bool> out;
  base::android::JavaBooleanArrayToBoolVector(
      base::android::AttachCurrentThread(), arg, &out);
  return boolean_array.size() == out.size() &&
         std::equal(boolean_array.begin(), boolean_array.end(), out.begin());
}

MATCHER_P(EqualsJavaIntArray, int_array, "") {
  std::vector<int> out;
  base::android::JavaIntArrayToIntVector(base::android::AttachCurrentThread(),
                                         arg, &out);
  return int_array.size() == out.size() &&
         std::equal(int_array.begin(), int_array.end(), out.begin());
}

MATCHER_P(EqualsJavaGURLArray, gurl_array, "") {
  JNIEnv* env = base::android::AttachCurrentThread();
  size_t size = env->GetArrayLength(arg.obj());
  std::vector<GURL> out(size);
  for (size_t i = 0; i < size; ++i) {
    out[i] = *url::GURLAndroid::ToNativeGURL(
        env, base::android::ScopedJavaLocalRef<jobject>(
                 env, env->GetObjectArrayElement(arg.obj(), i)));
  }

  return gurl_array.size() == out.size() &&
         std::equal(gurl_array.begin(), gurl_array.end(), out.begin());
}

TEST_F(SearchResultExtractorProducerRenderViewHostTest, FetchSuccess) {
  FakeSearchResultExtractor fake_extractor;
  mojom::CategoryResultsPtr results = GenerateValidResults(GURL(kUrl));
  fake_extractor.SetResponse(mojom::SearchResultExtractor::Status::kSuccess,
                             std::move(results));
  OverrideInterface(&fake_extractor);

  std::string kQuery = "test query";
  JNIEnv* env = base::android::AttachCurrentThread();
  auto mock_interface =
      std::make_unique<MockSearchResultExtractorProducerInterface>();
  base::RunLoop loop;
  EXPECT_CALL(*mock_interface,
              OnResultsAvailable(
                  ::testing::_, ::testing::_, EqualsJavaGURL(GURL(kUrl)),
                  EqualsJavaString(kQuery),
                  static_cast<jint>(PageCategory::kOrganicSrp),
                  EqualsJavaStringArray(std::vector<std::string>({"Group 1"})),
                  EqualsJavaBooleanArray(std::vector<bool>({false})),
                  EqualsJavaIntArray(std::vector<int>({1})),
                  EqualsJavaStringArray(std::vector<std::string>({"Bar"})),
                  EqualsJavaGURLArray(
                      std::vector<GURL>({GURL("https://www.bar.com/")}))))
      .Times(1)
      .WillOnce(Quit(loop.QuitClosure()));

  SearchResultExtractorProducer producer(env, nullptr,
                                         std::move(mock_interface));
  producer.FetchResults(
      env,
      base::android::JavaParamRef<jobject>(
          env, web_contents()->GetJavaWebContents().Release()),
      base::android::JavaParamRef<jstring>(
          env, base::android::ConvertUTF8ToJavaString(env, kQuery).Release()));
  loop.Run();
}

TEST_F(SearchResultExtractorProducerRenderViewHostTest, ErrorFetching) {
  FakeSearchResultExtractor fake_extractor;
  mojom::CategoryResultsPtr bad_results =
      GenerateValidResults(GURL("https://www.baz.com/"));
  fake_extractor.SetResponse(mojom::SearchResultExtractor::Status::kSuccess,
                             std::move(bad_results));
  OverrideInterface(&fake_extractor);

  std::string kQuery = "test query";
  JNIEnv* env = base::android::AttachCurrentThread();
  auto mock_interface =
      std::make_unique<MockSearchResultExtractorProducerInterface>();
  base::RunLoop loop;
  EXPECT_CALL(*mock_interface,
              OnError(::testing::_, ::testing::_,
                      static_cast<jint>(
                          SearchResultExtractorClientStatus::kUnexpectedUrl)))
      .Times(1)
      .WillOnce(Quit(loop.QuitClosure()));

  SearchResultExtractorProducer producer(env, nullptr,
                                         std::move(mock_interface));
  producer.FetchResults(
      env,
      base::android::JavaParamRef<jobject>(
          env, web_contents()->GetJavaWebContents().Release()),
      base::android::JavaParamRef<jstring>(
          env, base::android::ConvertUTF8ToJavaString(env, kQuery).Release()));
  loop.Run();
}

}  // namespace continuous_search
