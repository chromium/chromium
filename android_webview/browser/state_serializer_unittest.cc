// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/state_serializer.h"

#include <memory>
#include <string>

#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_entry_restore_context.h"
#include "content/public/common/content_client.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "url/gurl.h"

using std::string;

namespace android_webview {

namespace {

std::unique_ptr<content::NavigationEntry> CreateNavigationEntry() {
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());

  const GURL url("http://url");
  const GURL virtual_url("http://virtual_url");
  content::Referrer referrer;
  referrer.url = GURL("http://referrer_url");
  referrer.policy = network::mojom::ReferrerPolicy::kOrigin;
  const std::u16string title(u"title");
  const bool has_post_data = true;
  const GURL original_request_url("http://original_request_url");
  const GURL base_url_for_data_url("http://base_url");
  const string data_url_as_string("data:text/html;charset=utf-8;base64,");
  const bool is_overriding_user_agent = true;
  const base::Time timestamp = base::Time::FromInternalValue(12345);
  const int http_status_code = 404;

  entry->SetURL(url);
  entry->SetVirtualURL(virtual_url);
  entry->SetReferrer(referrer);
  entry->SetTitle(title);
  entry->SetHasPostData(has_post_data);
  entry->SetOriginalRequestURL(original_request_url);
  entry->SetBaseURLForDataURL(base_url_for_data_url);
  {
    scoped_refptr<base::RefCountedString> s = new base::RefCountedString();
    s->as_string() = data_url_as_string;
    entry->SetDataURLAsString(s);
  }
  entry->SetIsOverridingUserAgent(is_overriding_user_agent);
  entry->SetTimestamp(timestamp);
  entry->SetHttpStatusCode(http_status_code);
  return entry;
}

class AndroidWebViewStateSerializerTest : public testing::Test {
 public:
  AndroidWebViewStateSerializerTest() {
    content::SetContentClient(&content_client_);
    content::SetBrowserClientForTesting(&browser_client_);
  }

  AndroidWebViewStateSerializerTest(const AndroidWebViewStateSerializerTest&) =
      delete;
  AndroidWebViewStateSerializerTest& operator=(
      const AndroidWebViewStateSerializerTest&) = delete;

  ~AndroidWebViewStateSerializerTest() override {
    content::SetBrowserClientForTesting(nullptr);
    content::SetContentClient(nullptr);
  }

 private:
  content::ContentClient content_client_;
  content::ContentBrowserClient browser_client_;
};

}  // namespace

TEST_F(AndroidWebViewStateSerializerTest, TestHeaderSerialization) {
  base::Pickle pickle;
  internal::WriteHeaderToPickle(&pickle);

  base::PickleIterator iterator(pickle);
  uint32_t version = internal::RestoreHeaderFromPickle(&iterator);
  EXPECT_GT(version, 0U);
}

TEST_F(AndroidWebViewStateSerializerTest,
       TestLegacyVersionHeaderSerialization) {
  base::Pickle pickle;
  internal::WriteHeaderToPickle(internal::AW_STATE_VERSION_INITIAL, &pickle);

  base::PickleIterator iterator(pickle);
  uint32_t version = internal::RestoreHeaderFromPickle(&iterator);
  EXPECT_EQ(version, internal::AW_STATE_VERSION_INITIAL);
}

TEST_F(AndroidWebViewStateSerializerTest,
       TestUnsupportedVersionHeaderSerialization) {
  base::Pickle pickle;
  internal::WriteHeaderToPickle(20000101, &pickle);

  base::PickleIterator iterator(pickle);
  uint32_t version = internal::RestoreHeaderFromPickle(&iterator);
  EXPECT_EQ(version, 0U);
}

TEST_F(AndroidWebViewStateSerializerTest, TestNavigationEntrySerialization) {
  std::unique_ptr<content::NavigationEntry> entry(CreateNavigationEntry());

  base::Pickle pickle;
  internal::WriteNavigationEntryToPickle(*entry, &pickle);

  std::unique_ptr<content::NavigationEntry> copy(
      content::NavigationEntry::Create());
  base::PickleIterator iterator(pickle);
  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  bool result = internal::RestoreNavigationEntryFromPickle(
      &iterator, copy.get(), context.get());
  EXPECT_TRUE(result);

  EXPECT_EQ(entry->GetURL(), copy->GetURL());
  EXPECT_EQ(entry->GetVirtualURL(), copy->GetVirtualURL());
  EXPECT_EQ(entry->GetReferrer().url, copy->GetReferrer().url);
  EXPECT_EQ(entry->GetReferrer().policy, copy->GetReferrer().policy);
  EXPECT_EQ(entry->GetTitle(), copy->GetTitle());
  EXPECT_EQ(entry->GetPageState(), copy->GetPageState());
  EXPECT_EQ(entry->GetHasPostData(), copy->GetHasPostData());
  EXPECT_EQ(entry->GetOriginalRequestURL(), copy->GetOriginalRequestURL());
  EXPECT_EQ(entry->GetBaseURLForDataURL(), copy->GetBaseURLForDataURL());
  EXPECT_EQ(entry->GetDataURLAsString()->as_string(),
            copy->GetDataURLAsString()->as_string());
  EXPECT_EQ(entry->GetIsOverridingUserAgent(),
            copy->GetIsOverridingUserAgent());
  EXPECT_EQ(entry->GetTimestamp(), copy->GetTimestamp());
  EXPECT_EQ(entry->GetHttpStatusCode(), copy->GetHttpStatusCode());
}

TEST_F(AndroidWebViewStateSerializerTest,
       TestLegacyNavigationEntrySerialization) {
  std::unique_ptr<content::NavigationEntry> entry(CreateNavigationEntry());

  base::Pickle pickle;
  internal::WriteNavigationEntryToPickle(internal::AW_STATE_VERSION_INITIAL,
                                         *entry, &pickle);

  std::unique_ptr<content::NavigationEntry> copy(
      content::NavigationEntry::Create());
  base::PickleIterator iterator(pickle);
  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  bool result = internal::RestoreNavigationEntryFromPickle(
      internal::AW_STATE_VERSION_INITIAL, &iterator, copy.get(), context.get());
  EXPECT_TRUE(result);

  EXPECT_EQ(entry->GetURL(), copy->GetURL());
  EXPECT_EQ(entry->GetVirtualURL(), copy->GetVirtualURL());
  EXPECT_EQ(entry->GetReferrer().url, copy->GetReferrer().url);
  EXPECT_EQ(entry->GetReferrer().policy, copy->GetReferrer().policy);
  EXPECT_EQ(entry->GetTitle(), copy->GetTitle());
  EXPECT_EQ(entry->GetPageState(), copy->GetPageState());
  EXPECT_EQ(entry->GetHasPostData(), copy->GetHasPostData());
  EXPECT_EQ(entry->GetOriginalRequestURL(), copy->GetOriginalRequestURL());
  EXPECT_EQ(entry->GetBaseURLForDataURL(), copy->GetBaseURLForDataURL());
  // DataURL not supported by 20130814 format
  EXPECT_FALSE(copy->GetDataURLAsString());
  EXPECT_EQ(entry->GetIsOverridingUserAgent(),
            copy->GetIsOverridingUserAgent());
  EXPECT_EQ(entry->GetTimestamp(), copy->GetTimestamp());
  EXPECT_EQ(entry->GetHttpStatusCode(), copy->GetHttpStatusCode());
}

// This is a regression test for https://crbug.com/999078 - it checks that code
// is able to safely restore entries that were serialized with an empty
// PageState.
TEST_F(AndroidWebViewStateSerializerTest,
       TestDeserialization_20151204_EmptyPageState) {
  // Test data.
  GURL url("data:text/html,main_url");
  GURL virtual_url("https://example.com/virtual_url");
  content::Referrer referrer(GURL("https://example.com/referrer"),
                             network::mojom::ReferrerPolicy::kDefault);
  std::u16string title = u"title";
  std::string empty_encoded_page_state = "";
  bool has_post_data = false;
  GURL original_request_url("https://example.com/original");
  GURL base_url_for_data_url("https://example.com/base_url_for_data_url");
  bool is_overriding_user_agent = false;
  int64_t timestamp = 123456;
  int http_status_code = 404;

  // Write data to |pickle| in a way that would trigger https://crbug.com/999078
  // in the past.  The serialization format used below is based on version
  // 20151204 (aka AW_STATE_VERSION_DATA_URL).
  base::Pickle pickle;
  pickle.WriteString(url.spec());
  pickle.WriteString(virtual_url.spec());
  pickle.WriteString(referrer.url.spec());
  pickle.WriteInt(static_cast<int>(referrer.policy));
  pickle.WriteString16(title);
  pickle.WriteString(empty_encoded_page_state);
  pickle.WriteBool(has_post_data);
  pickle.WriteString(original_request_url.spec());
  pickle.WriteString(base_url_for_data_url.spec());
  pickle.WriteData(nullptr, 0);  // data_url_as_string
  pickle.WriteBool(is_overriding_user_agent);
  pickle.WriteInt64(timestamp);
  pickle.WriteInt(http_status_code);

  // Deserialize the |pickle|.
  base::PickleIterator iterator(pickle);
  std::unique_ptr<content::NavigationEntry> copy =
      content::NavigationEntry::Create();
  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  bool result = internal::RestoreNavigationEntryFromPickle(
      &iterator, copy.get(), context.get());
  EXPECT_TRUE(result);

  // In https://crbug.com/999078, the empty PageState would clobber the URL
  // leading to renderer-side CHECKs later on.  Code should replace the empty
  // PageState from the |pickle| with a real PageState that preserves the URL.
  // Additionally, the referrer needs to be restored in the NavigationEntry
  // (but not necessarily in the PageState - this preserves old behavior).
  EXPECT_EQ(url, copy->GetURL());
  EXPECT_FALSE(copy->GetPageState().ToEncodedData().empty());
  EXPECT_EQ(referrer.url, copy->GetReferrer().url);
  EXPECT_EQ(referrer.policy, copy->GetReferrer().policy);

  // Verify that other properties have deserialized as expected.
  EXPECT_EQ(virtual_url, copy->GetVirtualURL());
  EXPECT_EQ(referrer.policy, copy->GetReferrer().policy);
  EXPECT_EQ(title, copy->GetTitle());
  EXPECT_EQ(has_post_data, copy->GetHasPostData());
  EXPECT_EQ(original_request_url, copy->GetOriginalRequestURL());
  EXPECT_EQ(base_url_for_data_url, copy->GetBaseURLForDataURL());
  EXPECT_FALSE(copy->GetDataURLAsString());
  EXPECT_EQ(is_overriding_user_agent, copy->GetIsOverridingUserAgent());
  EXPECT_EQ(
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(timestamp)),
      copy->GetTimestamp());
  EXPECT_EQ(http_status_code, copy->GetHttpStatusCode());
}

TEST_F(AndroidWebViewStateSerializerTest, TestEmptyDataURLSerialization) {
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  EXPECT_FALSE(entry->GetDataURLAsString());

  base::Pickle pickle;
  internal::WriteNavigationEntryToPickle(*entry, &pickle);

  std::unique_ptr<content::NavigationEntry> copy(
      content::NavigationEntry::Create());
  base::PickleIterator iterator(pickle);
  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  bool result = internal::RestoreNavigationEntryFromPickle(
      &iterator, copy.get(), context.get());
  EXPECT_TRUE(result);
  EXPECT_FALSE(entry->GetDataURLAsString());
}

TEST_F(AndroidWebViewStateSerializerTest, TestHugeDataURLSerialization) {
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  string huge_data_url(1024 * 1024 * 20 - 1, 'd');
  huge_data_url.replace(0, strlen(url::kDataScheme), url::kDataScheme);
  {
    scoped_refptr<base::RefCountedString> s = new base::RefCountedString();
    s->as_string().assign(huge_data_url);
    entry->SetDataURLAsString(s);
  }

  base::Pickle pickle;
  internal::WriteNavigationEntryToPickle(*entry, &pickle);

  std::unique_ptr<content::NavigationEntry> copy(
      content::NavigationEntry::Create());
  base::PickleIterator iterator(pickle);
  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  bool result = internal::RestoreNavigationEntryFromPickle(
      &iterator, copy.get(), context.get());
  EXPECT_TRUE(result);
  EXPECT_EQ(huge_data_url, copy->GetDataURLAsString()->as_string());
}

}  // namespace android_webview
