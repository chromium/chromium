// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_preview_metadata.h"

#include <memory>

#include "base/files/file.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

TEST(PickerPreviewMetadataTest, ReturnsLastModified) {
  base::File::Info only_modified;
  ASSERT_TRUE(base::Time::FromString("23 Dec 2021 09:01:00",
                                     &only_modified.last_modified));

  EXPECT_EQ(PickerGetFilePreviewDescription(only_modified), u"Edited · Dec 23");
}

TEST(PickerPreviewMetadataTest, ReturnsLastAccessed) {
  base::File::Info only_accessed;
  ASSERT_TRUE(base::Time::FromString("23 Dec 2021 09:01:00",
                                     &only_accessed.last_accessed));

  EXPECT_EQ(PickerGetFilePreviewDescription(only_accessed),
            u"You opened · Dec 23");
}

TEST(PickerPreviewMetadataTest, ReturnsModifiedIfNewer) {
  base::File::Info modified_newer;
  ASSERT_TRUE(base::Time::FromString("23 Dec 2021 09:01:00",
                                     &modified_newer.last_modified));
  ASSERT_TRUE(base::Time::FromString("23 Dec 2021 09:00:00",
                                     &modified_newer.last_accessed));

  EXPECT_EQ(PickerGetFilePreviewDescription(modified_newer),
            u"Edited · Dec 23");
}

TEST(PickerPreviewMetadataTest, ReturnsAccessedIfNewer) {
  base::File::Info accessed_newer;
  ASSERT_TRUE(base::Time::FromString("23 Dec 2021 09:00:00",
                                     &accessed_newer.last_modified));
  ASSERT_TRUE(base::Time::FromString("23 Dec 2021 09:01:00",
                                     &accessed_newer.last_accessed));

  EXPECT_EQ(PickerGetFilePreviewDescription(accessed_newer),
            u"You opened · Dec 23");
}

TEST(PickerPreviewMetadataTest, ReturnsModifiedIfSameAsAccessed) {
  base::File::Info modified_newer;
  ASSERT_TRUE(base::Time::FromString("23 Dec 2021 09:01:00",
                                     &modified_newer.last_modified));
  ASSERT_TRUE(base::Time::FromString("23 Dec 2021 09:01:00",
                                     &modified_newer.last_accessed));

  EXPECT_EQ(PickerGetFilePreviewDescription(modified_newer),
            u"Edited · Dec 23");
}

}  // namespace
}  // namespace ash
