// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_handler.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/scanner/scanner_command.h"
#include "ash/scanner/scanner_command_delegate.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/service/drive_service_interface.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/manta/proto/scanner.pb.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"
#include "google_apis/people/people_api_request_types.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::base::test::IsJson;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::HasSubstr;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::VariantWith;

constexpr std::string_view kJsonMimeType = "application/json";

class TestScannerCommandDelegate : public ScannerCommandDelegate {
 public:
  MOCK_METHOD(void, OpenUrl, (const GURL& url), (override));
  MOCK_METHOD(drive::DriveServiceInterface*, GetDriveService, (), (override));
  MOCK_METHOD(google_apis::RequestSender*,
              GetGoogleApisRequestSender,
              (),
              (override));
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

// gMock matchers must match on const refs. Turning a `Contact` into a
// `base::Value::Dict` requires a rvalue reference, so explicitly create a copy
// to turn it into a dict.
base::Value::Dict ContactToDict(const google_apis::people::Contact& contact) {
  return google_apis::people::Contact(contact).ToDict();
}

TEST(ScannerActionToCommandTest, NewEvent) {
  ScannerCommand command =
      ScannerActionToCommand(manta::proto::NewEventAction());

  EXPECT_THAT(
      command,
      VariantWith<OpenUrlCommand>(FieldsAre(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property("query_piece", &GURL::query_piece, "action=TEMPLATE")))));
}

TEST(ScannerActionToCommandTest, NewEventWithTitle) {
  manta::proto::NewEventAction action;
  action.set_title("Test title?");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  EXPECT_THAT(
      command,
      VariantWith<OpenUrlCommand>(FieldsAre(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property("query_piece", &GURL::query_piece,
                   "action=TEMPLATE&text=Test+title%3F")))));
}

TEST(ScannerActionToCommandTest, NewEventWithDescription) {
  manta::proto::NewEventAction action;
  action.set_description("Test desc?");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  EXPECT_THAT(
      command,
      VariantWith<OpenUrlCommand>(FieldsAre(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property("query_piece", &GURL::query_piece,
                   "action=TEMPLATE&details=Test+desc%3F")))));
}

TEST(ScannerActionToCommandTest, NewEventWithDates) {
  manta::proto::NewEventAction action;
  action.set_dates("20241014T160000/20241014T161500");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  EXPECT_THAT(
      command,
      VariantWith<OpenUrlCommand>(FieldsAre(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property(
              "query_piece", &GURL::query_piece,
              "action=TEMPLATE&dates=20241014T160000%2F20241014T161500")))));
}

TEST(ScannerActionToCommandTest, NewEventWithLocation) {
  manta::proto::NewEventAction action;
  action.set_location("401 - Unauthorized");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  EXPECT_THAT(
      command,
      VariantWith<OpenUrlCommand>(FieldsAre(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property("query_piece", &GURL::query_piece,
                   "action=TEMPLATE&location=401+-+Unauthorized")))));
}

TEST(ScannerActionToCommandTest, NewEventWithMultipleFields) {
  manta::proto::NewEventAction action;
  action.set_title("🌏");
  action.set_description("formerly \"Geo Sync\"");
  action.set_dates("20241014T160000/20241014T161500");
  action.set_location("Wonderland");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  EXPECT_THAT(
      command,
      VariantWith<OpenUrlCommand>(FieldsAre(AllOf(
          Property("host_piece", &GURL::host_piece, kGoogleCalendarHost),
          Property("path_piece", &GURL::path_piece, kGoogleCalendarRenderPath),
          Property("query_piece", &GURL::query_piece,
                   "action=TEMPLATE"
                   "&text=%F0%9F%8C%8F"
                   "&details=formerly+%22Geo+Sync%22"
                   "&dates=20241014T160000%2F20241014T161500"
                   "&location=Wonderland")))));
}

TEST(ScannerActionToCommandTest, NewContact) {
  ScannerCommand command =
      ScannerActionToCommand(manta::proto::NewContactAction());

  EXPECT_THAT(std::move(command), VariantWith<CreateContactCommand>(FieldsAre(
                                      ResultOf(&ContactToDict, IsJson("{}")))));
}

TEST(ScannerActionToCommandTest, NewContactWithGivenName) {
  manta::proto::NewContactAction action;
  action.set_given_name("Léa");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  constexpr std::string_view kExpectedJson = R"json({
    "names": [
      {
        "givenName": "Léa",
      },
    ],
  })json";

  EXPECT_THAT(std::move(command),
              VariantWith<CreateContactCommand>(
                  FieldsAre(ResultOf(&ContactToDict, IsJson(kExpectedJson)))));
}

TEST(ScannerActionToCommandTest, NewContactWithFamilyName) {
  manta::proto::NewContactAction action;
  action.set_family_name("François");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  constexpr std::string_view kExpectedJson = R"json({
    "names": [
      {
        "familyName": "François",
      },
    ],
  })json";

  EXPECT_THAT(std::move(command),
              VariantWith<CreateContactCommand>(
                  FieldsAre(ResultOf(&ContactToDict, IsJson(kExpectedJson)))));
}

TEST(ScannerActionToCommandTest, NewContactWithDeprecatedEmail) {
  manta::proto::NewContactAction action;
  action.set_email("afrancois@example.com");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  constexpr std::string_view kExpectedJson = R"json({
    "emailAddresses": [
      {
        "value": "afrancois@example.com",
      },
    ],
  })json";

  EXPECT_THAT(std::move(command),
              VariantWith<CreateContactCommand>(
                  FieldsAre(ResultOf(&ContactToDict, IsJson(kExpectedJson)))));
}

TEST(ScannerActionToCommandTest, NewContactWithEmailAddresses) {
  manta::proto::NewContactAction action;
  manta::proto::NewContactAction::EmailAddress& home_email =
      *action.add_email_addresses();
  home_email.set_value("afrancois@example.com");
  home_email.set_type("home");
  manta::proto::NewContactAction::EmailAddress& work_email =
      *action.add_email_addresses();
  work_email.set_value("afrancois@work.example.com");
  work_email.set_type("work");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  constexpr std::string_view kExpectedJson = R"json({
    "emailAddresses": [
      {
        "value": "afrancois@example.com",
        "type": "home",
      },
      {
        "value": "afrancois@work.example.com",
        "type": "work",
      },
    ],
  })json";

  EXPECT_THAT(std::move(command),
              VariantWith<CreateContactCommand>(
                  FieldsAre(ResultOf(&ContactToDict, IsJson(kExpectedJson)))));
}

TEST(ScannerActionToCommandTest,
     NewContactWithEmailAddressesAndDeprecatedEmail) {
  manta::proto::NewContactAction action;
  action.set_email("afrancois@example.com");
  manta::proto::NewContactAction::EmailAddress& home_email =
      *action.add_email_addresses();
  home_email.set_value("afrancois@example.com");
  home_email.set_type("home");
  manta::proto::NewContactAction::EmailAddress& work_email =
      *action.add_email_addresses();
  work_email.set_value("afrancois@work.example.com");
  work_email.set_type("work");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  constexpr std::string_view kExpectedJson = R"json({
    "emailAddresses": [
      {
        "value": "afrancois@example.com",
        "type": "home",
      },
      {
        "value": "afrancois@work.example.com",
        "type": "work",
      },
    ],
  })json";

  EXPECT_THAT(std::move(command),
              VariantWith<CreateContactCommand>(
                  FieldsAre(ResultOf(&ContactToDict, IsJson(kExpectedJson)))));
}

TEST(ScannerActionToCommandTest, NewContactWithDeprecatedPhone) {
  manta::proto::NewContactAction action;
  action.set_phone("+61400000000");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  constexpr std::string_view kExpectedJson = R"json({
    "phoneNumbers": [
      {
        "value": "+61400000000",
      },
    ],
  })json";

  EXPECT_THAT(std::move(command),
              VariantWith<CreateContactCommand>(
                  FieldsAre(ResultOf(&ContactToDict, IsJson(kExpectedJson)))));
}

TEST(ScannerActionToCommandTest, NewContactWithPhoneNumbers) {
  manta::proto::NewContactAction action;
  manta::proto::NewContactAction::PhoneNumber& mobile_number =
      *action.add_phone_numbers();
  mobile_number.set_value("+61400000000");
  mobile_number.set_type("mobile");
  manta::proto::NewContactAction::PhoneNumber& home_number =
      *action.add_phone_numbers();
  home_number.set_value("+61390000000");
  home_number.set_type("home");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  constexpr std::string_view kExpectedJson = R"json({
    "phoneNumbers": [
      {
        "value": "+61400000000",
        "type": "mobile",
      },
      {
        "value": "+61390000000",
        "type": "home",
      },
    ],
  })json";

  EXPECT_THAT(std::move(command),
              VariantWith<CreateContactCommand>(
                  FieldsAre(ResultOf(&ContactToDict, IsJson(kExpectedJson)))));
}

TEST(ScannerActionToCommandTest, NewContactWithPhoneNumbersAndDeprecatedPhone) {
  manta::proto::NewContactAction action;
  action.set_phone("+61400000000");
  manta::proto::NewContactAction::PhoneNumber& mobile_number =
      *action.add_phone_numbers();
  mobile_number.set_value("+61400000000");
  mobile_number.set_type("mobile");
  manta::proto::NewContactAction::PhoneNumber& home_number =
      *action.add_phone_numbers();
  home_number.set_value("+61390000000");
  home_number.set_type("home");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  constexpr std::string_view kExpectedJson = R"json({
    "phoneNumbers": [
      {
        "value": "+61400000000",
        "type": "mobile",
      },
      {
        "value": "+61390000000",
        "type": "home",
      },
    ],
  })json";

  EXPECT_THAT(std::move(command),
              VariantWith<CreateContactCommand>(
                  FieldsAre(ResultOf(&ContactToDict, IsJson(kExpectedJson)))));
}

TEST(ScannerActionToCommandTest, NewContactWithMultipleFields) {
  manta::proto::NewContactAction action;
  action.set_given_name("André");
  action.set_family_name("François");
  manta::proto::NewContactAction::EmailAddress& home_email =
      *action.add_email_addresses();
  home_email.set_value("afrancois@example.com");
  home_email.set_type("home");
  manta::proto::NewContactAction::EmailAddress& work_email =
      *action.add_email_addresses();
  work_email.set_value("afrancois@work.example.com");
  work_email.set_type("work");
  manta::proto::NewContactAction::PhoneNumber& mobile_number =
      *action.add_phone_numbers();
  mobile_number.set_value("+61400000000");
  mobile_number.set_type("mobile");
  manta::proto::NewContactAction::PhoneNumber& home_number =
      *action.add_phone_numbers();
  home_number.set_value("+61390000000");
  home_number.set_type("home");
  ScannerCommand command = ScannerActionToCommand(std::move(action));

  constexpr std::string_view kExpectedJson = R"json({
    "names": [
      {
        "givenName": "André",
        "familyName": "François",
      },
    ],
    "emailAddresses": [
      {
        "value": "afrancois@example.com",
        "type": "home",
      },
      {
        "value": "afrancois@work.example.com",
        "type": "work",
      },
    ],
    "phoneNumbers": [
      {
        "value": "+61400000000",
        "type": "mobile",
      },
      {
        "value": "+61390000000",
        "type": "home",
      },
    ],
  })json";

  EXPECT_THAT(std::move(command),
              VariantWith<CreateContactCommand>(
                  FieldsAre(ResultOf(&ContactToDict, IsJson(kExpectedJson)))));
}

TEST(ScannerActionToCommandTest, NewGoogleDoc) {
  manta::proto::NewGoogleDocAction action;
  action.set_title("Doc Title");
  action.set_html_contents("<span>Contents</span>");

  ScannerCommand command = ScannerActionToCommand(std::move(action));

  EXPECT_THAT(
      command,
      VariantWith<DriveUploadCommand>(FieldsAre(
          "Doc Title", "<span>Contents</span>",
          /*contents_mime_type=*/"text/html",
          /*converted_mime_type=*/drive::util::kGoogleDocumentMimeType)));
}

TEST(ScannerActionToCommandTest, NewGoogleSheet) {
  manta::proto::NewGoogleSheetAction action;
  action.set_title("Sheet Title");
  action.set_csv_contents("a,b\n1,2");

  ScannerCommand command = ScannerActionToCommand(std::move(action));

  EXPECT_THAT(
      command,
      VariantWith<DriveUploadCommand>(FieldsAre(
          "Sheet Title", "a,b\n1,2",
          /*contents_mime_type=*/"text/csv",
          /*converted_mime_type=*/drive::util::kGoogleSpreadsheetMimeType)));
}

TEST(ScannerActionHandlerTest, CopyToClipboardWithPlainText) {
  manta::proto::CopyToClipboardAction action;
  action.set_plain_text("Hello");

  ScannerCommand command = ScannerActionToCommand(std::move(action));

  EXPECT_THAT(
      command,
      VariantWith<CopyToClipboardCommand>(FieldsAre(Pointee(
          AllOf(Property("format", &ui::ClipboardData::format,
                         static_cast<int>(ui::ClipboardInternalFormat::kText)),
                Property("text", &ui::ClipboardData::text, "Hello"))))));
}

TEST(ScannerActionHandlerTest, CopyToClipboardWithHtmlText) {
  manta::proto::CopyToClipboardAction action;
  action.set_html_text("<img />");

  ScannerCommand command = ScannerActionToCommand(std::move(action));

  EXPECT_THAT(
      command,
      VariantWith<CopyToClipboardCommand>(FieldsAre(Pointee(
          AllOf(Property("format", &ui::ClipboardData::format,
                         static_cast<int>(ui::ClipboardInternalFormat::kHtml)),
                Property("markup_data", &ui::ClipboardData::markup_data,
                         "<img />"))))));
}

TEST(ScannerActionHandlerTest, CopyToClipboardWithMultipleFields) {
  manta::proto::CopyToClipboardAction action;
  action.set_plain_text("Hello");
  action.set_html_text("<b>Hello</b>");

  ScannerCommand command = ScannerActionToCommand(std::move(action));

  EXPECT_THAT(
      command,
      VariantWith<CopyToClipboardCommand>(FieldsAre(Pointee(AllOf(
          Property("format", &ui::ClipboardData::format,
                   static_cast<int>(ui::ClipboardInternalFormat::kText) |
                       static_cast<int>(ui::ClipboardInternalFormat::kHtml)),
          Property("text", &ui::ClipboardData::text, "Hello"),
          Property("markup_data", &ui::ClipboardData::markup_data,
                   "<b>Hello</b>"))))));
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
  HandleScannerCommand(
      nullptr, CopyToClipboardCommand{std::make_unique<ui::ClipboardData>()},
      done_future.GetCallback());

  EXPECT_FALSE(done_future.Get());
}

TEST(ScannerActionHandlerTest, HandlesCopyToClipboardAction) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestScannerCommandDelegate delegate;
  ui::ClipboardData clipboard_data;
  clipboard_data.set_text("Hello");
  clipboard_data.set_markup_data("<b>Hello</b>");
  EXPECT_CALL(delegate, SetClipboard(Pointee(clipboard_data))).Times(1);

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(delegate.GetWeakPtr(),
                       CopyToClipboardCommand(
                           std::make_unique<ui::ClipboardData>(clipboard_data)),
                       done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
}

// Wrapper around an `EmbeddedTestServer` which starts the server in the
// constructor, so a `base_url()` can be obtained immediately after
// construction.
// This is required so `ScannerCreateContactCommandHandlerTest` can initialise
// `gaia_urls_overrider_` in the constructor - which requires `base_url()`.
//
// TODO: b/374624760 - Consider deduplicating this with
// google_apis/people/people_api_requests_unittest.cc.
class MockServer {
 public:
  MockServer() {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &MockServer::HandleRequest, base::Unretained(this)));
    CHECK(test_server_.Start());
  }

  MOCK_METHOD(std::unique_ptr<net::test_server::HttpResponse>,
              HandleRequest,
              (const net::test_server::HttpRequest& request));

  const GURL& base_url() const { return test_server_.base_url(); }

 private:
  net::test_server::EmbeddedTestServer test_server_;
};

// TODO: b/374624760 - Consider deduplicating this with
// google_apis/people/people_api_requests_unittest.cc.
class ScannerCreateContactCommandHandlerTest : public testing::Test {
 public:
  ScannerCreateContactCommandHandlerTest()
      : request_sender_(
            std::make_unique<google_apis::DummyAuthService>(),
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
                /*network_service=*/nullptr,
                /*is_trusted=*/true),
            task_environment_.GetMainThreadTaskRunner(),
            "test-user-agent",
            TRAFFIC_ANNOTATION_FOR_TESTS),
        gaia_urls_overrider_(base::CommandLine::ForCurrentProcess(),
                             "people_api_origin_url",
                             mock_server_.base_url().spec()) {
    CHECK_EQ(mock_server_.base_url(),
             GaiaUrls::GetInstance()->people_api_origin_url());
  }

  google_apis::RequestSender& request_sender() { return request_sender_; }
  MockServer& mock_server() { return mock_server_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  google_apis::RequestSender request_sender_;
  MockServer mock_server_;
  GaiaUrlsOverriderForTesting gaia_urls_overrider_;
};

TEST_F(ScannerCreateContactCommandHandlerTest, WithoutDelegate) {
  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(nullptr,
                       CreateContactCommand(google_apis::people::Contact()),
                       done_future.GetCallback());

  EXPECT_FALSE(done_future.Get());
}

TEST_F(ScannerCreateContactCommandHandlerTest, WithoutRequestSender) {
  TestScannerCommandDelegate delegate;
  EXPECT_CALL(delegate, GetGoogleApisRequestSender).WillOnce(Return(nullptr));

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(delegate.GetWeakPtr(),
                       CreateContactCommand(google_apis::people::Contact()),
                       done_future.GetCallback());

  EXPECT_FALSE(done_future.Get());
}

TEST_F(ScannerCreateContactCommandHandlerTest, WithDelayedDelegateDeletion) {
  // The response must be valid and successful to attempt to open a URL.
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content(R"json({"resourceName": "people/c1"})json");
  response->set_content_type(kJsonMimeType);
  EXPECT_CALL(mock_server(), HandleRequest)
      .WillOnce(Return(std::move(response)));

  base::test::TestFuture<bool> done_future;
  {
    testing::StrictMock<TestScannerCommandDelegate> delegate;
    EXPECT_CALL(delegate, GetGoogleApisRequestSender)
        .WillOnce(Return(&request_sender()));
    HandleScannerCommand(delegate.GetWeakPtr(),
                         CreateContactCommand(google_apis::people::Contact()),
                         done_future.GetCallback());
    // `delegate` is deleted here, invalidating weak pointers.
  }

  EXPECT_FALSE(done_future.Get());
}

TEST_F(ScannerCreateContactCommandHandlerTest, SendsRequestToServer) {
  constexpr std::string_view kContactJson = R"json({
    "emailAddresses": [
      {
        "value": "afrancois@example.com",
        "type": "home",
      },
      {
        "value": "afrancois@work.example.com",
        "type": "work",
      },
    ],
    "names": [
      {
        "familyName": "Francois",
        "givenName": "Andre",
      },
    ],
    "phoneNumbers": [
      {
        "value": "+61400000000",
        "type": "mobile",
      },
      {
        "value": "+61390000000",
        "type": "home",
      },
    ],
  })json";
  // We are not interested in the server response in this test - just the server
  // request - so return any arbitrary response.
  EXPECT_CALL(
      mock_server(),
      HandleRequest(AllOf(
          Field("relative_url", &net::test_server::HttpRequest::relative_url,
                HasSubstr("createContact")),
          Field("content", &net::test_server::HttpRequest::content,
                IsJson(kContactJson)))))
      .WillOnce(
          Return(std::make_unique<net::test_server::BasicHttpResponse>()));
  testing::StrictMock<TestScannerCommandDelegate> delegate;
  EXPECT_CALL(delegate, GetGoogleApisRequestSender)
      .WillOnce(Return(&request_sender()));

  google_apis::people::Contact contact;
  google_apis::people::EmailAddress home_email;
  home_email.value = "afrancois@example.com";
  home_email.type = "home";
  contact.email_addresses.push_back(std::move(home_email));
  google_apis::people::EmailAddress work_email;
  work_email.value = "afrancois@work.example.com";
  work_email.type = "work";
  contact.email_addresses.push_back(std::move(work_email));
  google_apis::people::Name name;
  name.family_name = "Francois";
  name.given_name = "Andre";
  contact.name = std::move(name);
  google_apis::people::PhoneNumber mobile_number;
  mobile_number.value = "+61400000000";
  mobile_number.type = "mobile";
  contact.phone_numbers.push_back(std::move(mobile_number));
  google_apis::people::PhoneNumber home_number;
  home_number.value = "+61390000000";
  home_number.type = "home";
  contact.phone_numbers.push_back(std::move(home_number));
  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(delegate.GetWeakPtr(),
                       CreateContactCommand(std::move(contact)),
                       done_future.GetCallback());

  // We are not interested in the result of the command in this test, just
  // whether the command sends the right JSON to the server.
  // However, we should still wait for the future to be resolved.
  ASSERT_TRUE(done_future.Wait());
}

TEST_F(ScannerCreateContactCommandHandlerTest, OpensEditContactInBrowser) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content(R"json({"resourceName": "people/c1"})json");
  response->set_content_type(kJsonMimeType);
  EXPECT_CALL(mock_server(), HandleRequest)
      .WillOnce(Return(std::move(response)));
  testing::StrictMock<TestScannerCommandDelegate> delegate;
  EXPECT_CALL(delegate, GetGoogleApisRequestSender)
      .WillOnce(Return(&request_sender()));
  EXPECT_CALL(delegate,
              OpenUrl(GURL("https://contacts.google.com/person/c1?edit=1")))
      .Times(1);

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(delegate.GetWeakPtr(),
                       CreateContactCommand(google_apis::people::Contact()),
                       done_future.GetCallback());

  EXPECT_TRUE(done_future.Get());
}

TEST_F(ScannerCreateContactCommandHandlerTest, HandlesServerErrors) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_CALL(mock_server(), HandleRequest)
      .WillOnce(Return(std::move(response)));
  testing::StrictMock<TestScannerCommandDelegate> delegate;
  EXPECT_CALL(delegate, GetGoogleApisRequestSender)
      .WillOnce(Return(&request_sender()));

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(delegate.GetWeakPtr(),
                       CreateContactCommand(google_apis::people::Contact()),
                       done_future.GetCallback());

  EXPECT_FALSE(done_future.Get());
}

TEST_F(ScannerCreateContactCommandHandlerTest, HandlesInvalidResourceNames) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content(R"json({"resourceName": "c1"})json");
  response->set_content_type(kJsonMimeType);
  EXPECT_CALL(mock_server(), HandleRequest)
      .WillOnce(Return(std::move(response)));
  testing::StrictMock<TestScannerCommandDelegate> delegate;
  EXPECT_CALL(delegate, GetGoogleApisRequestSender)
      .WillOnce(Return(&request_sender()));

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(delegate.GetWeakPtr(),
                       CreateContactCommand(google_apis::people::Contact()),
                       done_future.GetCallback());

  EXPECT_FALSE(done_future.Get());
}

TEST_F(ScannerCreateContactCommandHandlerTest,
       HandlesResourceNamesWithPathTraversal) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content(
      R"json({"resourceName": "people/../deleteAccount"})json");
  response->set_content_type(kJsonMimeType);
  EXPECT_CALL(mock_server(), HandleRequest)
      .WillOnce(Return(std::move(response)));
  testing::StrictMock<TestScannerCommandDelegate> delegate;
  EXPECT_CALL(delegate, GetGoogleApisRequestSender)
      .WillOnce(Return(&request_sender()));

  base::test::TestFuture<bool> done_future;
  HandleScannerCommand(delegate.GetWeakPtr(),
                       CreateContactCommand(google_apis::people::Contact()),
                       done_future.GetCallback());

  EXPECT_FALSE(done_future.Get());
}

}  // namespace
}  // namespace ash
