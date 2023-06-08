// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"

#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

using AccessCodeCastPrefUpdaterTest = testing::Test;

TEST_F(AccessCodeCastPrefUpdaterTest, TestGetMatchingIPEndPoints) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);
  base::Value::Dict devices_dict;
  devices_dict.Set(cast_sink.id(),
                   CreateValueDictFromMediaSinkInternal(cast_sink));

  EXPECT_TRUE(AccessCodeCastPrefUpdater::GetMatchingIPEndPoints(
                  devices_dict, cast_sink2.cast_data().ip_endpoint)
                  .empty());
  EXPECT_EQ(AccessCodeCastPrefUpdater::GetMatchingIPEndPoints(
                devices_dict, cast_sink.cast_data().ip_endpoint)
                .size(),
            1u);
  EXPECT_EQ(AccessCodeCastPrefUpdater::GetMatchingIPEndPoints(
                devices_dict, cast_sink.cast_data().ip_endpoint)
                .front(),
            cast_sink.sink().id());
}

TEST_F(AccessCodeCastPrefUpdaterTest, TestGetMatchingIPEndPointsIdenticalIPs) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);
  MediaSinkInternal cast_sink3 = CreateCastSink(3);

  // Set the ip_endpoint of cast_sink2 to the ip_endpoint of cast_sink.
  cast_sink2.set_cast_data(cast_sink.cast_data());

  base::Value::Dict devices_dict;
  devices_dict.Set(cast_sink.id(),
                   CreateValueDictFromMediaSinkInternal(cast_sink));
  devices_dict.Set(cast_sink2.id(),
                   CreateValueDictFromMediaSinkInternal(cast_sink2));
  devices_dict.Set(cast_sink3.id(),
                   CreateValueDictFromMediaSinkInternal(cast_sink3));

  std::vector<MediaSink::Id> expected_vector{cast_sink.sink().id(),
                                             cast_sink2.sink().id()};

  EXPECT_EQ(AccessCodeCastPrefUpdater::GetMatchingIPEndPoints(
                devices_dict, cast_sink.cast_data().ip_endpoint)
                .size(),
            2u);
  EXPECT_EQ(AccessCodeCastPrefUpdater::GetMatchingIPEndPoints(
                devices_dict, cast_sink.cast_data().ip_endpoint),
            expected_vector);
}

}  // namespace media_router
