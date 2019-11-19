// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_fetcher_win.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "components/version_info/version_info.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

constexpr char kDownloadStatusErrorCodeHistogramName[] =
    "SoftwareReporter.Cleaner.DownloadStatusErrorCode";

// Indicates the suffix to use for some histograms that depend on the final
// download status. This is used because UMA histogram macros define static
// constant strings to represent the name, so they can't be used when a name
// is dynamically generated. The alternative would be to replicate the logic
// of those macros, which is not ideal.
enum class FetchCompletedReasonHistogramSuffix {
  kDownloadSuccess,
  kDownloadFailure,
  kNetworkError,
};

base::FilePath::StringType CleanerTempDirectoryPrefix() {
  // Create a temporary directory name prefix like "ChromeCleaner_4_", where
  // "Chrome" is the product name and the 4 refers to the install mode of the
  // browser.
  int install_mode = install_static::InstallDetails::Get().install_mode_index();
  return base::StringPrintf(
      FILE_PATH_LITERAL("%" PRFilePath "%" PRFilePath "_%d_"),
      install_static::kProductPathName, FILE_PATH_LITERAL("Cleaner"),
      install_mode);
}

// These values are used to send UMA information and are replicated in the
// histograms.xml file, so the order MUST NOT CHANGE.
enum CleanerDownloadStatusHistogramValue {
  CLEANER_DOWNLOAD_STATUS_SUCCEEDED = 0,
  CLEANER_DOWNLOAD_STATUS_OTHER_FAILURE = 1,
  CLEANER_DOWNLOAD_STATUS_NOT_FOUND_ON_SERVER = 2,
  CLEANER_DOWNLOAD_STATUS_FAILED_TO_CREATE_TEMP_DIR = 3,
  CLEANER_DOWNLOAD_STATUS_FAILED_TO_SAVE_TO_FILE = 4,

  CLEANER_DOWNLOAD_STATUS_MAX,
};

net::NetworkTrafficAnnotationTag kChromeCleanerTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chrome_cleaner", R"(
      semantics {
        sender: "Chrome Cleaner"
        description:
          "Chrome Cleaner removes unwanted software that violates Google's "
          "unwanted software policy."
        trigger:
          "Chrome reporter detected unwanted software that the Cleaner can "
          "remove."
        data: "No data is sent up, this is just a download."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled in settings."
        policy_exception_justification: "Not implemented."
  })");

void RecordCleanerDownloadStatusHistogram(
    CleanerDownloadStatusHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.Cleaner.DownloadStatus", value,
                            CLEANER_DOWNLOAD_STATUS_MAX);
}

// Class that will attempt to download the Chrome Cleaner executable and call a
// given callback when done. Instances of ChromeCleanerFetcher own themselves
// and will self-delete if they encounter an error or when the network request
// has completed.
class ChromeCleanerFetcher {
 public:
  ChromeCleanerFetcher(ChromeCleanerFetchedCallback fetched_callback,
                       network::mojom::URLLoaderFactory* url_loader_factory);

 private:
  // Must be called on a sequence where IO is allowed.
  bool CreateTemporaryDirectory();
  // Will be called back on the same sequence as this object was created on.
  void OnTemporaryDirectoryCreated(bool success);
  void PostCallbackAndDeleteSelf(base::FilePath path,
                                 ChromeCleanerFetchStatus fetch_status);

  // Sends a histogram indicating an error and invokes the fetch callback if
  // the cleaner binary can't be downloaded or saved to the disk.
  void RecordDownloadStatusAndPostCallback(
      CleanerDownloadStatusHistogramValue histogram_value,
      ChromeCleanerFetchStatus fetch_status);

  void RecordTimeToCompleteDownload(FetchCompletedReasonHistogramSuffix suffix,
                                    base::TimeDelta download_duration);

  void OnDownloadedToFile(base::Time start_time, base::FilePath path);

  ChromeCleanerFetchedCallback fetched_callback_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  network::mojom::URLLoaderFactory* url_loader_factory_;

  // Used for file operations such as creating a new temporary directory.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // We will take ownership of the scoped temp directory once we know that the
  // resource load has succeeded. Must be deleted on a sequence where IO is
  // allowed.
  std::unique_ptr<base::ScopedTempDir, base::OnTaskRunnerDeleter>
      scoped_temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerFetcher);
};

ChromeCleanerFetcher::ChromeCleanerFetcher(
    ChromeCleanerFetchedCallback fetched_callback,
    network::mojom::URLLoaderFactory* url_loader_factory)
    : fetched_callback_(std::move(fetched_callback)),
      url_loader_factory_(url_loader_factory),
      blocking_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      scoped_temp_dir_(new base::ScopedTempDir(),
                       base::OnTaskRunnerDeleter(blocking_task_runner_)) {
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&ChromeCleanerFetcher::CreateTemporaryDirectory,
                 base::Unretained(this)),
      base::Bind(&ChromeCleanerFetcher::OnTemporaryDirectoryCreated,
                 base::Unretained(this)));
}

bool ChromeCleanerFetcher::CreateTemporaryDirectory() {
  base::FilePath temp_dir;
  return base::CreateNewTempDirectory(CleanerTempDirectoryPrefix(),
                                      &temp_dir) &&
         scoped_temp_dir_->Set(temp_dir);
}

void ChromeCleanerFetcher::OnTemporaryDirectoryCreated(bool success) {
  if (!success) {
    RecordCleanerDownloadStatusHistogram(
        CLEANER_DOWNLOAD_STATUS_FAILED_TO_CREATE_TEMP_DIR);
    PostCallbackAndDeleteSelf(
        base::FilePath(),
        ChromeCleanerFetchStatus::kFailedToCreateTemporaryDirectory);
    return;
  }

  DCHECK(!scoped_temp_dir_->GetPath().empty());

  base::FilePath temp_file = scoped_temp_dir_->GetPath().Append(
      base::ASCIIToUTF16(base::GenerateGUID()) + L".tmp");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GetSRTDownloadURL();
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(request), kChromeCleanerTrafficAnnotation);
  url_loader_->SetAllowHttpErrorResults(/*allow=*/true);
  url_loader_->SetRetryOptions(3, network::SimpleURLLoader::RETRY_ON_5XX);

  url_loader_->DownloadToFile(
      url_loader_factory_,
      base::BindOnce(&ChromeCleanerFetcher::OnDownloadedToFile,
                     base::Unretained(this), base::Time::Now()),
      temp_file);
}

void ChromeCleanerFetcher::PostCallbackAndDeleteSelf(
    base::FilePath path,
    ChromeCleanerFetchStatus fetch_status) {
  DCHECK(fetched_callback_);

  std::move(fetched_callback_).Run(std::move(path), fetch_status);

  // At this point, the url_fetcher_ is gone and this ChromeCleanerFetcher
  // instance is no longer needed.
  delete this;
}

void ChromeCleanerFetcher::OnDownloadedToFile(base::Time start_time,
                                              base::FilePath path) {
  const base::TimeDelta download_duration = base::Time::Now() - start_time;

  if (url_loader_->NetError() != net::OK) {
    base::UmaHistogramSparse(kDownloadStatusErrorCodeHistogramName,
                             url_loader_->NetError());
    RecordTimeToCompleteDownload(
        FetchCompletedReasonHistogramSuffix::kNetworkError, download_duration);
    RecordDownloadStatusAndPostCallback(
        CLEANER_DOWNLOAD_STATUS_OTHER_FAILURE,
        ChromeCleanerFetchStatus::kOtherFailure);
    return;
  }

  int response_code = 0;
  if (url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  base::UmaHistogramSparse(kDownloadStatusErrorCodeHistogramName,
                           response_code);
  const FetchCompletedReasonHistogramSuffix suffix =
      response_code == net::HTTP_OK
          ? FetchCompletedReasonHistogramSuffix::kDownloadSuccess
          : FetchCompletedReasonHistogramSuffix::kDownloadFailure;
  RecordTimeToCompleteDownload(suffix, download_duration);

  if (response_code == net::HTTP_NOT_FOUND) {
    RecordDownloadStatusAndPostCallback(
        CLEANER_DOWNLOAD_STATUS_NOT_FOUND_ON_SERVER,
        ChromeCleanerFetchStatus::kNotFoundOnServer);
    return;
  }

  if (response_code != net::HTTP_OK) {
    RecordDownloadStatusAndPostCallback(
        CLEANER_DOWNLOAD_STATUS_OTHER_FAILURE,
        ChromeCleanerFetchStatus::kOtherFailure);
    return;
  }

  // Take ownership of the scoped temp directory so it is not deleted.
  scoped_temp_dir_->Take();

  RecordCleanerDownloadStatusHistogram(CLEANER_DOWNLOAD_STATUS_SUCCEEDED);
  PostCallbackAndDeleteSelf(std::move(path),
                            ChromeCleanerFetchStatus::kSuccess);
}

void ChromeCleanerFetcher::RecordDownloadStatusAndPostCallback(
    CleanerDownloadStatusHistogramValue histogram_value,
    ChromeCleanerFetchStatus fetch_status) {
  RecordCleanerDownloadStatusHistogram(histogram_value);
  PostCallbackAndDeleteSelf(base::FilePath(), fetch_status);
}

void ChromeCleanerFetcher::RecordTimeToCompleteDownload(
    FetchCompletedReasonHistogramSuffix suffix,
    base::TimeDelta download_duration) {
  switch (suffix) {
    case FetchCompletedReasonHistogramSuffix::kDownloadFailure:
      UMA_HISTOGRAM_LONG_TIMES_100(
          "SoftwareReporter.Cleaner.TimeToCompleteDownload_DownloadFailure",
          download_duration);
      break;

    case FetchCompletedReasonHistogramSuffix::kDownloadSuccess:
      UMA_HISTOGRAM_LONG_TIMES_100(
          "SoftwareReporter.Cleaner.TimeToCompleteDownload_DownloadSuccess",
          download_duration);
      break;

    case FetchCompletedReasonHistogramSuffix::kNetworkError:
      UMA_HISTOGRAM_LONG_TIMES_100(
          "SoftwareReporter.Cleaner.TimeToCompleteDownload_NetworkError",
          download_duration);
      break;
  }
}

}  // namespace

void FetchChromeCleaner(ChromeCleanerFetchedCallback fetched_callback,
                        network::mojom::URLLoaderFactory* url_loader_factory) {
  new ChromeCleanerFetcher(std::move(fetched_callback), url_loader_factory);
}

}  // namespace safe_browsing
