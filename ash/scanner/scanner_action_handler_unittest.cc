// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_handler.h"

#include <memory>
#include <string>
#include <string_view>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/scanner/scanner_command.h"
#include "ash/scanner/scanner_command_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/service/drive_service_interface.h"
#include "components/drive/service/fake_drive_service.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/drive/drive_api_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::VariantWith;

class TestScannerCommandDelegate : public ScannerCommandDelegate {
 public:
  MOCK_METHOD(void, OpenUrl, (const GURL& url), (override));
  MOCK_METHOD(drive::DriveServiceInterface*, GetDriveService, (), (override));
  MOCK_METHOD(void,
              SetClipboard,
              (std::unique_ptr<ui::ClipboardData> data),
              (override));

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

TEST(ScannerActionToCommandTest, NewCalendarEventWithNoFields) {
  ScannerCommand command =
      ScannerActionToCommand(NewCalendarEventAction(/*title=*/""));

  EXPECT_THAT(
      command,
      VariantWith<OpenUrlCommand>(FieldsAre(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property("query_piece", &GURL::query_piece, "action=TEMPLATE")))));
}

TEST(ScannerActionToCommandTest, NewCalendarEventWithTitle) {
  ScannerCommand command =
      ScannerActionToCommand(NewCalendarEventAction("Test title?"));

  EXPECT_THAT(
      command,
      VariantWith<OpenUrlCommand>(FieldsAre(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property("query_piece", &GURL::query_piece,
                   "action=TEMPLATE&text=Test+title%3F")))));
}

TEST(ScannerActionToCommandTest, NewContactWithNoFields) {
  ScannerCommand command =
      ScannerActionToCommand(NewContactAction(/*given_name=*/""));

  EXPECT_THAT(
      command,
      VariantWith<OpenUrlCommand>(FieldsAre(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleContactsHost),
          Property("path_piece", &GURL::path_piece, kGoogleContactsNewPath),
          Property("query_piece", &GURL::query_piece, "")))));
}

TEST(ScannerActionToCommandTest, NewContactWithGivenName) {
  ScannerCommand command = ScannerActionToCommand(NewContactAction("Léa"));

  EXPECT_THAT(
      command,
      VariantWith<OpenUrlCommand>(FieldsAre(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleContactsHost),
          Property("path_piece", &GURL::path_piece, kGoogleContactsNewPath),
          Property("query_piece", &GURL::query_piece,
                   "given_name=L%C3%A9a")))));
}

TEST(ScannerActionToCommandTest, NewGoogleDoc) {
  ScannerCommand command = ScannerActionToCommand(
      NewGoogleDocAction("Doc Title", "<span>Contents</span>"));

  EXPECT_THAT(
      command,
      VariantWith<DriveUploadCommand>(FieldsAre(
          "Doc Title", "<span>Contents</span>",
          /*contents_mime_type=*/"text/html",
          /*converted_mime_type=*/drive::util::kGoogleDocumentMimeType)));
}

TEST(ScannerActionToCommandTest, NewGoogleSheet) {
  ScannerCommand command =
      ScannerActionToCommand(NewGoogleSheetAction("Sheet Title", "a,b\n1,2"));

  EXPECT_THAT(
      command,
      VariantWith<DriveUploadCommand>(FieldsAre(
          "Sheet Title", "a,b\n1,2",
          /*contents_mime_type=*/"text/csv",
          /*converted_mime_type=*/drive::util::kGoogleSpreadsheetMimeType)));
}

TEST(ScannerActionToCommandTest, CopyToClipboard) {
  ScannerCommand command =
      ScannerActionToCommand(CopyToClipboardAction("Hello", "<b>Hello</b>"));

  EXPECT_THAT(command, VariantWith<CopyToClipboardAction>(
                           FieldsAre("Hello", "<b>Hello</b>")));
}

TEST(ScannerActionHandlerTest, HandlesOpenUrlCommandWithoutDelegate) {
  base::test::SingleThreadTaskEnvironment task_environment;

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(nullptr, OpenUrlCommand(GURL("https://example.com")),
                       done_future.GetCallback());

  EXPECT_FALSE(done_future.Get());
}

TEST(ScannerActionHandlerTest, OpenUrlCommandOpensUrl) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestScannerCommandDelegate delegate;
  EXPECT_CALL(delegate, OpenUrl(GURL("https://example.com"))).Times(1);

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(delegate.GetWeakPtr(),
                       OpenUrlCommand(GURL("https://example.com")),
                       done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
}

TEST(ScannerActionHandlerTest, HandlesDriveUploadCommandWithoutDelegate) {
  base::test::TaskEnvironment task_environment;

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(
      nullptr,
      DriveUploadCommand(
          "Doc Title", "<span>Contents</span>",
          /*contents_mime_type=*/"text/html",
          /*converted_mime_type=*/drive::util::kGoogleDocumentMimeType),
      done_future.GetCallback());

  EXPECT_FALSE(done_future.Get());
}

TEST(ScannerActionHandlerTest,
     HandlesDriveUploadCommandWithDelayedDelegateDeletion) {
  base::test::TaskEnvironment task_environment;

  base::test::TestFuture<bool> done_future;
  {
    TestScannerCommandDelegate delegate;
    EXPECT_CALL(delegate, GetDriveService).Times(0);
    HandleScannerCommand(
        delegate.GetWeakPtr(),
        DriveUploadCommand(
            "Doc Title", "<span>Contents</span>",
            /*contents_mime_type=*/"text/html",
            /*converted_mime_type=*/drive::util::kGoogleDocumentMimeType),
        done_future.GetCallback());
    // `delegate` is deleted here, invalidating weak pointers.
  }
  ASSERT_FALSE(done_future.IsReady());

  EXPECT_FALSE(done_future.Get());
}

TEST(ScannerActionHandlerTest, DriveUploadCommandOpensAlternateLink) {
  base::test::TaskEnvironment task_environment;
  drive::FakeDriveService drive_service;

  TestScannerCommandDelegate delegate;
  EXPECT_CALL(delegate, GetDriveService).WillRepeatedly(Return(&drive_service));
  EXPECT_CALL(delegate,
              OpenUrl(GURL("https://document_alternate_link/Doc%20Title")))
      .Times(1);

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(
      delegate.GetWeakPtr(),
      DriveUploadCommand(
          "Doc Title", "<span>Contents</span>",
          /*contents_mime_type=*/"text/html",
          /*converted_mime_type=*/drive::util::kGoogleDocumentMimeType),
      done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
}

TEST(ScannerActionHandlerTest,
     DriveUploadCommandCreatesFileWithTitleAndMimeTypeInRoot) {
  base::test::TaskEnvironment task_environment;
  drive::FakeDriveService drive_service;

  {
    TestScannerCommandDelegate delegate;
    EXPECT_CALL(delegate, GetDriveService)
        .WillRepeatedly(Return(&drive_service));
    EXPECT_CALL(delegate, OpenUrl).Times(1);

    base::test::TestFuture<bool> done_future;
    HandleScannerCommand(
        delegate.GetWeakPtr(),
        DriveUploadCommand(
            "Doc Title", "<span>Contents</span>",
            /*contents_mime_type=*/"text/html",
            /*converted_mime_type=*/drive::util::kGoogleDocumentMimeType),
        done_future.GetCallback());

    ASSERT_TRUE(done_future.Get());
  }

  base::test::TestFuture<google_apis::ApiErrorCode,
                         std::unique_ptr<google_apis::FileList>>
      file_list_future;
  drive_service.SearchByTitle("Doc Title", drive_service.GetRootResourceId(),
                              file_list_future.GetCallback());
  auto [error, file_list] = file_list_future.Take();
  EXPECT_EQ(error, google_apis::ApiErrorCode::HTTP_SUCCESS);
  EXPECT_THAT(
      file_list,
      Pointee(Property(
          "items", &google_apis::FileList::items,
          ElementsAre(Pointee(AllOf(
              Property("title", &google_apis::FileResource::title, "Doc Title"),
              Property("mime_type", &google_apis::FileResource::mime_type,
                       drive::util::kGoogleDocumentMimeType),
              Property("parents", &google_apis::FileResource::parents,
                       ElementsAre(Property(
                           "file_id", &google_apis::ParentReference::file_id,
                           drive_service.GetRootResourceId())))))))));
}

TEST(ScannerActionHandlerTest, HandlesCopyToClipboardActionWithoutDelegate) {
  base::test::SingleThreadTaskEnvironment task_environment;

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(nullptr, CopyToClipboardAction("Hello", "<b>Hello</b>"),
                       done_future.GetCallback());

  EXPECT_FALSE(done_future.Get());
}

TEST(ScannerActionHandlerTest, HandlesCopyToClipboardActionOnlyPlainText) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestScannerCommandDelegate delegate;
  EXPECT_CALL(
      delegate,
      SetClipboard(Pointee(
          AllOf(Property("format", &ui::ClipboardData::format,
                         static_cast<int>(ui::ClipboardInternalFormat::kText)),
                Property("text", &ui::ClipboardData::text, "Hello")))))
      .Times(1);

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(delegate.GetWeakPtr(),
                       CopyToClipboardAction("Hello", /*html_text=*/""),
                       done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
}

TEST(ScannerActionHandlerTest, HandlesCopyToClipboardActionOnlyHtmlText) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestScannerCommandDelegate delegate;
  EXPECT_CALL(
      delegate,
      SetClipboard(Pointee(
          AllOf(Property("format", &ui::ClipboardData::format,
                         static_cast<int>(ui::ClipboardInternalFormat::kHtml)),
                Property("markup_data", &ui::ClipboardData::markup_data,
                         "<img />")))))
      .Times(1);

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(delegate.GetWeakPtr(),
                       CopyToClipboardAction(/*plain_text=*/"", "<img />"),
                       done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
}

TEST(ScannerActionHandlerTest, HandlesCopyToClipboardActionAllSet) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestScannerCommandDelegate delegate;
  EXPECT_CALL(
      delegate,
      SetClipboard(Pointee(AllOf(
          Property("format", &ui::ClipboardData::format,
                   static_cast<int>(ui::ClipboardInternalFormat::kText) |
                       static_cast<int>(ui::ClipboardInternalFormat::kHtml)),
          Property("text", &ui::ClipboardData::text, "Hello"),
          Property("markup_data", &ui::ClipboardData::markup_data,
                   "<b>Hello</b>")))))
      .Times(1);

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(delegate.GetWeakPtr(),
                       CopyToClipboardAction("Hello", "<b>Hello</b>"),
                       done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
}

}  // namespace
}  // namespace ash
