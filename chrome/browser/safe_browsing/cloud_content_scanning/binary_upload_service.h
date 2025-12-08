// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_

#include "base/functional/callback.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace safe_browsing {

// This class encapsulates the process of getting data scanned through a generic
// interface.
class BinaryUploadService : public KeyedService {
 public:
  // The maximum size of data that can be uploaded via this service.
  constexpr static size_t kMaxUploadSizeBytes = 50 * 1024 * 1024;  // 50 MB

  // Callbacks used to pass along the results of scanning. The response protos
  // will only be populated if the result is SUCCESS. Will run on UI thread.
  using ContentAnalysisCallback =
      base::OnceCallback<void(enterprise_connectors::ScanRequestUploadResult,
                              enterprise_connectors::ContentAnalysisResponse)>;

  // A class to encapsulate the a request for upload. This class will provide
  // all the functionality needed to generate a ContentAnalysisRequest, and
  // subclasses will provide different sources of data to upload (e.g. file,
  // page or string).
  class Request : public enterprise_connectors::BinaryUploadRequest {
   public:
    using enterprise_connectors::BinaryUploadRequest::Data;
    using enterprise_connectors::BinaryUploadRequest::DataCallback;
    using enterprise_connectors::BinaryUploadRequest::Id;
    using enterprise_connectors::BinaryUploadRequest::RequestStartCallback;

    Request(ContentAnalysisCallback,
            enterprise_connectors::CloudOrLocalAnalysisSettings settings);
    // Optional constructor which accepts RequestStartCallback. Will be called
    // before request attempts upload.
    Request(ContentAnalysisCallback,
            enterprise_connectors::CloudOrLocalAnalysisSettings settings,
            RequestStartCallback);
    ~Request() override;
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    Request(Request&&) = delete;
    Request& operator=(Request&&) = delete;
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

  // A class to encapsulate requests to cancel.  Any request that match the
  // given criteria is canceled.  This is best effort only, in some cases
  // requests may have already started and can no longer be canceled.
  class CancelRequests {
   public:
    explicit CancelRequests(
        enterprise_connectors::CloudOrLocalAnalysisSettings settings);
    virtual ~CancelRequests();
    CancelRequests(const CancelRequests&) = delete;
    CancelRequests& operator=(const CancelRequests&) = delete;
    CancelRequests(CancelRequests&&) = delete;
    CancelRequests& operator=(CancelRequests&&) = delete;

    void set_user_action_id(const std::string& user_action_id);
    const std::string& get_user_action_id() const { return user_action_id_; }

    const enterprise_connectors::CloudOrLocalAnalysisSettings&
    cloud_or_local_settings() const {
      return cloud_or_local_settings_;
    }

   private:
    std::string user_action_id_;

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

  // Cancel any requests that match the given criteria .  This is a best effort
  // approach only, since it is possible that requests have been started in a
  // way that they are no longer cancelable.
  virtual void MaybeCancelRequests(std::unique_ptr<CancelRequests> cancel) = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<BinaryUploadService> AsWeakPtr() = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_
