// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "content/public/common/content_client.h"
#include "content/public/common/page_state.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  const base::string16 title(base::UTF8ToUTF16("title"));
  const content::PageState page_state =
      content::PageState::CreateFromEncodedData("completely bogus state");
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
  entry->SetPageState(page_state);
  entry->SetHasPostData(has_post_data);
  entry->SetOriginalRequestURL(original_request_url);
  entry->SetBaseURLForDataURL(base_url_for_data_url);
  {
    scoped_refptr<base::RefCountedString> s = new base::RefCountedString();
    s->data().assign(data_url_as_string);
    entry->SetDataURLAsString(s);
  }
  entry->SetIsOverridingUserAgent(is_overriding_user_agent);
  entry->SetTimestamp(timestamp);
  entry->SetHttpStatusCode(http_status_code);
  return entry;
}

}  // namespace

TEST(AndroidWebViewStateSerializerTest, TestHeaderSerialization) {
  base::Pickle pickle;
  internal::WriteHeaderToPickle(&pickle);

  base::PickleIterator iterator(pickle);
  uint32_t version = internal::RestoreHeaderFromPickle(&iterator);
  EXPECT_GT(version, 0U);
}

TEST(AndroidWebViewStateSerializerTest, TestLegacyVersionHeaderSerialization) {
  base::Pickle pickle;
  internal::WriteHeaderToPickle(internal::AW_STATE_VERSION_INITIAL, &pickle);

  base::PickleIterator iterator(pickle);
  uint32_t version = internal::RestoreHeaderFromPickle(&iterator);
  EXPECT_EQ(version, internal::AW_STATE_VERSION_INITIAL);
}

TEST(AndroidWebViewStateSerializerTest,
     TestUnsupportedVersionHeaderSerialization) {
  base::Pickle pickle;
  internal::WriteHeaderToPickle(20000101, &pickle);

  base::PickleIterator iterator(pickle);
  uint32_t version = internal::RestoreHeaderFromPickle(&iterator);
  EXPECT_EQ(version, 0U);
}

TEST(AndroidWebViewStateSerializerTest, TestNavigationEntrySerialization) {
  // This is required for NavigationEntry::Create.
  content::ContentClient content_client;
  content::SetContentClient(&content_client);
  content::ContentBrowserClient browser_client;
  content::SetBrowserClientForTesting(&browser_client);

  std::unique_ptr<content::NavigationEntry> entry(CreateNavigationEntry());

  base::Pickle pickle;
  internal::WriteNavigationEntryToPickle(*entry, &pickle);

  std::unique_ptr<content::NavigationEntry> copy(
      content::NavigationEntry::Create());
  base::PickleIterator iterator(pickle);
  bool result =
      internal::RestoreNavigationEntryFromPickle(&iterator, copy.get());
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
  EXPECT_EQ(entry->GetDataURLAsString()->data(),
            copy->GetDataURLAsString()->data());
  EXPECT_EQ(entry->GetIsOverridingUserAgent(),
            copy->GetIsOverridingUserAgent());
  EXPECT_EQ(entry->GetTimestamp(), copy->GetTimestamp());
  EXPECT_EQ(entry->GetHttpStatusCode(), copy->GetHttpStatusCode());
}

TEST(AndroidWebViewStateSerializerTest,
     TestLegacyNavigationEntrySerialization) {
  // This is required for NavigationEntry::Create.
  content::ContentClient content_client;
  content::SetContentClient(&content_client);
  content::ContentBrowserClient browser_client;
  content::SetBrowserClientForTesting(&browser_client);

  std::unique_ptr<content::NavigationEntry> entry(CreateNavigationEntry());

  base::Pickle pickle;
  internal::WriteNavigationEntryToPickle(internal::AW_STATE_VERSION_INITIAL,
                                         *entry, &pickle);

  std::unique_ptr<content::NavigationEntry> copy(
      content::NavigationEntry::Create());
  base::PickleIterator iterator(pickle);
  bool result = internal::RestoreNavigationEntryFromPickle(
      internal::AW_STATE_VERSION_INITIAL, &iterator, copy.get());
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

TEST(AndroidWebViewStateSerializerTest, TestEmptyDataURLSerialization) {
  // This is required for NavigationEntry::Create.
  content::ContentClient content_client;
  content::SetContentClient(&content_client);
  content::ContentBrowserClient browser_client;
  content::SetBrowserClientForTesting(&browser_client);

  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  EXPECT_FALSE(entry->GetDataURLAsString());

  base::Pickle pickle;
  internal::WriteNavigationEntryToPickle(*entry, &pickle);

  std::unique_ptr<content::NavigationEntry> copy(
      content::NavigationEntry::Create());
  base::PickleIterator iterator(pickle);
  bool result =
      internal::RestoreNavigationEntryFromPickle(&iterator, copy.get());
  EXPECT_TRUE(result);
  EXPECT_FALSE(entry->GetDataURLAsString());
}

TEST(AndroidWebViewStateSerializerTest, TestHugeDataURLSerialization) {
  // This is required for NavigationEntry::Create.
  content::ContentClient content_client;
  content::SetContentClient(&content_client);
  content::ContentBrowserClient browser_client;
  content::SetBrowserClientForTesting(&browser_client);

  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  string huge_data_url(1024 * 1024 * 20 - 1, 'd');
  huge_data_url.replace(0, strlen(url::kDataScheme), url::kDataScheme);
  {
    scoped_refptr<base::RefCountedString> s = new base::RefCountedString();
    s->data().assign(huge_data_url);
    entry->SetDataURLAsString(s);
  }

  base::Pickle pickle;
  internal::WriteNavigationEntryToPickle(*entry, &pickle);

  std::unique_ptr<content::NavigationEntry> copy(
      content::NavigationEntry::Create());
  base::PickleIterator iterator(pickle);
  bool result =
      internal::RestoreNavigationEntryFromPickle(&iterator, copy.get());
  EXPECT_TRUE(result);
  EXPECT_EQ(huge_data_url, copy->GetDataURLAsString()->data());
}

}  // namespace android_webview
