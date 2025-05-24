// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace privacy_sandbox {

using notice::mojom::PrivacySandboxNoticeEvent;
using ::testing::Eq;
using ::testing::IsSupersetOf;
using ::testing::Pointee;

using enum NoticeActionTaken;

void CheckConvertsV1SchemaSuccessfully(NoticeActionTaken notice_action_taken,
                                       base::Time notice_taken_time,
                                       base::Time notice_last_shown) {
  V1MigrationData data_v1;
  data_v1.notice_action_taken = notice_action_taken;
  data_v1.notice_action_taken_time = notice_taken_time;
  data_v1.notice_last_shown = notice_last_shown;
  NoticeStorageData data_v2 = PrivacySandboxNoticeStorage::ToV2Schema(data_v1);
  EXPECT_EQ(data_v2.schema_version, 2);
  if (auto event = NoticeActionToEvent(notice_action_taken)) {
    EXPECT_THAT(data_v2.notice_events,
                IsSupersetOf({Pointee(
                    Eq(NoticeEventTimestampPair(*event, notice_taken_time)))}));
  }
  if (notice_last_shown != base::Time()) {
    EXPECT_THAT(data_v2.notice_events,
                IsSupersetOf({Pointee(Eq(NoticeEventTimestampPair(
                    PrivacySandboxNoticeEvent::kShown, notice_last_shown)))}));
  }
}

fuzztest::Domain<base::Time> AnyTime() {
  return fuzztest::Map(
      [](int64_t micros) {
        return base::Time::FromDeltaSinceWindowsEpoch(
            base::Microseconds(micros));
      },
      fuzztest::Arbitrary<int64_t>());
}

FUZZ_TEST(PrivacySandboxNoticeStorageFuzzTest,
          CheckConvertsV1SchemaSuccessfully)
    .WithDomains(
        /*notice_action_taken:*/ fuzztest::ElementOf<NoticeActionTaken>({
            kNotSet,
            kAck,
            kClosed,
            kOptIn,
            kOptOut,
            kSettings,
            kLearnMore_Deprecated,
            kOther,
            kUnknownActionPreMigration,
            kTimedOut,
        }),
        /*notice_taken_time:*/ AnyTime(),
        /*notice_last_shown:*/ AnyTime());

}  // namespace privacy_sandbox
