// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using Event = notice::mojom::PrivacySandboxNoticeEvent;
using enum Event;

TEST(PrivacySandboxNoticeHistogramsTest, CheckPSNoticeHistograms) {
  std::optional<base::HistogramVariantsEntryMap> notices;
  std::vector<std::string> missing_notices;
  {
    notices = base::ReadVariantsFromHistogramsXml("PSNotice", "privacy");
    ASSERT_TRUE(notices.has_value());
  }
  NoticeCatalogImpl catalog;
  EXPECT_EQ(catalog.GetNotices().size(), notices->size());
  for (const Notice* notice : catalog.GetNotices()) {
    // TODO(crbug.com/333406690): Implement something to clean up notices that
    // don't exist.
    if (!base::Contains(*notices, notice->GetStorageName())) {
      missing_notices.emplace_back(notice->GetStorageName());
    }
  }
  ASSERT_TRUE(missing_notices.empty())
      << "Notices:\n"
      << base::JoinString(missing_notices, ", ")
      << "\nconfigured in notice_catalog but no "
         "corresponding variants were added to PSNotice variants in "
         "//tools/metrics/histograms/metadata/privacy/histograms.xml";
}

TEST(PrivacySandboxNoticeHistogramsTest, CheckPSNoticeActionHistograms) {
  std::optional<base::HistogramVariantsEntryMap> actions;
  std::vector<std::string> missing_actions;
  {
    actions = base::ReadVariantsFromHistogramsXml("PSNoticeAction", "privacy");
    ASSERT_TRUE(actions.has_value());
  }

  for (int i = static_cast<int>(kMinValue); i <= static_cast<int>(kMaxValue);
       ++i) {
    Event event = static_cast<Event>(i);
    if (event == kShown) {
      continue;
    }
    if (std::string notice_name = GetNoticeActionStringFromEvent(event);
        !base::Contains(*actions, notice_name)) {
      missing_actions.emplace_back(notice_name);
    }
  }
  ASSERT_TRUE(missing_actions.empty())
      << "Actions:\n"
      << base::JoinString(missing_actions, ", ")
      << "\nconfigured in notice.mojom but no corresponding variants were "
         "added to PSNoticeAction variants in "
         "//tools/metrics/histograms/metadata/privacy/histograms.xml";
}

}  // namespace
}  // namespace privacy_sandbox
