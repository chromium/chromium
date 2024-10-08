// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_handler.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::AllOf;
using ::testing::Property;

class TestUrlNewWindowDelegate : public TestNewWindowDelegate {
 public:
  GURL TakeLastOpenedUrl() { return last_opened_url_future_.Take(); }

 private:
  // TestNewWindowDelegate:
  void OpenUrl(const GURL& url,
               OpenUrlFrom from,
               Disposition disposition) override {
    last_opened_url_future_.SetValue(url);
  }

  base::test::TestFuture<GURL> last_opened_url_future_;
};

constexpr std::string_view kGoogleCalendarHost = "calendar.google.com";
constexpr std::string_view kGoogleCalendarRenderPath = "/calendar/render";

constexpr std::string_view kGoogleContactsHost = "contacts.google.com";
constexpr std::string_view kGoogleContactsNewPath = "/new";

TEST(ScannerActionHandlerTest, NewCalendarEventWithNoFieldsOpensUrl) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestUrlNewWindowDelegate delegate;

  base::test::TestFuture<bool> done_future;
  HandleScannerAction(NewCalendarEventAction(/*title=*/""),
                      done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
  EXPECT_THAT(
      delegate.TakeLastOpenedUrl(),
      AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property("query_piece", &GURL::query_piece, "action=TEMPLATE")));
}

TEST(ScannerActionHandlerTest, NewCalendarEventWithTitleOpensUrl) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestUrlNewWindowDelegate delegate;

  base::test::TestFuture<bool> done_future;
  HandleScannerAction(NewCalendarEventAction("Test title?"),
                      done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
  EXPECT_THAT(
      delegate.TakeLastOpenedUrl(),
      AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property("query_piece", &GURL::query_piece,
                   "action=TEMPLATE&text=Test+title%3F")));
}

TEST(ScannerActionHandlerTest, NewContactWithNoFieldsOpensUrl) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestUrlNewWindowDelegate delegate;

  base::test::TestFuture<bool> done_future;
  HandleScannerAction(NewContactAction(/*given_name=*/""),
                      done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
  EXPECT_THAT(
      delegate.TakeLastOpenedUrl(),
      AllOf(Property("host_piece", &GURL::host_piece, kGoogleContactsHost),
            Property("path_piece", &GURL::path_piece, kGoogleContactsNewPath),
            Property("query_piece", &GURL::query_piece, "")));
}

TEST(ScannerActionHandlerTest, NewContactWithGivenNameOpensUrl) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestUrlNewWindowDelegate delegate;

  base::test::TestFuture<bool> done_future;
  HandleScannerAction(NewContactAction("Léa"), done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
  EXPECT_THAT(
      delegate.TakeLastOpenedUrl(),
      AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleContactsHost),
          Property("path_piece", &GURL::path_piece, kGoogleContactsNewPath),
          Property("query_piece", &GURL::query_piece, "given_name=L%C3%A9a")));
}

}  // namespace
}  // namespace ash
