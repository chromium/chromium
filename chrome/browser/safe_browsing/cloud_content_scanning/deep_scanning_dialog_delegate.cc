// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/policy/core/browser/url_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"

namespace safe_browsing {

const base::Feature kDeepScanningOfUploads{"SafeBrowsingDeepScanningOfUploads",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(rogerta): keeping this disabled by default until UX is finalized.
const base::Feature kDeepScanningOfUploadsUI{
    "SafeBrowsingDeepScanningOfUploadsUI", base::FEATURE_DISABLED_BY_DEFAULT};

namespace {

policy::DMToken* GetDMTokenForTestingStorage() {
  static policy::DMToken dm_token_storage =
      policy::DMToken::CreateEmptyTokenForTesting();
  return &dm_token_storage;
}

// Global pointer of factory function (RepeatingCallback) used to create
// instances of DeepScanningDialogDelegate in tests.  !is_null() only in tests.
DeepScanningDialogDelegate::Factory* GetFactoryStorage() {
  static base::NoDestructor<DeepScanningDialogDelegate::Factory> factory;
  return factory.get();
}

// Determines if the completion callback should be called only after all the
// scan requests have finished and the verdicts known.
bool WaitForVerdict() {
  int state = g_browser_process->local_state()->GetInteger(
      prefs::kDelayDeliveryUntilVerdict);
  return state == DELAY_UPLOADS || state == DELAY_UPLOADS_AND_DOWNLOADS;
}

struct FileContents {
  FileContents() : result(BinaryUploadService::Result::UNKNOWN) {}
  explicit FileContents(BinaryUploadService::Result result) : result(result) {}
  FileContents(FileContents&&) = default;
  FileContents& operator=(FileContents&&) = default;

  BinaryUploadService::Result result;
  BinaryUploadService::Request::Data data;
  std::string sha256;
};

// Callback used by FileSourceRequest to read file data on a blocking thread.
FileContents GetFileContentsSHA256Blocking(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return FileContents();

  size_t file_size = file.GetLength();
  if (file_size > BinaryUploadService::kMaxUploadSizeBytes)
    return FileContents(BinaryUploadService::Result::FILE_TOO_LARGE);

  FileContents file_contents;
  file_contents.result = BinaryUploadService::Result::SUCCESS;
  file_contents.data.contents.resize(file_size);

  size_t bytes_read = 0;
  while (bytes_read < file_size) {
    int64_t bytes_currently_read = file.ReadAtCurrentPos(
        &file_contents.data.contents[bytes_read], file_size - bytes_read);
    if (bytes_currently_read == -1)
      return FileContents();

    bytes_read += bytes_currently_read;
  }

  file_contents.sha256 = crypto::SHA256HashString(file_contents.data.contents);
  return file_contents;
}

// A BinaryUploadService::Request implementation that gets the data to scan
// from a string.
class StringSourceRequest : public BinaryUploadService::Request {
 public:
  StringSourceRequest(std::string text, BinaryUploadService::Callback callback);
  ~StringSourceRequest() override;

  StringSourceRequest(const StringSourceRequest&) = delete;
  StringSourceRequest& operator=(const StringSourceRequest&) = delete;

  // BinaryUploadService::Request implementation.
  void GetRequestData(DataCallback callback) override;

 private:
  Data data_;
  BinaryUploadService::Result result_ =
      BinaryUploadService::Result::FILE_TOO_LARGE;
};

StringSourceRequest::StringSourceRequest(std::string text,
                                         BinaryUploadService::Callback callback)
    : Request(std::move(callback)) {
  // Only remember strings less than the maximum allowed.
  if (text.size() < BinaryUploadService::kMaxUploadSizeBytes) {
    data_.contents = std::move(text);
    result_ = BinaryUploadService::Result::SUCCESS;
  }
}

StringSourceRequest::~StringSourceRequest() = default;

void StringSourceRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(result_, data_);
}

bool DlpTriggeredRulesOK(
    const ::safe_browsing::DlpDeepScanningVerdict& verdict) {
  if (verdict.status() != DlpDeepScanningVerdict::SUCCESS)
    return false;

  for (int i = 0; i < verdict.triggered_rules_size(); ++i) {
    if (verdict.triggered_rules(i).action() ==
        DlpDeepScanningVerdict::TriggeredRule::BLOCK) {
      return false;
    }
  }
  return true;
}

// File types supported for DLP scanning.
// Keep sorted for efficient access.
constexpr const std::array<const base::FilePath::CharType*, 36>
    kSupportedDLPFileTypes = {
        FILE_PATH_LITERAL(".bzip"),    FILE_PATH_LITERAL(".cab"),
        FILE_PATH_LITERAL(".doc"),     FILE_PATH_LITERAL(".docx"),
        FILE_PATH_LITERAL(".eps"),     FILE_PATH_LITERAL(".gzip"),
        FILE_PATH_LITERAL(".hwp"),     FILE_PATH_LITERAL(".img_for_ocr"),
        FILE_PATH_LITERAL(".kml"),     FILE_PATH_LITERAL(".kmz"),
        FILE_PATH_LITERAL(".odp"),     FILE_PATH_LITERAL(".ods"),
        FILE_PATH_LITERAL(".odt"),     FILE_PATH_LITERAL(".pdf"),
        FILE_PATH_LITERAL(".ppt"),     FILE_PATH_LITERAL(".pptx"),
        FILE_PATH_LITERAL(".ps"),      FILE_PATH_LITERAL(".rar"),
        FILE_PATH_LITERAL(".rtf"),     FILE_PATH_LITERAL(".sdc"),
        FILE_PATH_LITERAL(".sdd"),     FILE_PATH_LITERAL(".sdw"),
        FILE_PATH_LITERAL(".seven_z"), FILE_PATH_LITERAL(".sxc"),
        FILE_PATH_LITERAL(".sxi"),     FILE_PATH_LITERAL(".sxw"),
        FILE_PATH_LITERAL(".tar"),     FILE_PATH_LITERAL(".ttf"),
        FILE_PATH_LITERAL(".txt"),     FILE_PATH_LITERAL(".wml"),
        FILE_PATH_LITERAL(".wpd"),     FILE_PATH_LITERAL(".xls"),
        FILE_PATH_LITERAL(".xlsx"),    FILE_PATH_LITERAL(".xml"),
        FILE_PATH_LITERAL(".xps"),     FILE_PATH_LITERAL(".zip")};

}  // namespace

// A BinaryUploadService::Request implementation that gets the data to scan
// from the contents of a file.
class DeepScanningDialogDelegate::FileSourceRequest
    : public BinaryUploadService::Request {
 public:
  FileSourceRequest(base::WeakPtr<DeepScanningDialogDelegate> delegate,
                    base::FilePath path,
                    BinaryUploadService::Callback callback);
  FileSourceRequest(const FileSourceRequest&) = delete;
  FileSourceRequest& operator=(const FileSourceRequest&) = delete;
  ~FileSourceRequest() override = default;

 private:
  // BinaryUploadService::Request implementation.
  void GetRequestData(DataCallback callback) override;

  void OnGotFileContents(DataCallback callback, FileContents file_contents);

  base::WeakPtr<DeepScanningDialogDelegate> delegate_;
  base::FilePath path_;
  base::WeakPtrFactory<FileSourceRequest> weakptr_factory_{this};
};

DeepScanningDialogDelegate::FileSourceRequest::FileSourceRequest(
    base::WeakPtr<DeepScanningDialogDelegate> delegate,
    base::FilePath path,
    BinaryUploadService::Callback callback)
    : Request(std::move(callback)),
      delegate_(delegate),
      path_(std::move(path)) {}

void DeepScanningDialogDelegate::FileSourceRequest::GetRequestData(
    DataCallback callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetFileContentsSHA256Blocking, path_),
      base::BindOnce(&FileSourceRequest::OnGotFileContents,
                     weakptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DeepScanningDialogDelegate::FileSourceRequest::OnGotFileContents(
    DataCallback callback,
    FileContents file_contents) {
  if (delegate_)
    delegate_->SetFileInfo(path_, std::move(file_contents.sha256),
                           file_contents.data.contents.length());

  std::move(callback).Run(file_contents.result, file_contents.data);
}

DeepScanningDialogDelegate::Data::Data() = default;
DeepScanningDialogDelegate::Data::Data(Data&& other) = default;
DeepScanningDialogDelegate::Data::~Data() = default;

DeepScanningDialogDelegate::Result::Result() = default;
DeepScanningDialogDelegate::Result::Result(Result&& other) = default;
DeepScanningDialogDelegate::Result::~Result() = default;

DeepScanningDialogDelegate::FileInfo::FileInfo() = default;
DeepScanningDialogDelegate::FileInfo::FileInfo(FileInfo&& other) = default;
DeepScanningDialogDelegate::FileInfo::~FileInfo() = default;

DeepScanningDialogDelegate::~DeepScanningDialogDelegate() = default;

base::string16 DeepScanningDialogDelegate::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_TITLE);
}

base::string16 DeepScanningDialogDelegate::GetDialogMessage() {
  return l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_MESSAGE);
}

int DeepScanningDialogDelegate::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

void DeepScanningDialogDelegate::OnCanceled() {
  if (callback_.is_null())
    return;

  // Make sure to reject everything.
  FillAllResultsWith(false);
  RunCallback();
}

// static
bool DeepScanningDialogDelegate::FileTypeSupported(const bool for_malware_scan,
                                                   const bool for_dlp_scan,
                                                   const base::FilePath& path) {
  // At least one of the booleans needs to be true.
  DCHECK(for_malware_scan || for_dlp_scan);

  // Accept any file type for malware scans.
  if (for_malware_scan)
    return true;

  // Accept any file type in the supported list for DLP scans.
  if (for_dlp_scan) {
    base::FilePath::StringType extension(path.FinalExtension());
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   tolower);
    return std::binary_search(kSupportedDLPFileTypes.begin(),
                              kSupportedDLPFileTypes.end(), extension);
  }

  return false;
}

// static
bool DeepScanningDialogDelegate::IsEnabled(Profile* profile,
                                           GURL url,
                                           Data* data) {
  if (!base::FeatureList::IsEnabled(kDeepScanningOfUploads))
    return false;

  // If this is an incognitio profile, don't perform scans.
  if (profile->IsOffTheRecord())
    return false;

  // If there's no valid DM token, the upload will fail.
  if (!GetDMToken().is_valid())
    return false;

  // See if content compliance checks are needed.

  int state = g_browser_process->local_state()->GetInteger(
      prefs::kCheckContentCompliance);
  data->do_dlp_scan =
      state == CHECK_UPLOADS || state == CHECK_UPLOADS_AND_DOWNLOADS;

  if (data->do_dlp_scan &&
      g_browser_process->local_state()->HasPrefPath(
          prefs::kURLsToNotCheckComplianceOfUploadedContent)) {
    const base::ListValue* filters = g_browser_process->local_state()->GetList(
        prefs::kURLsToNotCheckComplianceOfUploadedContent);
    url_matcher::URLMatcher matcher;
    policy::url_util::AddAllowFilters(&matcher, filters);
    data->do_dlp_scan = matcher.MatchURL(url).empty();
  }

  // See if malware checks are needed.

  state = profile->GetPrefs()->GetInteger(
      prefs::kSafeBrowsingSendFilesForMalwareCheck);
  data->do_malware_scan =
      state == SEND_UPLOADS || state == SEND_UPLOADS_AND_DOWNLOADS;

  if (data->do_malware_scan) {
    if (g_browser_process->local_state()->HasPrefPath(
            prefs::kURLsToCheckForMalwareOfUploadedContent)) {
      const base::ListValue* filters =
          g_browser_process->local_state()->GetList(
              prefs::kURLsToCheckForMalwareOfUploadedContent);
      url_matcher::URLMatcher matcher;
      policy::url_util::AddAllowFilters(&matcher, filters);
      data->do_malware_scan = !matcher.MatchURL(url).empty();
    } else {
      data->do_malware_scan = false;
    }
  }

  return data->do_dlp_scan || data->do_malware_scan;
}

// static
void DeepScanningDialogDelegate::ShowForWebContents(
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback) {
  Factory* testing_factory = GetFactoryStorage();
  bool wait_for_verdict = WaitForVerdict();

  // Using new instead of std::make_unique<> to access non public constructor.
  auto delegate =
      testing_factory->is_null()
          ? std::unique_ptr<DeepScanningDialogDelegate>(
                new DeepScanningDialogDelegate(web_contents, std::move(data),
                                               std::move(callback)))
          : testing_factory->Run(web_contents, std::move(data),
                                 std::move(callback));

  bool work_being_done = delegate->UploadData();

  // Only show UI if work is being done in the background, the user must
  // wait for a verdict, and the UI feature is enabled.
  bool show_ui = work_being_done && wait_for_verdict &&
                 base::FeatureList::IsEnabled(kDeepScanningOfUploadsUI);

  // If the UI is enabled, create the modal dialog.
  if (show_ui) {
    DeepScanningDialogDelegate* delegate_ptr = delegate.get();
    delegate_ptr->dialog_ =
        TabModalConfirmDialog::Create(std::move(delegate), web_contents);
    return;
  }

  if (!wait_for_verdict || !work_being_done) {
    // The UI will not be shown but the policy is set to not wait for the
    // verdict, or no scans need to be performed.  Inform the caller that they
    // may proceed.
    //
    // Supporting "wait for verdict" while not showing a UI makes writing tests
    // for callers of this code easier.
    delegate->FillAllResultsWith(true);
    delegate->RunCallback();
  }

  // Upload service callback will delete the delegate.
  if (work_being_done)
    delegate.release();
}

// static
void DeepScanningDialogDelegate::SetFactoryForTesting(Factory factory) {
  *GetFactoryStorage() = factory;
}

// static
void DeepScanningDialogDelegate::SetDMTokenForTesting(
    const policy::DMToken& dm_token) {
  *GetDMTokenForTestingStorage() = dm_token;
}

DeepScanningDialogDelegate::DeepScanningDialogDelegate(
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback)
    : TabModalConfirmDialogDelegate(web_contents),
      web_contents_(web_contents),
      data_(std::move(data)),
      callback_(std::move(callback)) {
  DCHECK(web_contents_);
  result_.text_results.resize(data_.text.size(), false);
  result_.paths_results.resize(data_.paths.size(), false);
  file_info_.resize(data_.paths.size());
}

void DeepScanningDialogDelegate::StringRequestCallback(
    BinaryUploadService::Result result,
    DeepScanningClientResponse response) {
  MaybeReportDeepScanningVerdict(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      web_contents_->GetLastCommittedURL(), "Text data", std::string(),
      "text/plain",
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      std::accumulate(data_.text.begin(), data_.text.end(), 0,
                      [](int64_t acc, const base::string16& s) {
                        return acc + s.size() * sizeof(base::char16);
                      }),
      result, response);

  text_request_complete_ = true;
  bool text_complies = (result == BinaryUploadService::Result::SUCCESS &&
                        DlpTriggeredRulesOK(response.dlp_scan_verdict()));
  std::fill(result_.text_results.begin(), result_.text_results.end(),
            text_complies);
  MaybeCompleteScanRequest();
}

void DeepScanningDialogDelegate::FileRequestCallback(
    base::FilePath path,
    BinaryUploadService::Result result,
    DeepScanningClientResponse response) {
  // Find the path in the set of files that are being scanned.
  auto it = std::find(data_.paths.begin(), data_.paths.end(), path);
  DCHECK(it != data_.paths.end());
  size_t index = std::distance(data_.paths.begin(), it);

  // TODO(crbug.com/1013252): Obtain a more accurate MimeType by parsing the
  // file content.
  std::string mime_type;
  net::GetMimeTypeFromFile(path, &mime_type);

  MaybeReportDeepScanningVerdict(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      web_contents_->GetLastCommittedURL(), path.AsUTF8Unsafe(),
      file_info_[index].sha256, mime_type,
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      file_info_[index].size, result, response);

  bool dlp_ok = DlpTriggeredRulesOK(response.dlp_scan_verdict());
  bool malware_ok = response.malware_scan_verdict().verdict() !=
                        MalwareDeepScanningVerdict::UWS &&
                    response.malware_scan_verdict().verdict() !=
                        MalwareDeepScanningVerdict::MALWARE;
  bool file_complies =
      (result == BinaryUploadService::Result::SUCCESS) && dlp_ok && malware_ok;

  result_.paths_results[index] = file_complies;

  ++file_result_count_;
  MaybeCompleteScanRequest();
}

// static
policy::DMToken DeepScanningDialogDelegate::GetDMToken() {
  policy::DMToken dm_token = *GetDMTokenForTestingStorage();

#if !defined(OS_CHROMEOS)
  // This is not compiled on chromeos because
  // ChromeBrowserCloudManagementController does not exist.  Also,
  // policy::BrowserDMTokenStorage::Get()->RetrieveDMToken() does not return a
  // valid token either.  Once these are fixed the #if !defined can be removed.

  if (dm_token.is_empty() &&
      policy::ChromeBrowserCloudManagementController::IsEnabled()) {
    dm_token = policy::BrowserDMTokenStorage::Get()->RetrieveBrowserDMToken();
  }
#endif

  return dm_token;
}

bool DeepScanningDialogDelegate::UploadData() {
  if (data_.do_dlp_scan) {
    // Create a string data source based on all the text.
    std::string full_text;
    for (const auto& text : data_.text)
      full_text.append(base::UTF16ToUTF8(text));

    text_request_complete_ = full_text.empty();
    if (!text_request_complete_) {
      auto request = std::make_unique<StringSourceRequest>(
          std::move(full_text),
          base::BindOnce(&DeepScanningDialogDelegate::StringRequestCallback,
                         weak_ptr_factory_.GetWeakPtr()));

      PrepareRequest(DlpDeepScanningClientRequest::WEB_CONTENT_UPLOAD,
                     request.get());
      UploadTextForDeepScanning(std::move(request));
    }
  } else {
    // Text data sent only for content compliance.
    text_request_complete_ = true;
  }

  // Create a file request for each file.
  for (size_t i = 0; i < data_.paths.size(); ++i) {
    if (FileTypeSupported(data_.do_malware_scan, data_.do_dlp_scan,
                          data_.paths[i])) {
      auto request = std::make_unique<FileSourceRequest>(
          weak_ptr_factory_.GetWeakPtr(), data_.paths[i],
          base::BindOnce(&DeepScanningDialogDelegate::FileRequestCallback,
                         weak_ptr_factory_.GetWeakPtr(), data_.paths[i]));

      PrepareRequest(DlpDeepScanningClientRequest::FILE_UPLOAD, request.get());
      UploadFileForDeepScanning(data_.paths[i], std::move(request));
    } else {
      ++file_result_count_;
      result_.paths_results[i] = true;
    }
  }

  return !text_request_complete_ || file_result_count_ != data_.paths.size();
}

void DeepScanningDialogDelegate::PrepareRequest(
    DlpDeepScanningClientRequest::ContentSource trigger,
    BinaryUploadService::Request* request) {
  if (data_.do_dlp_scan) {
    DlpDeepScanningClientRequest dlp_request;
    dlp_request.set_content_source(trigger);
    request->set_request_dlp_scan(std::move(dlp_request));
  }

  if (data_.do_malware_scan) {
    MalwareDeepScanningClientRequest malware_request;
    malware_request.set_population(
        MalwareDeepScanningClientRequest::POPULATION_ENTERPRISE);
    request->set_request_malware_scan(std::move(malware_request));
  }

  request->set_dm_token(GetDMToken().value());
}

void DeepScanningDialogDelegate::FillAllResultsWith(bool status) {
  std::fill(result_.text_results.begin(), result_.text_results.end(), status);
  std::fill(result_.paths_results.begin(), result_.paths_results.end(), status);
}

void DeepScanningDialogDelegate::UploadTextForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_EQ(
      DlpDeepScanningClientRequest::WEB_CONTENT_UPLOAD,
      request->deep_scanning_request().dlp_scan_request().content_source());
  BinaryUploadService* upload_service =
      g_browser_process->safe_browsing_service()->GetBinaryUploadService(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

void DeepScanningDialogDelegate::UploadFileForDeepScanning(
    const base::FilePath& path,
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_EQ(
      DlpDeepScanningClientRequest::FILE_UPLOAD,
      request->deep_scanning_request().dlp_scan_request().content_source());
  BinaryUploadService* upload_service =
      g_browser_process->safe_browsing_service()->GetBinaryUploadService(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

bool DeepScanningDialogDelegate::CloseTabModalDialog() {
  if (!dialog_)
    return false;

  dialog_->CancelTabModalDialog();
  return true;
}

void DeepScanningDialogDelegate::MaybeCompleteScanRequest() {
  if (!text_request_complete_ || file_result_count_ < data_.paths.size())
    return;

  RunCallback();

  if (!CloseTabModalDialog()) {
    // No UI was shown.  Delete |this| to cleanup.
    delete this;
  }
}

void DeepScanningDialogDelegate::RunCallback() {
  if (!callback_.is_null())
    std::move(callback_).Run(data_, result_);
}

void DeepScanningDialogDelegate::SetFileInfo(const base::FilePath& path,
                                             std::string sha256,
                                             int64_t size) {
  auto it = std::find(data_.paths.begin(), data_.paths.end(), path);
  DCHECK(it != data_.paths.end());
  size_t index = std::distance(data_.paths.begin(), it);
  file_info_[index].sha256 = std::move(sha256);
  file_info_[index].size = size;
}

}  // namespace safe_browsing
