// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/browser/enterprise/connectors/analysis/analysis_settings.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

class Profile;

namespace safe_browsing {

// This class encapsulates the process of getting data scanned through a generic
// interface.
class BinaryUploadService : public KeyedService {
 public:
  // The maximum size of data that can be uploaded via this service.
  constexpr static size_t kMaxUploadSizeBytes = 50 * 1024 * 1024;  // 50 MB

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Result {
    // Unknown result.
    UNKNOWN = 0,

    // The request succeeded.
    SUCCESS = 1,

    // The upload failed, for an unspecified reason.
    UPLOAD_FAILURE = 2,

    // The upload succeeded, but a response was not received before timing out.
    TIMEOUT = 3,

    // The file was too large to upload.
    FILE_TOO_LARGE = 4,

    // The BinaryUploadService failed to get an InstanceID token.
    FAILED_TO_GET_TOKEN = 5,

    // The user is unauthorized to make the request.
    UNAUTHORIZED = 6,

    // Some or all parts of the file are encrypted.
    FILE_ENCRYPTED = 7,

    // The file's type is not supported and the file was not uploaded.
    DLP_SCAN_UNSUPPORTED_FILE_TYPE = 8,

    // The server returned a 429 HTTP status indicating too many requests are
    // being sent.
    TOO_MANY_REQUESTS = 9,

    kMaxValue = TOO_MANY_REQUESTS,
  };

  static std::string ResultToString(Result result);

  // Callbacks used to pass along the results of scanning. The response protos
  // will only be populated if the result is SUCCESS.
  using ContentAnalysisCallback =
      base::OnceCallback<void(Result,
                              enterprise_connectors::ContentAnalysisResponse)>;

  // A class to encapsulate the a request for upload. This class will provide
  // all the functionality needed to generate a ContentAnalysisRequest, and
  // subclasses will provide different sources of data to upload (e.g. file,
  // page or string).
  class Request {
   public:
    Request(ContentAnalysisCallback,
            enterprise_connectors::CloudOrLocalAnalysisSettings settings);
    virtual ~Request();
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    Request(Request&&) = delete;
    Request& operator=(Request&&) = delete;

    // Structure of data returned in the callback to GetRequestData().
    struct Data {
      Data();
      Data(Data&&);
      Data& operator=(Data&&);
      Data(const Data&) = delete;
      Data& operator=(const Data&) = delete;
      ~Data();

      // The data content. Only populated for string requests.
      std::string contents;

      // The path to the file to be scanned. Only populated for file requests.
      base::FilePath path;

      // The SHA256 of the data.
      std::string hash;

      // The size of the data. This can differ from `contents.size()` when the
      // file is too large for deep scanning. This field will contain the true
      // size.
      uint64_t size = 0;

      // The mime type of the data. Only populated for file requests.
      std::string mime_type;

      // The page's content. Only populated for page requests.
      base::ReadOnlySharedMemoryRegion page;
    };

    // Aynchronously returns the data required to make a MultipartUploadRequest.
    // `result` is set to SUCCESS if getting the request data succeeded or
    // some value describing the error.
    using DataCallback = base::OnceCallback<void(Result, Data)>;
    virtual void GetRequestData(DataCallback callback) = 0;

    // Returns the URL to send the request to.
    GURL GetUrlWithParams() const;

    // Returns the metadata to upload, as a ContentAnalysisRequest.
    const enterprise_connectors::ContentAnalysisRequest&
    content_analysis_request() const {
      return content_analysis_request_;
    }

    const enterprise_connectors::CloudOrLocalAnalysisSettings&
    cloud_or_local_settings() const {
      return cloud_or_local_settings_;
    }

    void set_tab_url(const GURL& tab_url);
    const GURL& tab_url() const;

    void set_per_profile_request(bool per_profile_request);
    bool per_profile_request() const;

    // Methods for modifying the ContentAnalysisRequest.
    void set_analysis_connector(
        enterprise_connectors::AnalysisConnector connector);
    void set_url(const std::string& url);
    void set_source(const std::string& source);
    void set_destination(const std::string& destination);
    void set_csd(ClientDownloadRequest csd);
    void add_tag(const std::string& tag);
    void set_email(const std::string& email);
    void set_fcm_token(const std::string& token);
    void set_device_token(const std::string& token);
    void set_filename(const std::string& filename);
    void set_digest(const std::string& digest);
    void clear_dlp_scan_request();
    void set_client_metadata(enterprise_connectors::ClientMetadata metadata);
    void set_content_type(const std::string& type);

    std::string SetRandomRequestToken();

    // Methods for accessing the ContentAnalysisRequest.
    enterprise_connectors::AnalysisConnector analysis_connector();
    const std::string& device_token() const;
    const std::string& request_token() const;
    const std::string& fcm_notification_token() const;
    const std::string& filename() const;
    const std::string& digest() const;
    const std::string& content_type() const;

    // Finish the request, with the given `result` and `response` from the
    // server.
    void FinishRequest(Result result,
                       enterprise_connectors::ContentAnalysisResponse response);

    // Calls SerializeToString on the appropriate proto request.
    void SerializeToString(std::string* destination) const;

    // Method used to identify authentication requests. This is used for
    // optimizations such as omitting FCM code paths for auth requests.
    virtual bool IsAuthRequest() const;

    const std::string& access_token() const;
    void set_access_token(const std::string& access_token);

   private:
    enterprise_connectors::ContentAnalysisRequest content_analysis_request_;
    ContentAnalysisCallback content_analysis_callback_;

    // Settings used to determine how the request is used in the cloud or
    // locally.
    enterprise_connectors::CloudOrLocalAnalysisSettings
        cloud_or_local_settings_;

    // The URL of the page that initially triggered the scan.
    GURL tab_url_;

    // Indicates if the request was triggered by a profile-level policy or not.
    bool per_profile_request_ = false;

    // Access token to be attached in the request headers.
    std::string access_token_;
  };

  // A class to encapsulate the a request acknowledgement. This class will
  // provide all the functionality needed to generate a
  // ContentAnalysisAcknowledgement.
  class Ack {
   public:
    explicit Ack(enterprise_connectors::CloudOrLocalAnalysisSettings settings);
    virtual ~Ack();
    Ack(const Ack&) = delete;
    Ack& operator=(const Ack&) = delete;
    Ack(Ack&&) = delete;
    Ack& operator=(Ack&&) = delete;

    void set_request_token(const std::string& token);
    void set_status(
        enterprise_connectors::ContentAnalysisAcknowledgement::Status status);
    void set_final_action(
        enterprise_connectors::ContentAnalysisAcknowledgement::FinalAction
            final_action);

    const enterprise_connectors::CloudOrLocalAnalysisSettings&
    cloud_or_local_settings() const {
      return cloud_or_local_settings_;
    }

    const enterprise_connectors::ContentAnalysisAcknowledgement& ack() const {
      return ack_;
    }

   private:
    enterprise_connectors::ContentAnalysisAcknowledgement ack_;

    // Settings used to determine how the request is used in the cloud or
    // locally.
    enterprise_connectors::CloudOrLocalAnalysisSettings
        cloud_or_local_settings_;
  };

  static BinaryUploadService* GetForProfile(
      Profile* profile,
      const enterprise_connectors::AnalysisSettings& settings);

  // Upload the given file contents for deep scanning if the browser is
  // authorized to upload data, otherwise queue the request.
  virtual void MaybeUploadForDeepScanning(std::unique_ptr<Request> request) = 0;

  // Send an acknowledgement for the request with the given token.
  virtual void MaybeAcknowledge(std::unique_ptr<Ack> ack) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_
