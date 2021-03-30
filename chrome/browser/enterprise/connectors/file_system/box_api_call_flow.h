// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_FLOW_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_FLOW_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace enterprise_connectors {

// Helper for making Box API calls.
//
// This class is abstract. The methods OAuth2ApiCallFlow::ProcessApiCallXXX must
// be implemented by subclasses.
class BoxApiCallFlow : public OAuth2ApiCallFlow {
 public:
  // Callback args are: whether request returned success, and
  // net::HttpStatusCode.
  using TaskCallback = base::OnceCallback<void(bool, int)>;
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

// Helper for finding the downloads folder in Box.
class BoxFindUpstreamFolderApiCallFlow : public BoxApiCallFlow {
 public:
  // Additional callback arg is: folder_id for the downloads folder found in
  // Box.
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
  // Additional callback arg is: folder_id for the downloads folder created in
  // Box.
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
  // Try to delete a local file and return true if and only if the file existed
  // and was successfully deleted
  // TODO(https://crbugs.com/1190891): Move to shared FSConnector code when we
  // support other partners
  static bool DeleteIfExists(base::FilePath file_path);

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
  base::WeakPtrFactory<BoxWholeFileUploadApiCallFlow> weak_factory_{this};
};

// Helper for starting an upload session to designated Chrome downloads folder
// in Box.
class BoxCreateUploadSessionApiCallFlow : public BoxApiCallFlow {
 public:
  // Additional callback arg is: session endpoints provided in API request
  // response.
  using TaskCallback = base::OnceCallback<void(bool, int, base::Value)>;
  BoxCreateUploadSessionApiCallFlow(TaskCallback callback,
                                    const std::string& folder_id,
                                    const size_t file_size,
                                    const base::FilePath& file_name);
  ~BoxCreateUploadSessionApiCallFlow() override;

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
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);

  TaskCallback callback_;
  const std::string folder_id_;
  const size_t file_size_;
  const base::FilePath file_name_;

  base::WeakPtrFactory<BoxCreateUploadSessionApiCallFlow> weak_factory_{this};
};

// Base helper for API requests related to chunked file uploads. Since
// BoxCreateUploadSessionApiCallFlow gives all relevant endpoints for an upload
// session in its response, the subsequent steps can take each endpoint as
// constructor argument and just return in CreateApiCallUrl() without
// formatting.
class BoxChunkedUploadBaseApiCallFlow : public BoxApiCallFlow {
 protected:
  explicit BoxChunkedUploadBaseApiCallFlow(const GURL endpoint);
  // BoxApiCallFlow interface.
  GURL CreateApiCallUrl() final;
  const GURL endpoint_;
};

// Helper for uploading a part of the file to Box.
class BoxPartFileUploadApiCallFlow : public BoxChunkedUploadBaseApiCallFlow {
 public:
  // Additional callback arg is: uploaded file part info in API request response
  // that needs to be attached in CommitUploadSession request.
  // Callback invoked when the file part upload completes. The bool argument is
  // true if the upload succeeded and false otherwise. The int argument
  // represents the final HTTP status code of the request. The Value holds a
  // JSON part object as returned by the Box Upload Part API, which is valid
  // only on success.
  using TaskCallback = base::OnceCallback<void(bool, int, base::Value)>;
  BoxPartFileUploadApiCallFlow(TaskCallback callback,
                               const std::string& session_endpoint,
                               const std::string& file_part_content,
                               const size_t byte_from,
                               const size_t byte_to,
                               const size_t byte_total);
  ~BoxPartFileUploadApiCallFlow() override;

  // Helper method.
  static std::string CreateFileDigest(const std::string& content);

 protected:
  // BoxApiCallFlow interface.
  net::HttpRequestHeaders CreateApiCallHeaders() override;
  std::string CreateApiCallBody() override;
  std::string CreateApiCallBodyContentType() override;
  std::string GetRequestTypeForBody(const std::string& body) override;
  bool IsExpectedSuccessCode(int code) const override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;

 private:
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);
  TaskCallback callback_;
  const std::string& part_content_;
  const std::string content_range_;
  const std::string sha_digest_;
  base::WeakPtrFactory<BoxPartFileUploadApiCallFlow> weak_factory_{this};
};

// Helper for committing an upload session once all the parts are uploaded
// successfully.
class BoxAbortUploadSessionApiCallFlow
    : public BoxChunkedUploadBaseApiCallFlow {
 public:
  BoxAbortUploadSessionApiCallFlow(TaskCallback callback,
                                   const std::string& session_endpoint);
  ~BoxAbortUploadSessionApiCallFlow() override;

 protected:
  // BoxApiCallFlow interface.
  std::string GetRequestTypeForBody(const std::string& body) override;
  bool IsExpectedSuccessCode(int code) const override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;

 private:
  TaskCallback callback_;
};

// Helper for committing an upload session once all the parts are uploaded
// successfully.
class BoxCommitUploadSessionApiCallFlow
    : public BoxChunkedUploadBaseApiCallFlow {
 public:
  using TaskCallback = base::OnceCallback<void(bool, int, base::TimeDelta)>;
  BoxCommitUploadSessionApiCallFlow(TaskCallback callback,
                                    const std::string& session_endpoint,
                                    const base::Value& parts,
                                    const std::string digest);
  ~BoxCommitUploadSessionApiCallFlow() override;

 protected:
  // BoxApiCallFlow interface.
  net::HttpRequestHeaders CreateApiCallHeaders() override;
  std::string CreateApiCallBody() override;
  bool IsExpectedSuccessCode(int code) const override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;

 private:
  TaskCallback callback_;
  const GURL commit_endpoint_;
  const base::Value upload_session_parts_;
  const std::string sha_digest_;
  base::TimeDelta retry_after_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_FLOW_H_
