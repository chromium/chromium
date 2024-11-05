// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_handler.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/scanner/scanner_command.h"
#include "ash/scanner/scanner_command_delegate.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/service/drive_api_service.h"
#include "components/drive/service/drive_service_interface.h"
#include "components/manta/proto/scanner.pb.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/people/people_api_request_types.h"
#include "google_apis/people/people_api_requests.h"
#include "google_apis/people/people_api_response_types.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The prefix that all `Person.resourceName`s from the People API should start
// with.
constexpr std::string_view kPersonResourceNamePrefix = "people/";
// The path for the Contacts web UI that displays a Person.
constexpr std::string_view kPersonContactsWebUiPath = "/person/";

const GURL& GetCalendarEventTemplateUrl() {
  // Required to delay the creation of this GURL to avoid hitting the
  // `url::DoSchemeModificationPreamble` DCHECK.
  static GURL kGoogleCalendarEventTemplateUrl(
      "https://calendar.google.com/calendar/render?action=TEMPLATE");
  return kGoogleCalendarEventTemplateUrl;
}

const GURL& GetGoogleContactsBaseUrl() {
  static GURL kGoogleContactsBaseUrl("https://contacts.google.com/");
  return kGoogleContactsBaseUrl;
}

GURL GetCalendarEventUrl(const manta::proto::NewEventAction& event) {
  std::string query = GetCalendarEventTemplateUrl().query();
  CHECK(!query.empty());
  if (!event.title().empty()) {
    query += "&text=";
    query += base::EscapeQueryParamValue(event.title(), /*use_plus=*/true);
  }
  if (!event.description().empty()) {
    query += "&details=";
    query +=
        base::EscapeQueryParamValue(event.description(), /*use_plus=*/true);
  }
  if (!event.dates().empty()) {
    query += "&dates=";
    query += base::EscapeQueryParamValue(event.dates(), /*use_plus=*/true);
  }
  if (!event.location().empty()) {
    query += "&location=";
    query += base::EscapeQueryParamValue(event.location(), /*use_plus=*/true);
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return GetCalendarEventTemplateUrl().ReplaceComponents(replacements);
}

// Given a resource name of a Person from the People API, returns a URL to the
// "edit" view of that Person in the Google Contacts web interface.
// Returns an invalid GURL if the resource name is invalid.
GURL GetEditContactUrl(std::string_view resource_name) {
  if (!resource_name.starts_with(kPersonResourceNamePrefix)) {
    // Resource names are guaranteed by the People API documentation to start
    // with the prefix ("people/");
    return GURL();
  }
  resource_name.remove_prefix(kPersonResourceNamePrefix.size());
  GURL::Replacements replacements;
  std::string path = base::StrCat({kPersonContactsWebUiPath, resource_name});
  replacements.SetPathStr(path);
  replacements.SetQueryStr("edit=1");
  GURL edit_contact_url =
      GetGoogleContactsBaseUrl().ReplaceComponents(replacements);

  if (!edit_contact_url.path_piece().starts_with(kPersonContactsWebUiPath)) {
    // The resulting URL's path should always start with the given Contacts web
    // UI path.
    // This may be indicative of a path traversal attack.
    return GURL();
  }

  return edit_contact_url;
}

// Opens the supplied URL in a browser tab using the provided
// `ScannerCommandDelegate`. Calls the callback depending on whether the
// URL was opened or not (if the delegate was null).
// Must be called on the same sequence that called `HandleScannerAction`.
void OpenInBrowserTab(base::WeakPtr<ScannerCommandDelegate> delegate,
                      const GURL& gurl,
                      ScannerCommandCallback callback) {
  if (delegate == nullptr) {
    std::move(callback).Run(false);
    return;
  }
  delegate->OpenUrl(gurl);
  std::move(callback).Run(true);
}

// Writes the given data into a temporary file, then returns the temporary file.
// If any I/O fails, this function returns nullopt and cleans up the temporary
// file.
// Must be run on a thread which can perform I/O.
// The returned `ScopedTempFile` must be destructed on a thread which can
// perform I/O.
//
// Care should be taken to ensure that binding this function should take
// ownership of the data to prevent UAFs. To be specific, `data` should be bound
// with a `std::string` instead of a `std::string_view` so the bound callback
// takes ownership of the data given.
//
// This is because `base::BindOnce`'s internal storage type,
// `base::internal::BindState::BoundArgsTuple`, depends on the arguments to
// `base::BindOnce`, not the parameters of the function.
std::optional<base::ScopedTempFile> CreateTempFileWithContents(
    std::string_view data) {
  base::ScopedTempFile temp_file;
  if (!temp_file.Create()) {
    return std::nullopt;
  }

  base::File file(temp_file.path(),
                  base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    return std::nullopt;
  }
  if (!file.WriteAndCheck(0, base::as_byte_span(data))) {
    return std::nullopt;
  }

  return temp_file;
}

// Runs after a temporary file has been uploaded to Google Drive. Cleans up the
// temporary file and opens the file's alternate link in a browser tab.
// Must be called on the same sequence that called `HandleScannerAction`.
void OnTempFileUploaded(base::WeakPtr<ScannerCommandDelegate> delegate,
                        ScannerCommandCallback callback,
                        base::ScopedTempFile temp_file,
                        google_apis::ApiErrorCode error,
                        std::unique_ptr<google_apis::FileResource> entry) {
  // Ensure that `temp_file` is cleaned up in a thread that can perform I/O.
  absl::Cleanup temp_file_cleanup = [temp_file =
                                         std::move(temp_file)]() mutable {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::DoNothingWithBoundArgs(std::move(temp_file)));
  };

  // `FakeDriveService` returns `HTTP_CREATED` when multipart files are uploaded
  // successfully. The real API returns `HTTP_SUCCESS`.
  // Either one indicates a successful upload.
  if (error != google_apis::ApiErrorCode::HTTP_SUCCESS &&
      error != google_apis::ApiErrorCode::HTTP_CREATED) {
    std::move(callback).Run(false);
    return;
  }
  if (entry == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  OpenInBrowserTab(std::move(delegate), entry->alternate_link(),
                   std::move(callback));
}

// Uploads a specified temporary file to Drive with the provided MIME types.
// `converted_mime_type` must have static lifetime, i.e. must be a compile-time
// constant.
// Must be called on the same sequence that called `HandleScannerAction`.
void UploadTempFileToDrive(base::WeakPtr<ScannerCommandDelegate> delegate,
                           std::string contents_mime_type,
                           std::string_view converted_mime_type,
                           size_t contents_size,
                           std::string title,
                           ScannerCommandCallback callback,
                           std::optional<base::ScopedTempFile> temp_file) {
  if (!temp_file.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  if (delegate == nullptr) {
    std::move(callback).Run(false);
    return;
  }
  drive::DriveServiceInterface* drive_service = delegate->GetDriveService();
  if (drive_service == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  // We need to create a copy of the temp file path, as `MultipartUploadNewFile`
  // takes in a const ref. Without the copy, the const ref would point to a
  // *moved* `base::FilePath` as we move `temp_file` into `OnTempFileUploaded`.
  base::FilePath temp_path = temp_file->path();
  drive_service->MultipartUploadNewFile(
      contents_mime_type, converted_mime_type, contents_size,
      drive_service->GetRootResourceId(), title, temp_path,
      drive::UploadNewFileOptions(),
      base::BindOnce(&OnTempFileUploaded, std::move(delegate),
                     std::move(callback), std::move(*temp_file)),
      /*progress_callback=*/base::NullCallback());
}

void HandleDriveUploadCommand(base::WeakPtr<ScannerCommandDelegate> delegate,
                              DriveUploadCommand command,
                              ScannerCommandCallback callback) {
  size_t contents_size = command.contents.size();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(&CreateTempFileWithContents, std::move(command.contents)),
      base::BindOnce(&UploadTempFileToDrive, std::move(delegate),
                     std::move(command.contents_mime_type),
                     std::move(command.converted_mime_type), contents_size,
                     std::move(command.title), std::move(callback)));
}

std::unique_ptr<ui::ClipboardData> ClipboardDataFromAction(
    manta::proto::CopyToClipboardAction action) {
  auto data = std::make_unique<ui::ClipboardData>();
  if (!action.plain_text().empty()) {
    data->set_text(std::move(*action.mutable_plain_text()));
  }
  if (!action.html_text().empty()) {
    data->set_markup_data(std::move(*action.mutable_html_text()));
  }
  return data;
}

// Returns the `google_apis::people::Contact` from the given new contact action.
google_apis::people::Contact ContactFromAction(
    manta::proto::NewContactAction action) {
  google_apis::people::Contact contact;

  // `google_apis::people::Name` will not be serialised if all field are empty,
  // so if the action's name fields are not set, the below would be a no-op.
  contact.name.family_name = std::move(*action.mutable_family_name());
  contact.name.given_name = std::move(*action.mutable_given_name());

  if (action.email_addresses_size() > 0) {
    contact.email_addresses = base::ToVector(
        *action.mutable_email_addresses(),
        [](manta::proto::NewContactAction::EmailAddress& proto_email) {
          google_apis::people::EmailAddress email_address;
          email_address.value = std::move(*proto_email.mutable_value());
          email_address.type = std::move(*proto_email.mutable_type());
          return email_address;
        });
  } else if (!action.email().empty()) {
    google_apis::people::EmailAddress email_address;
    email_address.value = std::move(*action.mutable_email());
    contact.email_addresses.push_back(std::move(email_address));
  }

  if (action.phone_numbers_size() > 0) {
    contact.phone_numbers = base::ToVector(
        *action.mutable_phone_numbers(),
        [](manta::proto::NewContactAction::PhoneNumber& proto_phone) {
          google_apis::people::PhoneNumber phone_number;
          phone_number.value = std::move(*proto_phone.mutable_value());
          phone_number.type = std::move(*proto_phone.mutable_type());
          return phone_number;
        });
  } else if (!action.phone().empty()) {
    google_apis::people::PhoneNumber phone_number;
    phone_number.value = std::move(*action.mutable_phone());
    contact.phone_numbers.push_back(std::move(phone_number));
  }

  return contact;
}

// Run when the create contact request to the People API finishes.
void OnContactCreated(base::WeakPtr<ScannerCommandDelegate> delegate,
                      ScannerCommandCallback callback,
                      base::expected<google_apis::people::Person,
                                     google_apis::ApiErrorCode> result) {
  if (!result.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  GURL edit_contact_url = GetEditContactUrl(result->resource_name);
  if (!edit_contact_url.is_valid()) {
    std::move(callback).Run(false);
    return;
  }

  OpenInBrowserTab(std::move(delegate), edit_contact_url, std::move(callback));
}

}  // namespace

ScannerCommand ScannerActionToCommand(ScannerAction action) {
  return std::visit(
      base::Overloaded{
          [&](manta::proto::NewEventAction& action) -> ScannerCommand {
            return OpenUrlCommand(GetCalendarEventUrl(action));
          },
          [&](manta::proto::NewContactAction& action) -> ScannerCommand {
            return CreateContactCommand(ContactFromAction(std::move(action)));
          },
          [&](manta::proto::NewGoogleDocAction& action) -> ScannerCommand {
            return DriveUploadCommand(
                std::move(*action.mutable_title()),
                std::move(*action.mutable_html_contents()),
                /*contents_mime_type=*/"text/html",
                /*converted_mime_type=*/drive::util::kGoogleDocumentMimeType);
          },
          [&](manta::proto::NewGoogleSheetAction& action) -> ScannerCommand {
            return DriveUploadCommand(std::move(*action.mutable_title()),
                                      std::move(*action.mutable_csv_contents()),
                                      /*contents_mime_type=*/"text/csv",
                                      /*converted_mime_type=*/
                                      drive::util::kGoogleSpreadsheetMimeType);
          },
          [&](manta::proto::CopyToClipboardAction& action) -> ScannerCommand {
            return CopyToClipboardCommand(
                ClipboardDataFromAction(std::move(action)));
          },
      },
      action);
}

void HandleScannerCommand(base::WeakPtr<ScannerCommandDelegate> delegate,
                          ScannerCommand command,
                          ScannerCommandCallback callback) {
  if (delegate == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  std::visit(
      base::Overloaded{
          [&](OpenUrlCommand& command) {
            OpenInBrowserTab(std::move(delegate), command.url,
                             std::move(callback));
          },
          [&](DriveUploadCommand& command) {
            HandleDriveUploadCommand(std::move(delegate), std::move(command),
                                     std::move(callback));
          },
          [&](CopyToClipboardCommand& command) {
            delegate->SetClipboard(std::move(command.clipboard_data));
            std::move(callback).Run(true);
          },
          [&](CreateContactCommand& command) {
            google_apis::RequestSender* request_sender =
                delegate->GetGoogleApisRequestSender();

            if (request_sender == nullptr) {
              std::move(callback).Run(false);
              return;
            }

            request_sender->StartRequestWithAuthRetry(
                std::make_unique<google_apis::people::CreateContactRequest>(
                    request_sender, std::move(command.contact),
                    base::BindOnce(&OnContactCreated, std::move(delegate),
                                   std::move(callback))));
          },
      },
      command);
}

}  // namespace ash
