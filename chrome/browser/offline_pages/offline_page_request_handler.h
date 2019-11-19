// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_HANDLER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/offline_pages/core/archive_validator.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "content/public/common/resource_type.h"

namespace base {
class FilePath;
class TaskRunner;
}

namespace content {
class WebContents;
}

namespace net {
class FileStream;
class HttpRequestHeaders;
class HttpResponseHeaders;
class IOBuffer;
}  // namespace net

namespace offline_pages {

// A handler that serves content from a trusted offline file, located either
// in internal directory or in public directory with digest validated, when a
// http/https URL is being navigated on disconnected or poor network. If no
// trusted offline file can be found, fall back to the default network handling
// which will try to load the live version.
//
// The only header handled by this request job is:
// * "X-Chrome-offline" custom header.
class OfflinePageRequestHandler {
 public:
  // This enum is used for UMA reporting. It contains all possible outcomes of
  // handling requests that might service offline page in different network
  // conditions. Generally one of these outcomes will happen.
  // The fringe errors (like no OfflinePageModel, etc.) are not reported due
  // to their low probability.
  // NOTE: because this is used for UMA reporting, these values should not be
  // changed or reused; new values should be ended immediately before the MAX
  // value. Make sure to update the histogram enum
  // (OfflinePagesAggregatedRequestResult in enums.xml) accordingly.
  // Public for testing.
  enum class AggregatedRequestResult {
    SHOW_OFFLINE_ON_DISCONNECTED_NETWORK,
    PAGE_NOT_FOUND_ON_DISCONNECTED_NETWORK,
    SHOW_OFFLINE_ON_FLAKY_NETWORK,
    PAGE_NOT_FOUND_ON_FLAKY_NETWORK,
    SHOW_OFFLINE_ON_PROHIBITIVELY_SLOW_NETWORK,
    PAGE_NOT_FOUND_ON_PROHIBITIVELY_SLOW_NETWORK,
    PAGE_NOT_FRESH_ON_PROHIBITIVELY_SLOW_NETWORK,
    SHOW_OFFLINE_ON_CONNECTED_NETWORK,
    PAGE_NOT_FOUND_ON_CONNECTED_NETWORK,
    NO_TAB_ID,
    NO_WEB_CONTENTS,
    SHOW_NET_ERROR_PAGE,
    REDIRECTED_ON_DISCONNECTED_NETWORK,
    REDIRECTED_ON_FLAKY_NETWORK,
    REDIRECTED_ON_PROHIBITIVELY_SLOW_NETWORK,
    REDIRECTED_ON_CONNECTED_NETWORK,
    DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK,
    DIGEST_MISMATCH_ON_FLAKY_NETWORK,
    DIGEST_MISMATCH_ON_PROHIBITIVELY_SLOW_NETWORK,
    DIGEST_MISMATCH_ON_CONNECTED_NETWORK,
    FILE_NOT_FOUND,
    AGGREGATED_REQUEST_RESULT_MAX
  };

  // This enum is used for UMA reporting of the UI location from which an
  // offline page was launched.
  // NOTE: because this is used for UMA reporting, these values should not be
  // changed or reused; new values should be appended immediately before COUNT.
  // Make sure to update the histogram enum (OfflinePagesAcessEntryPoint in
  // enums.xml) accordingly.
  enum class AccessEntryPoint {
    // Any other cases not listed below.
    UNKNOWN = 0,
    // Launched from the NTP suggestions or bookmarks.
    NTP_SUGGESTIONS_OR_BOOKMARKS = 1,
    // Launched from Downloads home.
    DOWNLOADS = 2,
    // Launched from the omnibox.
    OMNIBOX = 3,
    // Launched from Chrome Custom Tabs.
    CCT = 4,
    // Launched due to clicking a link in a page.
    LINK = 5,
    // Launched due to hitting the reload button or hitting enter in the
    // omnibox.
    RELOAD = 6,
    // Launched due to clicking a notification.
    NOTIFICATION = 7,
    // Launched due to processing a file URL intent to view MHTML file.
    FILE_URL_INTENT = 8,
    // Launched due to processing a content URL intent to view MHTML content.
    CONTENT_URL_INTENT = 9,
    // Launched due to clicking "Open" link in the progress bar.
    PROGRESS_BAR = 10,
    // Launched from content suggestion on the net error page.
    NET_ERROR_PAGE = 11,
    COUNT  // Must be last.
  };

  enum class NetworkState {
    // No network connection.
    DISCONNECTED_NETWORK,
    // Prohibitively slow means that the NetworkQualityEstimator reported a
    // connection slow enough to warrant showing an offline page if available.
    // This requires offline previews to be enabled and the URL of the request
    // to be allowed by previews.
    PROHIBITIVELY_SLOW_NETWORK,
    // Network error received due to bad network, i.e. connected to a hotspot or
    // proxy that does not have a working network.
    FLAKY_NETWORK,
    // Network is in working condition.
    CONNECTED_NETWORK,
    // Force to load the offline page if it is available, though network is in
    // working condition.
    FORCE_OFFLINE_ON_CONNECTED_NETWORK
  };

  // Describes the info about an offline page candidate.
  struct Candidate {
    OfflinePageItem offline_page;
    // Whether the archive file is in internal directory, for which it can be
    // deemed trusted without validation.
    bool archive_is_in_internal_dir;
  };

  // Delegate that allows the consumer to overwrite certain behaviors.
  // All methods are called from IO thread.
  class Delegate {
   public:
    using WebContentsGetter =
        base::RepeatingCallback<content::WebContents*(void)>;
    using TabIdGetter =
        base::RepeatingCallback<bool(content::WebContents*, int*)>;

    // Falls back to the default handling in the case that the offline content
    // can't be found and served.
    virtual void FallbackToDefault() = 0;

    // Notifies that a start error has occurred.
    virtual void NotifyStartError(int error) = 0;

    // Notifies that the headers have been received. |file_size| indicates the
    // total size to be read.
    virtual void NotifyHeadersComplete(int64_t file_size) = 0;

    // Notifies that ReadRawData() is completed. |bytes_read| is either >= 0 to
    // indicate a successful read and count of bytes read, or < 0 to indicate an
    // error.
    virtual void NotifyReadRawDataComplete(int bytes_read) = 0;

    // Sets |is_offline_page| flag in NavigationUIData.
    // Note: this should be called before the response data is being constructed
    // and returned because NavigationUIData may be disposed right after the
    // response data is received.
    virtual void SetOfflinePageNavigationUIData(bool is_offline_page) = 0;

    // Returns true if the preview is allowed.
    virtual bool ShouldAllowPreview() const = 0;

    // Returns the page transition type for this navigation.
    virtual int GetPageTransition() const = 0;

    // Returns the getter to retrieve the web contents associated with this
    // this navigation.
    virtual WebContentsGetter GetWebContentsGetter() const = 0;

    // Returns the getter to retrieve the ID of the tab where this navigation
    // occurs.
    virtual TabIdGetter GetTabIdGetter() const = 0;

   protected:
    virtual ~Delegate() {}
  };

  class ThreadSafeArchiveValidator final
      : public ArchiveValidator,
        public base::RefCountedThreadSafe<ThreadSafeArchiveValidator> {
   public:
    ThreadSafeArchiveValidator() = default;

   private:
    friend class base::RefCountedThreadSafe<ThreadSafeArchiveValidator>;
    ~ThreadSafeArchiveValidator() override = default;
  };

  // Reports the aggregated result combining both request result and network
  // state.
  static void ReportAggregatedRequestResult(AggregatedRequestResult result);

  OfflinePageRequestHandler(
      const GURL& url,
      const net::HttpRequestHeaders& extra_request_headers,
      Delegate* delegate);

  ~OfflinePageRequestHandler();

  void Start();
  void Kill();
  bool IsServingOfflinePage() const;

  // Returns the http response headers to trigger a redirect.
  scoped_refptr<net::HttpResponseHeaders> GetRedirectHeaders();

  // Reads at most |dest_size| bytes of the raw data into |dest| buffer.
  int ReadRawData(net::IOBuffer* dest, int dest_size);

  // Called when offline pages matching the request URL are found. The list is
  // sorted based on creation date in descending order.
  void OnOfflinePagesAvailable(const std::vector<Candidate>& candidates);

 private:
  enum class FileValidationResult {
    // The file passes the digest validation and thus can be trusted.
    FILE_VALIDATION_SUCCEEDED,
    // The file does not exist.
    FILE_NOT_FOUND,
    // The digest validation fails.
    FILE_VALIDATION_FAILED,
  };

  void StartAsync();

  NetworkState GetNetworkState() const;

  AccessEntryPoint GetAccessEntryPoint() const;

  const OfflinePageItem& GetCurrentOfflinePage() const;

  bool IsProcessingFileUrlIntent() const;
  bool IsProcessingContentUrlIntent() const;
  bool IsProcessingFileOrContentUrlIntent() const;

  void OnTrustedOfflinePageFound();
  void VisitTrustedOfflinePage();
  void Redirect(const GURL& redirected_url);

  void OpenFile(const base::FilePath& file_path,
                const base::Callback<void(int)>& callback);
  void UpdateDigestOnBackground(
      scoped_refptr<net::IOBuffer> buffer,
      size_t len,
      base::OnceCallback<void(void)> digest_updated_callback);
  void FinalizeDigestOnBackground(
      base::OnceCallback<void(const std::string&)> digest_finalized_callback);

  // All the work related to validations.
  void ValidateFile();
  void GetFileSizeForValidation();
  void DidGetFileSizeForValidation(const int64_t* actual_file_size);
  void DidOpenForValidation(int result);
  void ReadForValidation();
  void DidReadForValidation(int result);
  void DidComputeActualDigestForValidation(const std::string& actual_digest);
  void OnFileValidationDone(FileValidationResult result);

  // All the work related to serving from the archive file.
  void DidOpenForServing(int result);
  void DidSeekForServing(int64_t result);
  void DidReadForServing(scoped_refptr<net::IOBuffer> buf, int result);
  void NotifyReadRawDataComplete(int result);
  void DidComputeActualDigestForServing(int result,
                                        const std::string& actual_digest);

  GURL url_;
  Delegate* delegate_;

  OfflinePageHeader offline_header_;
  NetworkState network_state_;

  // For redirect simulation.
  scoped_refptr<net::HttpResponseHeaders> fake_headers_for_redirect_;

  // To run any file related operations.
  scoped_refptr<base::TaskRunner> file_task_runner_;

  // For file validaton purpose.
  std::vector<Candidate> candidates_;
  size_t candidate_index_;
  scoped_refptr<net::IOBuffer> buffer_;
  scoped_refptr<ThreadSafeArchiveValidator> archive_validator_;

  // For the purpose of serving from the archive file.
  base::FilePath file_path_;
  std::unique_ptr<net::FileStream> stream_;

  base::WeakPtrFactory<OfflinePageRequestHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestHandler);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_HANDLER_H_
