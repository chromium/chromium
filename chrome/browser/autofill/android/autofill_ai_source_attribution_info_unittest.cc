// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/autofill_ai_source_attribution_info.h"

#include <string>

#include "base/i18n/time_formatting.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using Source = EntityInstance::PersonalContextRecordTypePayload::Source;
using GmailSourceMetadata =
    EntityInstance::PersonalContextRecordTypePayload::GmailSourceMetadata;
using PhotosSourceMetadata =
    EntityInstance::PersonalContextRecordTypePayload::PhotosSourceMetadata;

class AutofillAiSourceAttributionInfoTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AutofillAiSourceAttributionInfoTest, ConstructFromGmailSource) {
  Source gmail_source{
      .url = GURL("https://mail.google.com/mail/u/0/#inbox/123"),
      .metadata = GmailSourceMetadata{.title = "Flight Confirmation"},
  };

  AutofillAiSourceAttributionInfo info(gmail_source);

  EXPECT_EQ(info.source_type, Source::Type::kGmail);
  EXPECT_EQ(info.url, GURL("https://mail.google.com/mail/u/0/#inbox/123"));
  EXPECT_EQ(info.title, u"Flight Confirmation");
}

TEST_F(AutofillAiSourceAttributionInfoTest, ConstructFromPhotosSource) {
  base::Time timestamp = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  Source photos_source{
      .url = GURL("https://photos.google.com/photo/456"),
      .metadata = PhotosSourceMetadata{.timestamp = timestamp},
  };

  AutofillAiSourceAttributionInfo info(photos_source);

  EXPECT_EQ(info.source_type, Source::Type::kPhotos);
  EXPECT_EQ(info.url, GURL("https://photos.google.com/photo/456"));
  EXPECT_EQ(info.title, base::TimeFormatShortDate(timestamp));
}

TEST_F(AutofillAiSourceAttributionInfoTest, ConstructFromPhotosSourceNullTime) {
  Source photos_source{
      .url = GURL("https://photos.google.com/photo/456"),
      .metadata = PhotosSourceMetadata{.timestamp = base::Time()},
  };

  AutofillAiSourceAttributionInfo info(photos_source);

  EXPECT_EQ(info.source_type, Source::Type::kPhotos);
  EXPECT_EQ(info.url, GURL("https://photos.google.com/photo/456"));
  EXPECT_EQ(info.title, u"");
}

TEST_F(AutofillAiSourceAttributionInfoTest, DoubleConversion) {
  AutofillAiSourceAttributionInfo original(
      Source::Type::kGmail, GURL("https://mail.google.com/mail/u/0/#inbox/123"),
      u"Flight Confirmation");

  JNIEnv* env = jni_zero::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> java_instance =
      jni_zero::ToJniType<AutofillAiSourceAttributionInfo>(env, original);

  AutofillAiSourceAttributionInfo converted =
      jni_zero::FromJniType<AutofillAiSourceAttributionInfo>(env,
                                                             java_instance);

  EXPECT_EQ(original, converted);
}

}  // namespace

}  // namespace autofill
