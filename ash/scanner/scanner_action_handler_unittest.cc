// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_handler.h"

#include <string>
#include <string_view>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/scanner/scanner_command_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::AllOf;
using ::testing::Property;

class TestScannerCommandDelegate : public ScannerCommandDelegate {
 public:
  MOCK_METHOD(void, OpenUrl, (const GURL& url), (override));

  base::WeakPtr<TestScannerCommandDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestScannerCommandDelegate> weak_factory_{this};
};

constexpr std::string_view kGoogleCalendarHost = "calendar.google.com";
constexpr std::string_view kGoogleCalendarRenderPath = "/calendar/render";

constexpr std::string_view kGoogleContactsHost = "contacts.google.com";
constexpr std::string_view kGoogleContactsNewPath = "/new";

TEST(ScannerActionHandlerTest, NewCalendarEventWithoutDelegateReturnsFalse) {
  base::test::SingleThreadTaskEnvironment task_environment;

  base::test::TestFuture<bool> done_future;
  HandleScannerAction(nullptr, NewCalendarEventAction(/*title=*/""),
                      done_future.GetCallback());

  EXPECT_FALSE(done_future.Get());
}

TEST(ScannerActionHandlerTest, NewCalendarEventWithNoFieldsOpensUrl) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestScannerCommandDelegate delegate;
  EXPECT_CALL(
      delegate,
      OpenUrl(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property("query_piece", &GURL::query_piece, "action=TEMPLATE"))))
      .Times(1);

  base::test::TestFuture<bool> done_future;
  HandleScannerAction(delegate.GetWeakPtr(),
                      NewCalendarEventAction(/*title=*/""),
                      done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
}

TEST(ScannerActionHandlerTest, NewCalendarEventWithTitleOpensUrl) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestScannerCommandDelegate delegate;
  EXPECT_CALL(
      delegate,
      OpenUrl(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property("query_piece", &GURL::query_piece,
                   "action=TEMPLATE&text=Test+title%3F"))))
      .Times(1);

  base::test::TestFuture<bool> done_future;
  HandleScannerAction(delegate.GetWeakPtr(),
                      NewCalendarEventAction("Test title?"),
                      done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
}

TEST(ScannerActionHandlerTest, NewContactWithoutDelegateReturnsFalse) {
  base::test::SingleThreadTaskEnvironment task_environment;

  base::test::TestFuture<bool> done_future;
  HandleScannerAction(nullptr, NewContactAction(/*given_name=*/""),
                      done_future.GetCallback());

  EXPECT_FALSE(done_future.Get());
}

TEST(ScannerActionHandlerTest, NewContactWithNoFieldsOpensUrl) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestScannerCommandDelegate delegate;
  EXPECT_CALL(
      delegate,
      OpenUrl(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleContactsHost),
          Property("path_piece", &GURL::path_piece, kGoogleContactsNewPath),
          Property("query_piece", &GURL::query_piece, ""))))
      .Times(1);

  base::test::TestFuture<bool> done_future;
  HandleScannerAction(delegate.GetWeakPtr(),
                      NewContactAction(/*given_name=*/""),
                      done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
}

TEST(ScannerActionHandlerTest, NewContactWithGivenNameOpensUrl) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestScannerCommandDelegate delegate;
  EXPECT_CALL(
      delegate,
      OpenUrl(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleContactsHost),
          Property("path_piece", &GURL::path_piece, kGoogleContactsNewPath),
          Property("query_piece", &GURL::query_piece, "given_name=L%C3%A9a"))))
      .Times(1);

  base::test::TestFuture<bool> done_future;
  HandleScannerAction(delegate.GetWeakPtr(), NewContactAction("Léa"),
                      done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
}

}  // namespace
}  // namespace ash
