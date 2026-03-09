// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/state_serializer.h"

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "android_webview/common/aw_features.h"
#include "base/check_op.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
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

// Helper methods
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
  const base::Time timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(12345);
  const int http_status_code = 404;
  const string extra_header = "X-Navigation-Extra-Header: Testing";

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
  entry->AddExtraHeaders(extra_header);

  return entry;
}

void VerifyRestoredNavigation(uint32_t state_version,
                              content::NavigationEntry* original,
                              content::NavigationEntry* copy) {
  EXPECT_EQ(original->GetURL(), copy->GetURL());
  EXPECT_EQ(original->GetVirtualURL(), copy->GetVirtualURL());
  EXPECT_EQ(original->GetReferrer().url, copy->GetReferrer().url);
  EXPECT_EQ(original->GetReferrer().policy, copy->GetReferrer().policy);
  EXPECT_EQ(original->GetTitle(), copy->GetTitle());
  EXPECT_EQ(original->GetPageState(), copy->GetPageState());
  EXPECT_EQ(original->GetHasPostData(), copy->GetHasPostData());
  EXPECT_EQ(original->GetOriginalRequestURL(), copy->GetOriginalRequestURL());
  EXPECT_EQ(original->GetBaseURLForDataURL(), copy->GetBaseURLForDataURL());
  if (state_version >= internal::AW_STATE_VERSION_DATA_URL) {
    EXPECT_EQ(original->GetDataURLAsString()->as_string(),
              copy->GetDataURLAsString()->as_string());
  } else {
    EXPECT_FALSE(copy->GetDataURLAsString());
  }
  EXPECT_EQ(original->GetIsOverridingUserAgent(),
            copy->GetIsOverridingUserAgent());
  EXPECT_EQ(original->GetTimestamp(), copy->GetTimestamp());
  EXPECT_EQ(original->GetHttpStatusCode(), copy->GetHttpStatusCode());
  if (state_version >= internal::AW_STATE_VERSION_INCLUDE_HEADERS &&
      base::FeatureList::IsEnabled(features::kWebViewSaveStateIncludeHeaders)) {
    EXPECT_EQ(original->GetExtraHeaders(), copy->GetExtraHeaders());
  } else {
    EXPECT_TRUE(copy->GetExtraHeaders().empty());
  }
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

// A fake navigation controller that holds a simple list of navigation entries,
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

void VerifyRestoredNavigationHistory(uint32_t state_version,
                                     TestNavigationController& original,
                                     TestNavigationController& copy) {
  EXPECT_EQ(original.GetEntryCount(), copy.GetEntryCount());
  EXPECT_EQ(original.GetCurrentEntry(), copy.GetCurrentEntry());
  for (int i = 0; i < original.GetEntryCount(); i++) {
    VerifyRestoredNavigation(state_version, original.GetEntryAtIndex(i),
                             copy.GetEntryAtIndex(i));
  }
}

void WriteToPickle_LegacyVersion(internal::NavigationHistory& history,
                                 base::Pickle* pickle,
                                 uint32_t state_version) {
  DCHECK(pickle);

  internal::WriteHeaderToPickle(pickle, state_version);

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
    internal::WriteNavigationEntryToPickle(*history.GetEntryAtIndex(i), pickle,
                                           state_version);
  }
}

}  // namespace

TEST_F(AndroidWebViewStateSerializerTest, TestHeaderSerialization) {
  for (uint32_t version : internal::AW_STATE_SUPPORTED_VERSIONS) {
    base::Pickle pickle;
    internal::WriteHeaderToPickle(&pickle, version);

    base::PickleIterator iterator(pickle);
    uint32_t restored_version = internal::RestoreHeaderFromPickle(&iterator);
    EXPECT_EQ(restored_version, version);
  }
}

TEST_F(AndroidWebViewStateSerializerTest,
       TestUnsupportedVersionHeaderSerialization) {
  base::Pickle pickle;
  internal::WriteHeaderToPickle(&pickle, 20000101);

  base::PickleIterator iterator(pickle);
  uint32_t version = internal::RestoreHeaderFromPickle(&iterator);
  EXPECT_EQ(version, 0U);
}

TEST_F(AndroidWebViewStateSerializerTest, TestNavigationEntrySerialization) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebViewSaveStateIncludeHeaders);
  std::unique_ptr<content::NavigationEntry> entry(CreateNavigationEntry());

  for (uint32_t version : internal::AW_STATE_SUPPORTED_VERSIONS) {
    // Create pickle and save navigation entry to it
    base::Pickle pickle;
    internal::WriteNavigationEntryToPickle(*entry, &pickle, version);

    // Restore navigation entry from pickle
    std::unique_ptr<content::NavigationEntry> copy(
        content::NavigationEntry::Create());
    base::PickleIterator iterator(pickle);
    std::unique_ptr<content::NavigationEntryRestoreContext> context =
        content::NavigationEntryRestoreContext::Create();
    bool result = internal::RestoreNavigationEntryFromPickle(
        &iterator, copy.get(), context.get(), version);
    EXPECT_TRUE(result);

    // Check that the navigation entry restored correctly based on the version
    // it was saved against
    VerifyRestoredNavigation(version, entry.get(), copy.get());
  }
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
  pickle.WriteData(std::string_view());  // data_url_as_string
  pickle.WriteBool(is_overriding_user_agent);
  pickle.WriteInt64(timestamp);
  pickle.WriteInt(http_status_code);
  pickle.WriteString("");

  // Deserialize the |pickle|.
  base::PickleIterator iterator(pickle);
  std::unique_ptr<content::NavigationEntry> copy =
      content::NavigationEntry::Create();
  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  bool result = internal::RestoreNavigationEntryFromPickle(
      &iterator, copy.get(), context.get(),
      internal::AW_STATE_VERSION_DATA_URL);
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

// Note the below tests only test against the current version used for saving
// state

TEST_F(AndroidWebViewStateSerializerTest, TestEmptyDataURLSerialization) {
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  EXPECT_FALSE(entry->GetDataURLAsString());

  base::Pickle pickle;
  internal::WriteNavigationEntryToPickle(*entry, &pickle,
                                         internal::AW_STATE_VERSION);

  std::unique_ptr<content::NavigationEntry> copy(
      content::NavigationEntry::Create());
  base::PickleIterator iterator(pickle);
  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  bool result = internal::RestoreNavigationEntryFromPickle(
      &iterator, copy.get(), context.get(), internal::AW_STATE_VERSION);
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
  internal::WriteNavigationEntryToPickle(*entry, &pickle,
                                         internal::AW_STATE_VERSION);

  std::unique_ptr<content::NavigationEntry> copy(
      content::NavigationEntry::Create());
  base::PickleIterator iterator(pickle);
  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  bool result = internal::RestoreNavigationEntryFromPickle(
      &iterator, copy.get(), context.get(), internal::AW_STATE_VERSION);
  EXPECT_TRUE(result);
  EXPECT_EQ(huge_data_url, copy->GetDataURLAsString()->as_string());
}

TEST_F(AndroidWebViewStateSerializerTest, TestHistorySerialization) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebViewSaveStateIncludeHeaders);

  // Create navigation history
  TestNavigationController controller;
  controller.Add(CreateNavigationEntry("http://url1"));
  controller.Add(CreateNavigationEntry("http://url2"));
  controller.Add(CreateNavigationEntry("http://url3"));

  // Check for each supported version that the state is saved and restored
  // correctly
  for (uint32_t version : internal::AW_STATE_SUPPORTED_VERSIONS) {
    // Create pickle and save history to it
    base::Pickle pickle;
    if (version < internal::AW_STATE_VERSION_MOST_RECENT_FIRST) {
      WriteToPickle_LegacyVersion(controller, &pickle, version);
    } else {
      pickle = internal::WriteToPickle(controller, version).value();
    }

    // Restore history from pickle
    TestNavigationController copy;
    base::PickleIterator iterator(pickle);
    bool result = internal::RestoreFromPickle(&iterator, copy);
    EXPECT_TRUE(result);

    // Check that the history restored correctly based on the version it was
    // saved against
    VerifyRestoredNavigationHistory(version, controller, copy);
  }
}

TEST_F(AndroidWebViewStateSerializerTest, TestHistoryTruncation) {
  // Create the expected result first, so we can measure it and determine what
  // to set the max size to.
  TestNavigationController expected;
  expected.Add(CreateNavigationEntry("http://url2"));
  expected.Add(CreateNavigationEntry("http://url3"));
  size_t max_size =
      internal::WriteToPickle(expected, internal::AW_STATE_VERSION)->size();

  TestNavigationController controller;
  controller.Add(CreateNavigationEntry("http://url1"));
  controller.Add(CreateNavigationEntry("http://url2"));
  controller.Add(CreateNavigationEntry("http://url3"));

  base::Pickle pickle =
      internal::WriteToPickle(controller, internal::AW_STATE_VERSION, max_size)
          .value();

  TestNavigationController copy;
  base::PickleIterator iterator(pickle);
  EXPECT_TRUE(internal::RestoreFromPickle(&iterator, copy));

  VerifyRestoredNavigationHistory(internal::AW_STATE_VERSION, expected, copy);
}

TEST_F(AndroidWebViewStateSerializerTest,
       TestHistoryTruncation_MaxSizeTooSmall) {
  size_t max_size = 0;

  TestNavigationController controller;
  controller.Add(CreateNavigationEntry("http://url1"));
  controller.Add(CreateNavigationEntry("http://url2"));
  controller.Add(CreateNavigationEntry("http://url3"));

  std::optional<base::Pickle> maybe_pickle =
      internal::WriteToPickle(controller, internal::AW_STATE_VERSION, max_size);

  EXPECT_FALSE(maybe_pickle.has_value());
}

TEST_F(AndroidWebViewStateSerializerTest,
       TestHistoryTruncation_NoForwardHistory) {
  // In this test we expect url3 to be cut, because url2 is selected and we pass
  // save_forward_history as false.
  TestNavigationController expected;
  expected.Add(CreateNavigationEntry("http://url1"));
  expected.Add(CreateNavigationEntry("http://url2"));
  size_t max_size =
      internal::WriteToPickle(expected, internal::AW_STATE_VERSION)->size();

  TestNavigationController controller;
  controller.Add(CreateNavigationEntry("http://url1"));
  controller.Add(CreateNavigationEntry("http://url2"));
  controller.Add(CreateNavigationEntry("http://url3"));
  controller.SetCurrentEntry(1);

  base::Pickle pickle =
      internal::WriteToPickle(controller, internal::AW_STATE_VERSION, max_size,
                              /* save_forward_history= */ false)
          .value();

  TestNavigationController copy;
  base::PickleIterator iterator(pickle);
  EXPECT_TRUE(internal::RestoreFromPickle(&iterator, copy));

  VerifyRestoredNavigationHistory(internal::AW_STATE_VERSION, expected, copy);
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
    max_size =
        internal::WriteToPickle(controller, internal::AW_STATE_VERSION)->size();
  }

  TestNavigationController expected;
  expected.Add(CreateNavigationEntry("http://url1"));

  TestNavigationController controller;
  controller.Add(CreateNavigationEntry("http://url1"));
  controller.Add(CreateNavigationEntry("http://url2"));
  controller.Add(CreateNavigationEntry("http://url3"));
  controller.SetCurrentEntry(0);

  base::Pickle pickle =
      internal::WriteToPickle(controller, internal::AW_STATE_VERSION, max_size)
          .value();

  TestNavigationController copy;
  base::PickleIterator iterator(pickle);
  EXPECT_TRUE(internal::RestoreFromPickle(&iterator, copy));

  VerifyRestoredNavigationHistory(internal::AW_STATE_VERSION, expected, copy);
}

}  // namespace android_webview
