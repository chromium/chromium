// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"

#include <numeric>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"
#include "chrome/browser/enterprise/connectors/analysis/source_destination_matcher_ash.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/recursive_operation_delegate.h"

using safe_browsing::BinaryUploadService;

namespace {

enterprise_connectors::FileTransferAnalysisDelegate::
    FileTransferAnalysisDelegateFactory&
    GetFactoryStorage() {
  static base::NoDestructor<
      enterprise_connectors::FileTransferAnalysisDelegate::
          FileTransferAnalysisDelegateFactory>
      factory;
  return *factory;
}

// GetFileURLsDelegate is used to get the `FileSystemURL`s of all files lying
// within `root`. A vector of these urls is passed to `callback`. If `root` is
// a file, the vector will only contain `root`. If `root` is a directory all
// files lying in that directory or any descended subdirectory are passed to
// `callback`.
class GetFileURLsDelegate final : public storage::RecursiveOperationDelegate {
 public:
  using FileURLsCallback =
      base::OnceCallback<void(std::vector<storage::FileSystemURL>)>;

  GetFileURLsDelegate(storage::FileSystemContext* file_system_context,
                      const storage::FileSystemURL& root,
                      FileURLsCallback callback)
      : RecursiveOperationDelegate(file_system_context),
        root_(root),
        callback_(std::move(callback)) {}

  GetFileURLsDelegate(const GetFileURLsDelegate&) = delete;
  GetFileURLsDelegate& operator=(const GetFileURLsDelegate&) = delete;

  ~GetFileURLsDelegate() override = default;

  // RecursiveOperationDelegate:
  void Run() override { NOTREACHED_IN_MIGRATION(); }
  void RunRecursively() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    StartRecursiveOperation(root_,
                            storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
                            base::BindOnce(&GetFileURLsDelegate::Completed,
                                           weak_ptr_factory_.GetWeakPtr()));
  }
  void ProcessFile(const storage::FileSystemURL& url,
                   StatusCallback callback) override {
    if (error_url_.is_valid() && error_url_ == url) {
      std::move(callback).Run(base::File::FILE_ERROR_FAILED);
      return;
    }

    file_system_context()->operation_runner()->GetMetadata(
        url, {storage::FileSystemOperation::GetMetadataField::kIsDirectory},
        base::BindOnce(&GetFileURLsDelegate::OnGetMetadata,
                       weak_ptr_factory_.GetWeakPtr(), url,
                       std::move(callback)));
  }
  void ProcessDirectory(const storage::FileSystemURL& url,
                        StatusCallback callback) override {
    std::move(callback).Run(base::File::FILE_OK);
  }
  void PostProcessDirectory(const storage::FileSystemURL& url,
                            StatusCallback callback) override {
    std::move(callback).Run(base::File::FILE_OK);
  }
  base::WeakPtr<storage::RecursiveOperationDelegate> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnGetMetadata(const storage::FileSystemURL& url,
                     StatusCallback callback,
                     base::File::Error result,
                     const base::File::Info& file_info) {
    if (result != base::File::FILE_OK) {
      std::move(callback).Run(result);
      return;
    }
    if (file_info.is_directory) {
      std::move(callback).Run(base::File::FILE_ERROR_NOT_A_FILE);
      return;
    }

    file_urls_.push_back(url);
    std::move(callback).Run(base::File::FILE_OK);
  }

  void Completed(base::File::Error result) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(file_urls_)));
  }

  storage::FileSystemURL root_;
  FileURLsCallback callback_;
  storage::FileSystemURL error_url_;
  std::vector<storage::FileSystemURL> file_urls_;

  base::WeakPtrFactory<GetFileURLsDelegate> weak_ptr_factory_{this};
};

bool IsInSameFileSystem(Profile* profile,
                        storage::FileSystemURL source_url,
                        storage::FileSystemURL destination_url) {
  // Cheap check: source file system url.
  if (!source_url.IsInSameFileSystem(destination_url)) {
    return false;
  }

  // For some URLs FileSystemURL's IsInSameFileSystem function returns false
  // positives. Which `volume_manager` is able to properly determine.
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile);
  base::WeakPtr<file_manager::Volume> source_volume =
      volume_manager->FindVolumeFromPath(source_url.path());
  base::WeakPtr<file_manager::Volume> destination_volume =
      volume_manager->FindVolumeFromPath(destination_url.path());

  if (!source_volume || !destination_volume) {
    // The source or destination volume don't exist, so we trust the
    // FileSystemURL response, i.e., they lie in the same file system.
    return true;
  }
  // If both volumes exist, we check whether their ID is the same.
  return source_volume->volume_id() == destination_volume->volume_id();
}

}  // namespace

namespace enterprise_connectors {

// static
FileTransferAnalysisDelegate::FileTransferAnalysisResult
FileTransferAnalysisDelegate::FileTransferAnalysisResult::Allowed() {
  return FileTransferAnalysisResult(
      Verdict::ALLOWED, /*final_result=*/std::nullopt, /*tag=*/std::string());
}

// static
FileTransferAnalysisDelegate::FileTransferAnalysisResult
FileTransferAnalysisDelegate::FileTransferAnalysisResult::Blocked(
    FinalContentAnalysisResult final_result,
    const std::string& tag) {
  return FileTransferAnalysisResult(Verdict::BLOCKED, final_result, tag);
}

// static
FileTransferAnalysisDelegate::FileTransferAnalysisResult
FileTransferAnalysisDelegate::FileTransferAnalysisResult::Unknown() {
  return FileTransferAnalysisResult(
      Verdict::UNKNOWN, /*final_result=*/std::nullopt, /*tag=*/std::string());
}

const std::string&
FileTransferAnalysisDelegate::FileTransferAnalysisResult::tag() const {
  return tag_;
}

const std::optional<FinalContentAnalysisResult>
FileTransferAnalysisDelegate::FileTransferAnalysisResult::final_result() const {
  return final_result_;
}

FileTransferAnalysisDelegate::FileTransferAnalysisResult::
    FileTransferAnalysisResult(
        Verdict verdict,
        std::optional<FinalContentAnalysisResult> final_result,
        const std::string& tag)
    : verdict_(verdict), final_result_(final_result), tag_(tag) {}

FileTransferAnalysisDelegate::FileTransferAnalysisResult::
    ~FileTransferAnalysisResult() = default;

FileTransferAnalysisDelegate::FileTransferAnalysisResult::
    FileTransferAnalysisResult(const FileTransferAnalysisResult& other) =
        default;

FileTransferAnalysisDelegate::FileTransferAnalysisResult&
FileTransferAnalysisDelegate::FileTransferAnalysisResult::operator=(
    FileTransferAnalysisResult&& other) = default;

bool FileTransferAnalysisDelegate::FileTransferAnalysisResult::IsAllowed()
    const {
  return verdict_ == Verdict::ALLOWED;
}
bool FileTransferAnalysisDelegate::FileTransferAnalysisResult::IsBlocked()
    const {
  return verdict_ == Verdict::BLOCKED;
}
bool FileTransferAnalysisDelegate::FileTransferAnalysisResult::IsUnknown()
    const {
  return verdict_ == Verdict::UNKNOWN;
}

// static
std::unique_ptr<FileTransferAnalysisDelegate>
FileTransferAnalysisDelegate::Create(
    safe_browsing::DeepScanAccessPoint access_point,
    storage::FileSystemURL source_url,
    storage::FileSystemURL destination_url,
    Profile* profile,
    storage::FileSystemContext* file_system_context,
    AnalysisSettings settings) {
  if (GetFactoryStorage().is_null()) {
    // This code path is always reached outside of tests.
    return base::WrapUnique(
        new enterprise_connectors::FileTransferAnalysisDelegate(
            access_point, source_url, destination_url, profile,
            file_system_context, std::move(settings)));
  } else {
    // Only in tests, GetFactoryStorage() can be set and this code path can be
    // reached.
    // Pass `idx` in addition to the constructor parameters to make testing
    // easier.
    return GetFactoryStorage().Run(access_point, source_url, destination_url,
                                   profile, file_system_context,
                                   std::move(settings));
  }
}

// static
void FileTransferAnalysisDelegate::SetFactorForTesting(
    FileTransferAnalysisDelegateFactory factory) {
  GetFactoryStorage() = factory;
}

// static
std::vector<std::optional<AnalysisSettings>>
FileTransferAnalysisDelegate::IsEnabledVec(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& source_urls,
    storage::FileSystemURL destination_url) {
  DCHECK(profile);
  auto* service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  // If the corresponding Connector policy isn't set, don't perform scans.
  if (!service ||
      !service->IsConnectorEnabled(enterprise_connectors::FILE_TRANSFER)) {
    // Return an empty vector.
    return {};
  }

  std::vector<std::optional<AnalysisSettings>> settings(source_urls.size());

  bool at_least_one_enabled = false;
  for (size_t i = 0; i < source_urls.size(); ++i) {
    if (IsInSameFileSystem(profile, source_urls[i], destination_url)) {
      // Scanning is disabled for transfers on the same file system.
      continue;
    }

    settings[i] = service->GetAnalysisSettings(
        source_urls[i], destination_url, enterprise_connectors::FILE_TRANSFER);
    at_least_one_enabled |= settings[i].has_value();
  }
  if (!at_least_one_enabled) {
    // Return an empty vector.
    return {};
  }

  return settings;
}

FileTransferAnalysisDelegate::FileTransferAnalysisResult
FileTransferAnalysisDelegate::GetAnalysisResultAfterScan(
    storage::FileSystemURL url) {
  // Should only be called for blocking scans.
  DCHECK_EQ(settings_.block_until_verdict, BlockUntilVerdict::kBlock);
  DCHECK_EQ(results_.size(), scanning_urls_.size());

  for (size_t i = 0; i < scanning_urls_.size(); ++i) {
    if (scanning_urls_[i] == url) {
      if (results_[i].complies ||
          (warning_is_bypassed_ &&
           results_[i].final_result == FinalContentAnalysisResult::WARNING)) {
        return FileTransferAnalysisResult::Allowed();
      }
      return FileTransferAnalysisResult::Blocked(results_[i].final_result,
                                                 results_[i].tag);
    }
  }
  return FileTransferAnalysisResult::Unknown();
}

std::vector<storage::FileSystemURL>
FileTransferAnalysisDelegate::GetWarnedFiles() const {
  // Should only be called for blocking scans.
  DCHECK_EQ(settings_.block_until_verdict, BlockUntilVerdict::kBlock);
  DCHECK_EQ(results_.size(), scanning_urls_.size());

  std::vector<storage::FileSystemURL> warned_files;
  for (size_t i = 0; i < scanning_urls_.size(); ++i) {
    if (!results_[i].complies &&
        results_[i].final_result == FinalContentAnalysisResult::WARNING) {
      warned_files.push_back(scanning_urls_[i]);
    }
  }
  return warned_files;
}

void FileTransferAnalysisDelegate::UploadData(
    base::OnceClosure completion_callback) {
  callback_ = std::move(completion_callback);
  DCHECK(!callback_.is_null());

  // This will start aggregating the needed file urls and pass them to
  // OnGotFileSourceURLs.
  // The usage of the WeakPtr is only safe if `get_file_urls_delegate_` is
  // deleted on the IOThread.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&storage::RecursiveOperationDelegate::RunRecursively,
                     get_file_urls_delegate_->AsWeakPtr()));
}

FileTransferAnalysisDelegate::FileTransferAnalysisDelegate(
    safe_browsing::DeepScanAccessPoint access_point,
    storage::FileSystemURL source_url,
    storage::FileSystemURL destination_url,
    Profile* profile,
    storage::FileSystemContext* file_system_context,
    AnalysisSettings settings)
    : settings_{std::move(settings)},
      profile_{profile},
      access_point_{access_point},
      source_url_(std::move(source_url)),
      destination_url_{std::move(destination_url)} {
  DCHECK(profile);

  // For blocking scans, scanning is performed before the copy/move and
  // thus scanning should be performed on the source.
  // For non-blocking report-only scans, scanning is performed after the
  // copy/move and thus scanning should be performed on the destination.
  auto scanning_url = settings_.block_until_verdict == BlockUntilVerdict::kBlock
                          ? source_url_
                          : destination_url_;

  get_file_urls_delegate_ = std::make_unique<GetFileURLsDelegate>(
      file_system_context, scanning_url,
      base::BindOnce(&FileTransferAnalysisDelegate::OnGotFileURLs,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FileTransferAnalysisDelegate::BypassWarnings(
    std::optional<std::u16string> user_justification) {
  if (!warned_file_indices_.empty()) {
    request_handler_->ReportWarningBypass(user_justification);
    warning_is_bypassed_ = true;
  }
}

void FileTransferAnalysisDelegate::Cancel(bool warning) {
  // TODO(crbug.com/1340313)
}

std::optional<std::u16string> FileTransferAnalysisDelegate::GetCustomMessage(
    const std::string& tag) const {
  auto it = settings_.tags.find(tag);
  if (it == settings_.tags.end()) {
    return std::nullopt;
  }
  const std::u16string& message = it->second.custom_message.message;
  if (message.empty()) {
    return std::nullopt;
  }
  return message;
}

std::optional<GURL> FileTransferAnalysisDelegate::GetCustomLearnMoreUrl(
    const std::string& tag) const {
  auto it = settings_.tags.find(tag);
  if (it == settings_.tags.end()) {
    return std::nullopt;
  }
  const GURL& learn_more_url = it->second.custom_message.learn_more_url;
  if (!learn_more_url.is_valid()) {
    return std::nullopt;
  }
  return learn_more_url;
}

bool FileTransferAnalysisDelegate::BypassRequiresJustification(
    const std::string& tag) const {
  auto it = settings_.tags.find(tag);
  if (it == settings_.tags.end()) {
    return false;
  }
  return it->second.requires_justification;
}

FilesRequestHandler*
FileTransferAnalysisDelegate::GetFilesRequestHandlerForTesting() {
  return request_handler_.get();
}

void FileTransferAnalysisDelegate::OnGotFileURLs(
    std::vector<storage::FileSystemURL> scanning_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scanning_urls_ = std::move(scanning_urls);
  if (scanning_urls_.empty()) {
    ContentAnalysisCompleted(std::vector<RequestHandlerResult>());
    return;
  }

  std::vector<base::FilePath> paths;
  for (const storage::FileSystemURL& url : scanning_urls_) {
    paths.push_back(url.path());
  }

  request_handler_ = FilesRequestHandler::Create(
      safe_browsing::BinaryUploadService::GetForProfile(profile_, settings_),
      profile_, settings_, GURL{},
      SourceDestinationMatcherAsh::GetVolumeDescriptionFromPath(
          profile_, source_url_.path()),
      SourceDestinationMatcherAsh::GetVolumeDescriptionFromPath(
          profile_, destination_url_.path()),
      // User action id and tab title are only needed for local content
      // analysis, leave them empty here.
      /*user_action_id=*/std::string(), /*tab_title=*/std::string(),
      /*content_transfer_method=*/std::string(), access_point_,
      ContentAnalysisRequest::UNKNOWN, std::move(paths),
      base::BindOnce(&FileTransferAnalysisDelegate::ContentAnalysisCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  request_handler_->UploadData();
}

FileTransferAnalysisDelegate::~FileTransferAnalysisDelegate() {
  if (get_file_urls_delegate_) {
    // To ensure that there are no race conditions, we post the deletion of
    // `get_file_urls_delegate_` to the IO thread.
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<storage::RecursiveOperationDelegate> delegate) {
              // Do nothing.
              // At the end of this task `get_file_urls_delegate_`
              // will be deleted.
            },
            std::move(get_file_urls_delegate_)));
  }
}

void FileTransferAnalysisDelegate::ContentAnalysisCompleted(
    std::vector<RequestHandlerResult> results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  results_ = std::move(results);

  // Don't show warning here, as we use multiple FileTransferAnalysisDelegate's
  // and only want to show one warning.
  for (size_t index = 0; index < results_.size(); ++index) {
    FinalContentAnalysisResult result = results_[index].final_result;
    if (result == FinalContentAnalysisResult::WARNING) {
      warned_file_indices_.push_back(index);
    }
  }

  DCHECK(!callback_.is_null());
  std::move(callback_).Run();
}

}  // namespace enterprise_connectors
