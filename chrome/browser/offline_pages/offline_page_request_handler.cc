// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_request_handler.h"

#include <utility>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/file_stream.h"
#include "net/base/filename_util.h"
#include "net/base/io_buffer.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_redirect_job.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {

// This enum is used to tell all possible outcomes of handling network requests
// that might serve offline contents.
enum class RequestResult {
  // Offline page was shown for current URL.
  OFFLINE_PAGE_SERVED,
  // Redirected from original URL to final URL in preparation to show the
  // offline page under final URL. OFFLINE_PAGE_SERVED is most likely to be
  // reported next if no other error is encountered.
  REDIRECTED,
  // Tab was gone.
  NO_TAB_ID,
  // Web contents was gone.
  NO_WEB_CONTENTS,
  // The offline page found was not fresh enough, i.e. not created in the past
  // day. This only applies in prohibitively slow network.
  PAGE_NOT_FRESH,
  // Offline page was not found, by searching with either final URL or original
  // URL.
  OFFLINE_PAGE_NOT_FOUND,
  // Digest for the archive file does not match with the one saved in the
  // metadata database.
  DIGEST_MISMATCH,
  // The archive file does not exist.
  FILE_NOT_FOUND,
};

// Consistent with the buffer size used in url request data reading.
const size_t kMaxBufferSizeForValidation = 4096;

void GetFileSize(const base::FilePath& file_path, int64_t* file_size) {
  bool succeeded = base::GetFileSize(file_path, file_size);
  if (!succeeded) {
    // Use -1 to indicate that file is not found.
    *file_size = -1;
  }
}

void UpdateDigest(
    const scoped_refptr<OfflinePageRequestHandler::ThreadSafeArchiveValidator>&
        validator,
    scoped_refptr<net::IOBuffer> buffer,
    size_t len) {
  validator->Update(buffer->data(), len);
}

OfflinePageModel* GetOfflinePageModel(
    content::WebContents::Getter web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = web_contents_getter.Run();
  return web_contents ? OfflinePageModelFactory::GetForBrowserContext(
                            web_contents->GetBrowserContext())
                      : nullptr;
}

// Notifies OfflinePageRequestHandler about all the matched offline pages.
void NotifyAvailableOfflinePagesOnUI(
    base::WeakPtr<OfflinePageRequestHandler> job,
    const std::vector<OfflinePageRequestHandler::Candidate>& candidates) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (job)
    job->OnOfflinePagesAvailable(candidates);
}

// Failed to find an offline page.
void FailedToFindOfflinePage(
    RequestResult request_error_result,
    OfflinePageRequestHandler::NetworkState network_state,
    base::WeakPtr<OfflinePageRequestHandler> job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(RequestResult::OFFLINE_PAGE_SERVED, request_error_result);

  NotifyAvailableOfflinePagesOnUI(
      job, std::vector<OfflinePageRequestHandler::Candidate>());
}

// Handles the result of finding matched offline pages.
void SelectPagesForURLDone(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    OfflinePageRequestHandler::NetworkState network_state,
    base::WeakPtr<OfflinePageRequestHandler> job,
    content::WebContents::Getter web_contents_getter,
    const std::vector<OfflinePageItem>& offline_pages) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Bail out if no page is found.
  if (offline_pages.empty()) {
    FailedToFindOfflinePage(RequestResult::OFFLINE_PAGE_NOT_FOUND,
                            network_state, job);
    return;
  }

  // Bail out if web_contents is gone.
  OfflinePageModel* offline_page_model =
      GetOfflinePageModel(web_contents_getter);
  if (!offline_page_model) {
    FailedToFindOfflinePage(RequestResult::NO_WEB_CONTENTS, network_state, job);
    return;
  }

  std::vector<OfflinePageRequestHandler::Candidate> candidates;
  for (const auto& offline_page : offline_pages) {
    OfflinePageRequestHandler::Candidate candidate;
    candidate.offline_page = offline_page;
    candidate.archive_is_in_internal_dir =
        offline_page_model->IsArchiveInInternalDir(offline_page.file_path);
    candidates.push_back(candidate);
  }

  NotifyAvailableOfflinePagesOnUI(job, candidates);
}

// Handles the result of finding an offline page with the requested offline ID.
void GetPageByOfflineIdDone(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    OfflinePageRequestHandler::NetworkState network_state,
    content::WebContents::Getter web_contents_getter,
    base::WeakPtr<OfflinePageRequestHandler> job,
    const OfflinePageItem* offline_page) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the found offline page does not match the request URL, fail.
  if (!offline_page || offline_page->url != url) {
    FailedToFindOfflinePage(RequestResult::OFFLINE_PAGE_NOT_FOUND,
                            network_state, job);
    return;
  }

  std::vector<OfflinePageItem> offline_pages;
  offline_pages.push_back(*offline_page);
  SelectPagesForURLDone(url, offline_header, network_state, job,
                        web_contents_getter, offline_pages);
}

// Tries to find all the offline pages to serve for |url|.
void GetPagesToServeURL(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    OfflinePageRequestHandler::NetworkState network_state,
    content::WebContents::Getter web_contents_getter,
    OfflinePageRequestHandler::Delegate::TabIdGetter tab_id_getter,
    base::WeakPtr<OfflinePageRequestHandler> job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents) {
    FailedToFindOfflinePage(RequestResult::NO_WEB_CONTENTS, network_state, job);
    return;
  }
  int tab_id;
  if (!tab_id_getter.Run(web_contents, &tab_id)) {
    FailedToFindOfflinePage(RequestResult::NO_TAB_ID, network_state, job);
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  SimpleFactoryKey* key = profile->GetProfileKey();

  // If an int64 offline ID is present in the offline header, try to load that
  // particular version.
  if (!offline_header.id.empty()) {
    int64_t offline_id;
    if (base::StringToInt64(offline_header.id, &offline_id)) {
      OfflinePageModel* offline_page_model =
          OfflinePageModelFactory::GetForKey(key);
      if (!offline_page_model) {
        FailedToFindOfflinePage(RequestResult::OFFLINE_PAGE_NOT_FOUND,
                                network_state, job);
        return;
      }
      offline_page_model->GetPageByOfflineId(
          offline_id,
          base::BindOnce(&GetPageByOfflineIdDone, url, offline_header,
                         network_state, web_contents_getter, job));
      return;
    }
  }

  OfflinePageUtils::SelectPagesForURL(
      key, url, tab_id,
      base::BindOnce(&SelectPagesForURLDone, url, offline_header, network_state,
                     job, web_contents_getter));
}

// Do all the things needed to be done on UI thread after a trusted offline
// page has been visited.
void VisitTrustedOfflinePageOnUI(
    const OfflinePageHeader& offline_header,
    OfflinePageRequestHandler::NetworkState network_state,
    content::WebContents::Getter web_contents_getter,
    const OfflinePageItem& offline_page,
    bool archive_is_in_internal_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |web_contents_getter| is passed from IO thread. We need to check if
  // web contents is still valid.
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!offline_page_model)
    return;

  // Mark the offline page as being accessed.
  offline_page_model->MarkPageAccessed(offline_page.offline_id);

  // Save an cached copy of OfflinePageItem such that Tab code can get
  // the loaded offline page immediately.
  OfflinePageTabHelper* tab_helper =
      OfflinePageTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  tab_helper->SetOfflinePage(
      offline_page, offline_header,
      archive_is_in_internal_dir
          ? OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR
          : OfflinePageTrustedState::TRUSTED_AS_UNMODIFIED_AND_IN_PUBLIC_DIR,
      network_state ==
          OfflinePageRequestHandler::NetworkState::PROHIBITIVELY_SLOW_NETWORK);
}

void ClearOfflinePageData(content::WebContents::Getter web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |web_contents_getter| is passed from IO thread. We need to check if
  // web contents is still valid.
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  // Save an cached copy of OfflinePageItem such that Tab code can get
  // the loaded offline page immediately.
  OfflinePageTabHelper* tab_helper =
      OfflinePageTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  tab_helper->ClearOfflinePage();
}

}  // namespace

OfflinePageRequestHandler::OfflinePageRequestHandler(
    const GURL& url,
    const net::HttpRequestHeaders& extra_request_headers,
    Delegate* delegate)
    : url_(url),
      delegate_(delegate),
      network_state_(NetworkState::CONNECTED_NETWORK),
      candidate_index_(0) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string offline_header_value =
      extra_request_headers.GetHeader(kOfflinePageHeader)
          .value_or(std::string());
  // Note that |offline_header| will be empty if parsing from the header value
  // fails.
  offline_header_ = OfflinePageHeader(offline_header_value);
}

OfflinePageRequestHandler::~OfflinePageRequestHandler() {}

OfflinePageRequestHandler::NetworkState
OfflinePageRequestHandler::GetNetworkState() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (offline_header_.reason == OfflinePageHeader::Reason::NET_ERROR)
    return OfflinePageRequestHandler::NetworkState::FLAKY_NETWORK;

  bool is_offline = net::NetworkChangeNotifier::IsOffline();
  if (offline_pages::IsOfflinePagesNetworkStateLikelyUnknown()) {
    is_offline = false;
  }

  if (is_offline)
    return OfflinePageRequestHandler::NetworkState::DISCONNECTED_NETWORK;

  // If RELOAD is present in the offline header, load the live page.
  if (offline_header_.reason == OfflinePageHeader::Reason::RELOAD)
    return OfflinePageRequestHandler::NetworkState::CONNECTED_NETWORK;

  // If other reason is present in the offline header, force loading the offline
  // page even when the network is connected.
  if (offline_header_.reason != OfflinePageHeader::Reason::NONE) {
    return OfflinePageRequestHandler::NetworkState::
        FORCE_OFFLINE_ON_CONNECTED_NETWORK;
  }

  // Otherwise, the network state is a good network.
  return OfflinePageRequestHandler::NetworkState::CONNECTED_NETWORK;
}

void OfflinePageRequestHandler::Start() {
  network_state_ = GetNetworkState();
  if (network_state_ == NetworkState::CONNECTED_NETWORK) {
    delegate_->FallbackToDefault();
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&OfflinePageRequestHandler::StartAsync,
                                weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestHandler::StartAsync() {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    GetPagesToServeURL(url_, offline_header_, network_state_,
                       delegate_->GetWebContentsGetter(),
                       delegate_->GetTabIdGetter(),
                       weak_ptr_factory_.GetWeakPtr());
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&GetPagesToServeURL, url_, offline_header_,
                       network_state_, delegate_->GetWebContentsGetter(),
                       delegate_->GetTabIdGetter(),
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void OfflinePageRequestHandler::Kill() {
  stream_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool OfflinePageRequestHandler::IsServingOfflinePage() const {
  return candidates_.size() > 0 && candidate_index_ < candidates_.size();
}

scoped_refptr<net::HttpResponseHeaders>
OfflinePageRequestHandler::GetRedirectHeaders() {
  return fake_headers_for_redirect_;
}

int OfflinePageRequestHandler::ReadRawData(net::IOBuffer* dest, int dest_size) {
  DCHECK_NE(dest_size, 0);

  return stream_->Read(
      dest, dest_size,
      base::BindOnce(&OfflinePageRequestHandler::DidReadForServing,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::WrapRefCounted(dest)));
}

void OfflinePageRequestHandler::OnOfflinePagesAvailable(
    const std::vector<Candidate>& candidates) {
  // If no offline page is found, restart the job to fall back to the default
  // handling.
  if (candidates.empty()) {
    delegate_->FallbackToDefault();
    return;
  }

  file_task_runner_ = base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Start the file validation from the 1st offline page.
  candidates_ = candidates;
  candidate_index_ = 0;
  ValidateFile();
}

void OfflinePageRequestHandler::OnTrustedOfflinePageFound() {
  // If the match is for original URL, trigger the redirect.
  // Note: If the offline page has same orginal URL and final URL, don't trigger
  // the redirect. Some websites might route the redirect finally back to itself
  // after intermediate redirects for authentication. Previously this case was
  // not handled and some pages might be saved with same URLs. Though we fixed
  // the problem, we still need to support those pages already saved with this
  if (url_ == GetCurrentOfflinePage().original_url_if_different &&
      url_ != GetCurrentOfflinePage().url) {
    Redirect(GetCurrentOfflinePage().url);
    return;
  }

  // No need to open the file if it has already been opened for the validation.
  if (stream_) {
    DidOpenForServing(net::OK);
    return;
  }

  // If a file:// or content:// intent is being processed, open the file:// or
  // content:// denoted in the intent instead. Otherwise, open the archive file
  // associated with the offline page.
  base::FilePath file_path;
  if (IsProcessingFileUrlIntent()) {
    bool valid = net::FileURLToFilePath(offline_header_.intent_url, &file_path);
    DCHECK(valid);
#if BUILDFLAG(IS_ANDROID)
  } else if (IsProcessingContentUrlIntent()) {
    file_path = base::FilePath(offline_header_.intent_url.spec());
    DCHECK(file_path.IsContentUri());
#endif  // BUILDFLAG(IS_ANDROID)
  } else {
    file_path = GetCurrentOfflinePage().file_path;
  }
  OpenFile(file_path,
           base::BindRepeating(&OfflinePageRequestHandler::DidOpenForServing,
                               weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestHandler::VisitTrustedOfflinePage() {
  delegate_->SetOfflinePageNavigationUIData(true /*is_offline_page*/);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&VisitTrustedOfflinePageOnUI, offline_header_,
                     network_state_, delegate_->GetWebContentsGetter(),
                     GetCurrentOfflinePage(),
                     candidates_[candidate_index_].archive_is_in_internal_dir));
}

void OfflinePageRequestHandler::Redirect(const GURL& redirected_url) {
  std::string header_string = base::StringPrintf(
      "HTTP/1.1 %i Internal Redirect\n"
      // Clear referrer to avoid leak when going online.
      "Referrer-Policy: no-referrer\n"
      "Location: %s\n"
      "Non-Authoritative-Reason: offline redirects",
      // 302 is used to remove response bodies in order to
      // avoid leak when going online.
      static_cast<int>(net::RedirectUtil::ResponseCode::REDIRECT_302_FOUND),
      redirected_url.spec().c_str());

  fake_headers_for_redirect_ = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(header_string));
  DCHECK(fake_headers_for_redirect_->IsRedirect(nullptr));

  delegate_->NotifyHeadersComplete(0);
}

const OfflinePageItem& OfflinePageRequestHandler::GetCurrentOfflinePage()
    const {
  return candidates_[candidate_index_].offline_page;
}

bool OfflinePageRequestHandler::IsProcessingFileUrlIntent() const {
  return offline_header_.reason == OfflinePageHeader::Reason::FILE_URL_INTENT;
}

bool OfflinePageRequestHandler::IsProcessingContentUrlIntent() const {
  return offline_header_.reason ==
         OfflinePageHeader::Reason::CONTENT_URL_INTENT;
}

bool OfflinePageRequestHandler::IsProcessingFileOrContentUrlIntent() const {
  return IsProcessingFileUrlIntent() || IsProcessingContentUrlIntent();
}

void OfflinePageRequestHandler::OpenFile(
    const base::FilePath& file_path,
    const base::RepeatingCallback<void(int)>& callback) {
  if (!stream_)
    stream_ = std::make_unique<net::FileStream>(file_task_runner_);

  int flags =
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC;
#if BUILDFLAG(IS_WIN)
  flags |= base::File::FLAG_WIN_EXCLUSIVE_READ;
#endif  // BUILDFLAG(IS_WIN)
  int result = stream_->Open(file_path, flags, callback);
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

void OfflinePageRequestHandler::UpdateDigestOnBackground(
    scoped_refptr<net::IOBuffer> buffer,
    size_t len,
    base::OnceCallback<void(void)> digest_updated_callback) {
  DCHECK_GT(len, 0u);

  if (!archive_validator_)
    archive_validator_ = new ThreadSafeArchiveValidator();

  // Delegate to background task runner to update the hash since it is time
  // consuming. Once it is done, |digest_updated_callback| will be called to
  // continue the reading.
  file_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&UpdateDigest, archive_validator_, buffer, len),
      std::move(digest_updated_callback));
}

void OfflinePageRequestHandler::FinalizeDigestOnBackground(
    base::OnceCallback<void(const std::string&)> digest_finalized_callback) {
  if (!archive_validator_)
    archive_validator_ = new ThreadSafeArchiveValidator();

  // Delegate to background task runner to finalize the hash to get the digest
  // since it is time consuming. Once it is done, |digest_finalized_callback|
  // will be called with the digest.
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ThreadSafeArchiveValidator::Finish, archive_validator_),
      std::move(digest_finalized_callback));
}

void OfflinePageRequestHandler::ValidateFile() {
  // If the archive file is in internal directory, the offline page can be
  // deemed as trusted without going through valication.
  if (candidates_[candidate_index_].archive_is_in_internal_dir) {
    OnTrustedOfflinePageFound();
    return;
  }

  // Otherwise, the archive file is in public directory. If the digest is empty,
  // the validation can fail immediately.
  if (GetCurrentOfflinePage().digest.empty()) {
    OnFileValidationDone(FileValidationResult::FILE_VALIDATION_FAILED);
    return;
  }

  // If a file:// or content:// URL intent is being viewed, skip the validation.
  // The digest for the file:// or content:// denoted in the intent was computed
  // and used to find the offline page. However, we will not validate and read
  // from the archive archive file assoicated with the offline page since it may
  // not exist or even got modified. We will read from the file:// or content://
  // denoted in the intent  and compute the digest of the read data to make sure
  // it does not get changed.
  if (IsProcessingFileOrContentUrlIntent()) {
    OnFileValidationDone(FileValidationResult::FILE_VALIDATION_SUCCEEDED);
    return;
  }

  GetFileSizeForValidation();
}

void OfflinePageRequestHandler::GetFileSizeForValidation() {
  int64_t* file_size = new int64_t(0);
  file_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetFileSize, GetCurrentOfflinePage().file_path,
                     base::Unretained(file_size)),
      base::BindOnce(&OfflinePageRequestHandler::DidGetFileSizeForValidation,
                     weak_ptr_factory_.GetWeakPtr(), base::Owned(file_size)));
}

void OfflinePageRequestHandler::DidGetFileSizeForValidation(
    const int64_t* actual_file_size) {
  if (*actual_file_size == -1) {
    OnFileValidationDone(FileValidationResult::FILE_NOT_FOUND);
    return;
  }

  if (*actual_file_size != GetCurrentOfflinePage().file_size) {
    OnFileValidationDone(FileValidationResult::FILE_VALIDATION_FAILED);
    return;
  }

  // Open file to compute the digest.
  OpenFile(GetCurrentOfflinePage().file_path,
           base::BindRepeating(&OfflinePageRequestHandler::DidOpenForValidation,
                               weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestHandler::DidOpenForValidation(int result) {
  if (result != net::OK) {
    OnFileValidationDone(FileValidationResult::FILE_VALIDATION_FAILED);
    return;
  }

  if (!buffer_)
    buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(
        kMaxBufferSizeForValidation);

  ReadForValidation();
}

void OfflinePageRequestHandler::ReadForValidation() {
  int result = stream_->Read(
      buffer_.get(), kMaxBufferSizeForValidation,
      base::BindOnce(&OfflinePageRequestHandler::DidReadForValidation,
                     weak_ptr_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING)
    DidReadForValidation(result);
}

void OfflinePageRequestHandler::DidReadForValidation(int result) {
  if (result < 0) {
    OnFileValidationDone(FileValidationResult::FILE_VALIDATION_FAILED);
    return;
  }

  if (result > 0) {
    UpdateDigestOnBackground(
        buffer_, result,
        base::BindOnce(&OfflinePageRequestHandler::ReadForValidation,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // When |result| is 0 (net::OK), it indicates EOF. We need to finalize the
  // hash to get the actual digest.
  FinalizeDigestOnBackground(base::BindOnce(
      &OfflinePageRequestHandler::DidComputeActualDigestForValidation,
      weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestHandler::DidComputeActualDigestForValidation(
    const std::string& actual_digest) {
  DCHECK(!GetCurrentOfflinePage().digest.empty());
  bool is_trusted = actual_digest == GetCurrentOfflinePage().digest;
  OnFileValidationDone(is_trusted
                           ? FileValidationResult::FILE_VALIDATION_SUCCEEDED
                           : FileValidationResult::FILE_VALIDATION_FAILED);
}

void OfflinePageRequestHandler::OnFileValidationDone(
    FileValidationResult result) {
  if (result == FileValidationResult::FILE_VALIDATION_SUCCEEDED) {
    OnTrustedOfflinePageFound();
    return;
  }

  // The stream is no longer needed.
  stream_.reset();

  // Move to next archive file if there is more.
  candidate_index_++;
  if (candidate_index_ < candidates_.size()) {
    ValidateFile();
    return;
  }

  // Otherwise, no trusted offline page can be found so we fall back to the
  // default handling.
  delegate_->FallbackToDefault();
}

void OfflinePageRequestHandler::DidOpenForServing(int result) {
  // Handle the file opening failure.
  if (result != net::OK) {
    // If the file:// or content:// intent is being processed, don't fall
    // back to the default handling. Instead, we should fail the request.
    if (IsProcessingFileOrContentUrlIntent())
      delegate_->NotifyStartError(net::ERR_FAILED);
    else
      delegate_->FallbackToDefault();
    return;
  }

  // Now we're going to read the archive file to serve the offline page. Do
  // all the necessary reporting and bookkeeping for using this offline page.
  VisitTrustedOfflinePage();

  // Note that we always seek to the beginning of the file because the file may
  // have already been read for validation purpose.
  int seek_result = stream_->Seek(
      0, base::BindOnce(&OfflinePageRequestHandler::DidSeekForServing,
                        weak_ptr_factory_.GetWeakPtr()));
  if (seek_result != net::ERR_IO_PENDING)
    DidSeekForServing(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
}

void OfflinePageRequestHandler::DidSeekForServing(int64_t result) {
  DCHECK_LE(result, 0);

  if (result < 0) {
    delegate_->NotifyStartError(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  delegate_->NotifyHeadersComplete(GetCurrentOfflinePage().file_size);
}

void OfflinePageRequestHandler::DidReadForServing(
    scoped_refptr<net::IOBuffer> buf,
    int result) {
  if (result < 0 || !IsProcessingFileOrContentUrlIntent()) {
    buf = nullptr;
    NotifyReadRawDataComplete(result);
    return;
  }

  // At this point, we have result >= 0 && IsProcessingFileOrContentUrlIntent()
  // which means the read succeeds for processing the file:// or content:// URL
  // intent. We need to compute the digest to ensure that the file:// or
  // content:// we read is not modified since the time we received the intent,
  // validated the data provided by file:// or content:// URL, and decided to
  // turn it into the corresponding http/https URL and let
  // OfflinePageRequestHandler handle it.
  if (result > 0) {
    UpdateDigestOnBackground(
        buf, result,
        base::BindOnce(&OfflinePageRequestHandler::NotifyReadRawDataComplete,
                       weak_ptr_factory_.GetWeakPtr(), result));

  } else {
    // When |result| is 0 (net::OK), it indicates EOF. We need to finalize the
    // hash to get the actual digest.
    FinalizeDigestOnBackground(base::BindOnce(
        &OfflinePageRequestHandler::DidComputeActualDigestForServing,
        weak_ptr_factory_.GetWeakPtr(), result));
  }
}

void OfflinePageRequestHandler::NotifyReadRawDataComplete(int result) {
  delegate_->NotifyReadRawDataComplete(result);
}

void OfflinePageRequestHandler::DidComputeActualDigestForServing(
    int result,
    const std::string& actual_digest) {
  // If the actual digest does not match, fail the request job.
  bool mismatch = actual_digest != GetCurrentOfflinePage().digest;
  if (mismatch) {
    // Note: Do not call delegate_->SetOfflinePageNavigationUIData to clear
    // the offline bit since SetOfflinePageNavigationUIData is supposed to
    // be called before the response is being received. Furthermore, there is
    // no need to clear the offline bit since the error code should already
    // indicate that the offline page is not loaded.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ClearOfflinePageData,
                                  delegate_->GetWebContentsGetter()));
    result = net::ERR_FAILED;
  }

  NotifyReadRawDataComplete(result);
}

}  // namespace offline_pages
