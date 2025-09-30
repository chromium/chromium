// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/web_contents_state.h"

#include "base/android/jni_android.h"
#include "base/android/jni_bytebuffer.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

sessions::SerializedNavigationEntry CreateNavigation(
    int index,
    const std::string& url,
    const std::u16string& title) {
  sessions::SerializedNavigationEntry entry;
  entry.set_index(index);
  entry.set_virtual_url(GURL(url));
  entry.set_title(title);
  entry.set_transition_type(ui::PAGE_TRANSITION_LINK);
  return entry;
}

void ExpectNavigationsEqual(const sessions::SerializedNavigationEntry& expected,
                            const sessions::SerializedNavigationEntry& actual) {
  EXPECT_EQ(expected.index(), actual.index());
  EXPECT_EQ(expected.virtual_url(), actual.virtual_url());
  EXPECT_EQ(expected.title(), actual.title());
}

size_t CalculateUnpackedSize(const WebContentsStateUnpacked& unpacked) {
  size_t size = sizeof(WebContentsStateUnpacked);
  size += unpacked.navigations().capacity() *
          sizeof(sessions::SerializedNavigationEntry);
  for (const auto& nav : unpacked.navigations()) {
    size += nav.virtual_url().spec().size();
    size += nav.title().size() * sizeof(char16_t);
    size += nav.encoded_page_state().size();
  }
  return size;
}

}  // namespace

class WebContentsStateUnpackedTest : public ::testing::Test {
 public:
  WebContentsStateUnpackedTest() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(WebContentsStateUnpackedTest, PackAndUnpack) {
  const bool kIsOffTheRecord = false;
  const int kCurrentEntryIndex = 1;
  const int kSavedStateVersion = 2;

  std::vector<sessions::SerializedNavigationEntry> navigations;
  navigations.push_back(
      CreateNavigation(0, "https://example.com/0", u"Title 0"));
  navigations.push_back(
      CreateNavigation(1, "https://example.com/1", u"Title 1"));
  navigations.push_back(
      CreateNavigation(2, "https://example.com/2", u"Title 2"));
  const auto navigations_for_comparison = navigations;

  WebContentsStateUnpacked original_unpacked(
      kIsOffTheRecord, kCurrentEntryIndex, std::move(navigations));

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> buffer_obj =
      original_unpacked.Pack(env);
  ASSERT_TRUE(buffer_obj);

  base::span<const uint8_t> buffer_span =
      base::android::JavaByteBufferToSpan(env, buffer_obj);
  ASSERT_FALSE(buffer_span.empty());

  base::TimeTicks start_time = base::TimeTicks::Now();
  std::unique_ptr<WebContentsStateUnpacked> unpacked =
      WebContentsState::Unpack(buffer_span, kSavedStateVersion);
  base::TimeDelta unpack_duration = base::TimeTicks::Now() - start_time;
  LOG(INFO) << "Unpack duration: " << unpack_duration.InMicroseconds() << "us";
  ASSERT_TRUE(unpacked);

  EXPECT_EQ(unpacked->is_off_the_record(), kIsOffTheRecord);
  EXPECT_EQ(unpacked->current_entry_index(), kCurrentEntryIndex);
  ASSERT_EQ(unpacked->navigations().size(), navigations_for_comparison.size());
  for (size_t i = 0; i < unpacked->navigations().size(); ++i) {
    ExpectNavigationsEqual(navigations_for_comparison[i],
                           unpacked->navigations()[i]);
  }

  size_t unpacked_size = CalculateUnpackedSize(*unpacked);
  LOG(INFO) << "Buffer size (bytes): " << buffer_span.size()
            << ", Unpacked size (bytes): " << unpacked_size;
}
