// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class which handles communication with the SafeBrowsing servers for
// improved binary download protection.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_PPAPI_DOWNLOAD_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_PPAPI_DOWNLOAD_REQUEST_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace network {
class SimpleURLLoader;
}

class Profile;

namespace safe_browsing {

class DownloadProtectionService;
class SafeBrowsingDatabaseManager;
class PPAPIDownloadRequest;

// A request for checking whether a PPAPI initiated download is safe.
//
// These are considered different from DownloadManager mediated downloads
// because:
//
// * The download bytes are produced by the PPAPI plugin *after* the check
//   returns due to architectural constraints.
//
// * Since the download bytes are produced by the PPAPI plugin, there's no
//   reliable network request information to associate with the download.
//
// PPAPIDownloadRequest objects are owned by the DownloadProtectionService
// indicated by |service|.
class PPAPIDownloadRequest {
 public:
  // The outcome of the request. These values are used for UMA. New values
  // should only be added at the end.
  enum class RequestOutcome : int {
    UNKNOWN,
    REQUEST_DESTROYED,
    UNSUPPORTED_FILE_TYPE,
    TIMEDOUT,
    WHITELIST_HIT,
    REQUEST_MALFORMED,
    FETCH_FAILED,
    RESPONSE_MALFORMED,
    SUCCEEDED
  };

  PPAPIDownloadRequest(
      const GURL& requestor_url,
      const GURL& initiating_frame_url,
      content::WebContents* web_contents,
      const base::FilePath& default_file_path,
      const std::vector<base::FilePath::StringType>& alternate_extensions,
      Profile* profile,
      CheckDownloadCallback callback,
      DownloadProtectionService* service,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager);

  ~PPAPIDownloadRequest();

  // Start the process of checking the download request. The callback passed as
  // the |callback| parameter to the constructor will be invoked with the result
  // of the check at some point in the future.
  //
  // From the this point on, the code is arranged to follow the most common
  // workflow.
  //
  // Note that |this| should be added to the list of pending requests in the
  // associated DownloadProtectionService object *before* calling Start().
  // Otherwise a synchronous Finish() call may result in leaking the
  // PPAPIDownloadRequest object. This is enforced via a DCHECK in
  // DownloadProtectionService.
  void Start();

  // Returns the URL that will be used for download requests.
  static GURL GetDownloadRequestUrl();

 private:
  static const char kDownloadRequestUrl[];

  friend class DownloadProtectionService;

  // Whitelist checking needs to the done on the IO thread.
  static void CheckWhitelistsOnIOThread(
      const GURL& requestor_url,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      base::WeakPtr<PPAPIDownloadRequest> download_request);

  void WhitelistCheckComplete(bool was_on_whitelist);

  void SendRequest();

  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  void OnRequestTimedOut();

  void Finish(RequestOutcome reason, DownloadCheckResult response);

  static DownloadCheckResult DownloadCheckResultFromClientDownloadResponse(
      ClientDownloadResponse::Verdict verdict);

  // Given a |default_file_path| and a list of |alternate_extensions|,
  // constructs a FilePath with each possible extension and returns one that
  // satisfies IsCheckedBinaryFile(). If none are supported, returns an
  // empty FilePath.
  static base::FilePath GetSupportedFilePath(
      const base::FilePath& default_file_path,
      const std::vector<base::FilePath::StringType>& alternate_extensions);

  std::unique_ptr<network::SimpleURLLoader> loader_;
  std::string client_download_request_data_;

  // URL of document that requested the PPAPI download.
  const GURL requestor_url_;

  // URL of the frame that hosts the PPAPI plugin.
  const GURL initiating_frame_url_;

  // URL of the tab that contains the initialting_frame.
  const GURL initiating_main_frame_url_;

  // Tab id that associated with the PPAPI plugin, computed by
  // sessions::SessionTabHelper::IdForTab().
  SessionID tab_id_;

  // If the user interacted with this PPAPI plugin to trigger the download.
  bool has_user_gesture_;

  // Default download path requested by the PPAPI plugin.
  const base::FilePath default_file_path_;

  // List of alternate extensions provided by the PPAPI plugin. Each extension
  // must begin with a leading extension separator.
  const std::vector<base::FilePath::StringType> alternate_extensions_;

  // Callback to invoke with the result of the PPAPI download request check.
  CheckDownloadCallback callback_;

  DownloadProtectionService* service_;
  const scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // Time request was started.
  const base::TimeTicks start_time_;

  // A download path that is supported by SafeBrowsing. This is determined by
  // invoking GetSupportedFilePath(). If non-empty,
  // IsCheckedBinaryFile(supported_path_) is always true. This
  // path is therefore used as the download target when sending the SafeBrowsing
  // ping.
  const base::FilePath supported_path_;

  bool is_extended_reporting_;
  bool is_enhanced_protection_;

  Profile* profile_;

  base::WeakPtrFactory<PPAPIDownloadRequest> weakptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PPAPIDownloadRequest);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_PPAPI_DOWNLOAD_REQUEST_H_
