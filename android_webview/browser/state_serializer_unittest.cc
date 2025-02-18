// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/state_serializer.h"

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_entry_restore_context.h"
#include "content/public/common/content_client.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "url/gurl.h"

using std::string;

namespace android_webview {

namespace {

std::unique_ptr<content::NavigationEntry> CreateNavigationEntry(
    string url = "http://url") {
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());

  const GURL gurl(url);
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

  entry->SetURL(gurl);
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

void AssertEntriesEqual(content::NavigationEntry* lhs,
                        content::NavigationEntry* rhs) {
  EXPECT_EQ(lhs->GetURL(), rhs->GetURL());
  EXPECT_EQ(lhs->GetVirtualURL(), rhs->GetVirtualURL());
  EXPECT_EQ(lhs->GetReferrer().url, rhs->GetReferrer().url);
  EXPECT_EQ(lhs->GetReferrer().policy, rhs->GetReferrer().policy);
  EXPECT_EQ(lhs->GetTitle(), rhs->GetTitle());
  EXPECT_EQ(lhs->GetPageState(), rhs->GetPageState());
  EXPECT_EQ(lhs->GetHasPostData(), rhs->GetHasPostData());
  EXPECT_EQ(lhs->GetOriginalRequestURL(), rhs->GetOriginalRequestURL());
  EXPECT_EQ(lhs->GetBaseURLForDataURL(), rhs->GetBaseURLForDataURL());
  EXPECT_EQ(lhs->GetDataURLAsString()->as_string(),
            rhs->GetDataURLAsString()->as_string());
  EXPECT_EQ(lhs->GetIsOverridingUserAgent(), rhs->GetIsOverridingUserAgent());
  EXPECT_EQ(lhs->GetTimestamp(), rhs->GetTimestamp());
  EXPECT_EQ(lhs->GetHttpStatusCode(), rhs->GetHttpStatusCode());
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

// A fake navigation controller that holes a simple list of navigation entries,
// and can restore to that list.
class TestNavigationController : public internal::NavigationHistory,
                                 public internal::NavigationHistorySink {
 public:
  int GetEntryCount() override { return entries_.size(); }

  int GetCurrentEntry() override { return current_entry_; }

  content::NavigationEntry* GetEntryAtIndex(int index) override {
    return entries_.at(index).get();
  }

  void Add(std::unique_ptr<content::NavigationEntry> entry) {
    entries_.push_back(std::move(entry));
    current_entry_ = entries_.size() - 1;
  }

  void SetCurrentEntry(unsigned int index) {
    DCHECK_LT(index, entries_.size());
    current_entry_ = index;
  }

  void Restore(int selected_entry,
               std::vector<std::unique_ptr<content::NavigationEntry>>* entries)
      override {
    current_entry_ = selected_entry;
    entries_.insert(entries_.end(), std::make_move_iterator(entries->begin()),
                    std::make_move_iterator(entries->end()));
    entries->clear();
  }

 private:
  int current_entry_ = 0;
  std::vector<std::unique_ptr<content::NavigationEntry>> entries_;
};

void AssertHistoriesEqual(TestNavigationController& lhs,
                          TestNavigationController& rhs) {
  EXPECT_EQ(lhs.GetEntryCount(), rhs.GetEntryCount());
  EXPECT_EQ(lhs.GetCurrentEntry(), rhs.GetCurrentEntry());
  for (int i = 0; i < lhs.GetEntryCount(); i++) {
    AssertEntriesEqual(lhs.GetEntryAtIndex(i), rhs.GetEntryAtIndex(i));
  }
}

void WriteToPickleLegacy_VersionDataUrl(internal::NavigationHistory& history,
                                        base::Pickle* pickle) {
  DCHECK(pickle);
  int state_version = internal::AW_STATE_VERSION_DATA_URL;

  internal::WriteHeaderToPickle(state_version, pickle);

  const int entry_count = history.GetEntryCount();
  const int selected_entry = history.GetCurrentEntry();
  // A NavigationEntry will always exist, so there will always be at least 1
  // entry.
  DCHECK_GE(entry_count, 1);
  DCHECK_GE(selected_entry, 0);
  DCHECK_LT(selected_entry, entry_count);

  pickle->WriteInt(entry_count);
  pickle->WriteInt(selected_entry);
  for (int i = 0; i < entry_count; ++i) {
    internal::WriteNavigationEntryToPickle(state_version,
                                           *history.GetEntryAtIndex(i), pickle);
  }

  // Please update AW_STATE_VERSION and IsSupportedVersion() if serialization
  // format is changed.
  // Make sure the serialization format is updated in a backwards compatible
  // way.
}

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

  AssertEntriesEqual(entry.get(), copy.get());
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

TEST_F(AndroidWebViewStateSerializerTest,
       TestDeserializeLegacy_VersionDataUrl) {
  TestNavigationController controller;

  controller.Add(CreateNavigationEntry("http://url1"));
  controller.Add(CreateNavigationEntry("http://url2"));
  controller.Add(CreateNavigationEntry("http://url3"));

  base::Pickle pickle;
  WriteToPickleLegacy_VersionDataUrl(controller, &pickle);

  TestNavigationController copy;
  base::PickleIterator iterator(pickle);
  internal::RestoreFromPickle(&iterator, copy);

  AssertHistoriesEqual(controller, copy);
}

TEST_F(AndroidWebViewStateSerializerTest, TestHistorySerialization) {
  TestNavigationController controller;

  controller.Add(CreateNavigationEntry("http://url1"));
  controller.Add(CreateNavigationEntry("http://url2"));
  controller.Add(CreateNavigationEntry("http://url3"));

  base::Pickle pickle = internal::WriteToPickle(controller).value();

  TestNavigationController copy;
  base::PickleIterator iterator(pickle);
  internal::RestoreFromPickle(&iterator, copy);

  AssertHistoriesEqual(controller, copy);
}

TEST_F(AndroidWebViewStateSerializerTest, TestHistoryTruncation) {
  // Create the expected result first, so we can measure it and determine what
  // to set the max size to.
  TestNavigationController expected;
  expected.Add(CreateNavigationEntry("http://url2"));
  expected.Add(CreateNavigationEntry("http://url3"));
  size_t max_size = internal::WriteToPickle(expected)->size();

  TestNavigationController controller;
  controller.Add(CreateNavigationEntry("http://url1"));
  controller.Add(CreateNavigationEntry("http://url2"));
  controller.Add(CreateNavigationEntry("http://url3"));

  base::Pickle pickle = internal::WriteToPickle(controller, max_size).value();

  TestNavigationController copy;
  base::PickleIterator iterator(pickle);
  EXPECT_TRUE(internal::RestoreFromPickle(&iterator, copy));

  AssertHistoriesEqual(expected, copy);
}

TEST_F(AndroidWebViewStateSerializerTest,
       TestHistoryTruncation_MaxSizeTooSmall) {
  size_t max_size = 0;

  TestNavigationController controller;
  controller.Add(CreateNavigationEntry("http://url1"));
  controller.Add(CreateNavigationEntry("http://url2"));
  controller.Add(CreateNavigationEntry("http://url3"));

  std::optional<base::Pickle> maybe_pickle =
      internal::WriteToPickle(controller, max_size);

  EXPECT_FALSE(maybe_pickle.has_value());
}

TEST_F(AndroidWebViewStateSerializerTest,
       TestHistoryTruncation_NoForwardHistory) {
  // In this test we expect url3 to be cut, because url2 is selected and we pass
  // save_forward_history as false.
  TestNavigationController expected;
  expected.Add(CreateNavigationEntry("http://url1"));
  expected.Add(CreateNavigationEntry("http://url2"));
  size_t max_size = internal::WriteToPickle(expected)->size();

  TestNavigationController controller;
  controller.Add(CreateNavigationEntry("http://url1"));
  controller.Add(CreateNavigationEntry("http://url2"));
  controller.Add(CreateNavigationEntry("http://url3"));
  controller.SetCurrentEntry(1);

  base::Pickle pickle =
      internal::WriteToPickle(controller, max_size,
                              /* save_forward_history= */ false)
          .value();

  TestNavigationController copy;
  base::PickleIterator iterator(pickle);
  EXPECT_TRUE(internal::RestoreFromPickle(&iterator, copy));

  AssertHistoriesEqual(expected, copy);
}

TEST_F(AndroidWebViewStateSerializerTest,
       TestHistoryTruncation_SelectedEntryFarBack) {
  // The selected entry is far back in history (more than max_size) back. Make
  // sure that we don't drop it.

  // The current implementation falls back to save_forward_history = false, but
  // the key requirement is that the selected entry is saved.

  size_t max_size;
  {
    TestNavigationController controller;
    controller.Add(CreateNavigationEntry("http://url2"));
    controller.Add(CreateNavigationEntry("http://url3"));
    max_size = internal::WriteToPickle(controller)->size();
  }

  TestNavigationController expected;
  expected.Add(CreateNavigationEntry("http://url1"));

  TestNavigationController controller;
  controller.Add(CreateNavigationEntry("http://url1"));
  controller.Add(CreateNavigationEntry("http://url2"));
  controller.Add(CreateNavigationEntry("http://url3"));
  controller.SetCurrentEntry(0);

  base::Pickle pickle = internal::WriteToPickle(controller, max_size).value();

  TestNavigationController copy;
  base::PickleIterator iterator(pickle);
  EXPECT_TRUE(internal::RestoreFromPickle(&iterator, copy));

  AssertHistoriesEqual(expected, copy);
}

}  // namespace android_webview
