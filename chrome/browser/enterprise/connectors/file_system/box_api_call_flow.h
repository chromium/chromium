// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_FLOW_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_FLOW_H_

#include "base/callback.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace enterprise_connectors {

// Helper for making Box API calls.
//
// This class is abstract. The methods OAuth2ApiCallFlow::ProcessApiCallXXX must
// be implemented by subclasses.
class BoxApiCallFlow : public OAuth2ApiCallFlow {
 public:
  BoxApiCallFlow();
  ~BoxApiCallFlow() override;

  // OAuth2ApiCallFlow interface.
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  std::string CreateApiCallBodyContentType() override;
  net::PartialNetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag()
      override;

  // Used by BoxApiCallFlow inherited classes and FileSystemDownloadController
  // to determine whether to use WholeFileUpload or ChunkedFileUpload
  static const size_t kChunkFileUploadMinSize;
  static const size_t kWholeFileUploadMaxSize;
};

// Helper for finding the downloads folder in box.
class BoxFindUpstreamFolderApiCallFlow : public BoxApiCallFlow {
 public:
  using TaskCallback = base::OnceCallback<void(bool, int, const std::string&)>;
  explicit BoxFindUpstreamFolderApiCallFlow(TaskCallback callback);
  ~BoxFindUpstreamFolderApiCallFlow() override;

 protected:
  // BoxApiCallFlow interface.
  GURL CreateApiCallUrl() override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;

 private:
  // Callback for JsonParser that extracts folder id in ProcessApiCallSuccess().
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);

  // Callback from the controller to report success, http_code, folder_id.
  TaskCallback callback_;
  base::WeakPtrFactory<BoxFindUpstreamFolderApiCallFlow> weak_factory_{this};
};

// Helper for creating an upstream downloads folder in box.
class BoxCreateUpstreamFolderApiCallFlow : public BoxApiCallFlow {
 public:
  using TaskCallback = base::OnceCallback<void(bool, int, const std::string&)>;
  explicit BoxCreateUpstreamFolderApiCallFlow(TaskCallback callback);
  ~BoxCreateUpstreamFolderApiCallFlow() override;

 protected:
  // BoxApiCallFlow interface.
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  bool IsExpectedSuccessCode(int code) const override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;

 private:
  // Callback for JsonParser that extracts folder id in ProcessApiCallSuccess().
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);

  // Callback from the controller to report success, http_code, folder_id.
  TaskCallback callback_;
  base::WeakPtrFactory<BoxCreateUpstreamFolderApiCallFlow> weak_factory_{this};
};

// Helper for uploading a small (<= kWholeFileUploadMaxSize) file to upstream
// downloads folder in box.
class BoxWholeFileUploadApiCallFlow : public BoxApiCallFlow {
 public:
  using TaskCallback = base::OnceCallback<void(bool, int)>;
  BoxWholeFileUploadApiCallFlow(TaskCallback callback,
                                const std::string& folder_id,
                                const base::FilePath& target_file_name,
                                const base::FilePath& local_file_path);
  ~BoxWholeFileUploadApiCallFlow() override;

  // Overrides OAuth2ApiCallFlow::Start() to first read local file content
  // before kicking off OAuth2ApiCallFlow::Start().
  void Start(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             const std::string& access_token) override;

 protected:
  // BoxApiCallFlow interface.
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  std::string CreateApiCallBodyContentType() override;
  bool IsExpectedSuccessCode(int code) const override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;

 private:
  // Post a task to ThreadPool to read the local file, forward the
  // parameters from Start() into OnFileRead(), which is the callback that then
  // kicks off OAuth2CallFlow::Start() after file content is read.
  void PostReadFileTask(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token);

  // Post a task to ThreadPool to delete the local file, after calls to
  // Box's whole file upload API, with callback OnFileDeleted(), which reports
  // success back to original thread via callback_.
  void PostDeleteFileTask();

  // Helper functions to read and delete the local file.
  // Task posted to ThreadPool to read the local file. Return type is
  // base::Optional in case file is read successfully but the file content is
  // really empty.
  static base::Optional<std::string> ReadFile(const base::FilePath& path);
  // Callback attached in PostReadFileTask(). Take in read file content and
  // kick off OAuth2CallFlow::Start().
  void OnFileRead(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      base::Optional<std::string> content);
  // Callback attached in PostDeleteFileTask(). Report success back to original
  // thread via callback_.
  void OnFileDeleted(bool result);

  const std::string folder_id_;
  const base::FilePath target_file_name_;
  const base::FilePath local_file_path_;
  const std::string file_mime_type_;
  const std::string multipart_boundary_;
  std::string file_content_;

  // Callback from the controller to report success.
  TaskCallback callback_;
  base::WeakPtrFactory<BoxWholeFileUploadApiCallFlow> factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_FLOW_H_
