// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_FLOW_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_FLOW_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace enterprise_connectors {

extern const char kBoxEnterpriseIdFieldName[];
extern const char kBoxLoginFieldName[];
extern const char kBoxNameFieldName[];

struct BoxApiCallResponse;

// Helper for making Box API calls.
//
// This class is abstract. The methods OAuth2ApiCallFlow::ProcessApiCallXXX must
// be implemented by subclasses.
class BoxApiCallFlow : public OAuth2ApiCallFlow {
 public:
  using Response = BoxApiCallResponse;
  using TaskCallback = base::OnceCallback<void(Response)>;
  BoxApiCallFlow();
  ~BoxApiCallFlow() override;

  // OAuth2ApiCallFlow interface.
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  std::string CreateApiCallBodyContentType() override;
  net::PartialNetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag()
      override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;

  static std::string FormatSHA1Digest(const std::string& sha_digest);
  static GURL MakeUrlToShowFile(const std::string& file_id);
  static GURL MakeUrlToShowFolder(const std::string& folder_id);

  // Used by BoxApiCallFlow inherited classes and BoxUploader
  // to determine whether to use WholeFileUpload or ChunkedFileUpload
  static const size_t kChunkFileUploadMinSize;
  static const size_t kWholeFileUploadMaxSize;

  using ParseResult = data_decoder::DataDecoder::ValueOrError;

 protected:
  void OnFailureJsonParsed(int http_error, ParseResult result);
  // Called in OnFailureJsonParsed() to send the failure back.
  virtual void ProcessFailure(Response response) = 0;

  base::WeakPtrFactory<BoxApiCallFlow> weak_factory_{this};
};

// Helper for getting the folder of a file in Box.
class BoxGetFileFolderApiCallFlow : public BoxApiCallFlow {
 public:
  // Additional callback arg is: folder_id for the downloads folder found in
  // Box.
  using TaskCallback = base::OnceCallback<void(Response, const std::string&)>;
  explicit BoxGetFileFolderApiCallFlow(TaskCallback callback,
                                       const std::string& file_id);
  ~BoxGetFileFolderApiCallFlow() override;

 protected:
  // BoxApiCallFlow interface.
  GURL CreateApiCallUrl() override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessFailure(Response response) override;

 private:
  // Callback for JsonParser that extracts folder id in ProcessApiCallSuccess().
  void OnSuccessJsonParsed(ParseResult result);

  // Callback from the uploader to report success, http_code, folder_id.
  TaskCallback callback_;
  const std::string file_id_;
  base::WeakPtrFactory<BoxGetFileFolderApiCallFlow> weak_factory_{this};
};

// Helper for finding the downloads folder in Box.
class BoxFindUpstreamFolderApiCallFlow : public BoxApiCallFlow {
 public:
  // Additional callback arg is: folder_id for the downloads folder found in
  // Box.
  using TaskCallback = base::OnceCallback<void(Response, const std::string&)>;
  explicit BoxFindUpstreamFolderApiCallFlow(TaskCallback callback);
  ~BoxFindUpstreamFolderApiCallFlow() override;

 protected:
  // BoxApiCallFlow interface.
  GURL CreateApiCallUrl() override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessFailure(Response response) override;

 private:
  // Callback for JsonParser that extracts folder id in ProcessApiCallSuccess().
  void OnSuccessJsonParsed(ParseResult result);

  // Callback from the uploader to report success, http_code, folder_id.
  TaskCallback callback_;
  base::WeakPtrFactory<BoxFindUpstreamFolderApiCallFlow> weak_factory_{this};
};

// Helper for creating an upstream downloads folder in box.
class BoxCreateUpstreamFolderApiCallFlow : public BoxApiCallFlow {
 public:
  // Additional callback arg is: folder_id for the downloads folder created in
  // Box.
  using TaskCallback = base::OnceCallback<void(Response, const std::string&)>;
  explicit BoxCreateUpstreamFolderApiCallFlow(TaskCallback callback);
  ~BoxCreateUpstreamFolderApiCallFlow() override;

 protected:
  // BoxApiCallFlow interface.
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  bool IsExpectedSuccessCode(int code) const override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessFailure(Response response) override;

 private:
  // Callback for JsonParser that extracts folder id in ProcessApiCallSuccess().
  void OnSuccessJsonParsed(int network_response_code, ParseResult result);

  // Callback from the uploader to report success, http_code, folder_id.
  TaskCallback callback_;
  base::WeakPtrFactory<BoxCreateUpstreamFolderApiCallFlow> weak_factory_{this};
};

// Helper for performing preflight checks before uploading a file.
class BoxGetCurrentUserApiCallFlow : public BoxApiCallFlow {
 public:
  explicit BoxGetCurrentUserApiCallFlow(
      base::OnceCallback<void(Response, base::Value)> callback);
  ~BoxGetCurrentUserApiCallFlow() override;

  // BoxApiCallFlow interface.
  GURL CreateApiCallUrl() override;
  bool IsExpectedSuccessCode(int code) const override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessFailure(Response response) override;

 private:
  // Callback for JsonParser that extracts enterprise_id in
  // ProcessApiCallSuccess().
  void OnJsonParsed(ParseResult result);

  // Callback from the controller to report success, http_code, folder_id.
  base::OnceCallback<void(Response, base::Value)> callback_;
  base::WeakPtrFactory<BoxGetCurrentUserApiCallFlow> weak_factory_{this};
};

// Helper for performing preflight checks before uploading a file.
class BoxPreflightCheckApiCallFlow : public BoxApiCallFlow {
 public:
  BoxPreflightCheckApiCallFlow(TaskCallback callback,
                               const base::FilePath& target_file_name,
                               const std::string& folder_id);
  ~BoxPreflightCheckApiCallFlow() override;

 protected:
  // BoxApiCallFlow interface.
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  std::string GetRequestTypeForBody(const std::string& body) override;
  bool IsExpectedSuccessCode(int code) const override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessFailure(Response response) override;

 private:
  // Callback from the controller to report success, http_code, folder_id.
  TaskCallback callback_;
  const base::FilePath target_file_name_;
  const std::string folder_id_;

  base::WeakPtrFactory<BoxPreflightCheckApiCallFlow> weak_factory_{this};
};

// Helper for uploading a small (<= kWholeFileUploadMaxSize) file to upstream
// downloads folder in box.
class BoxWholeFileUploadApiCallFlow : public BoxApiCallFlow {
 public:
  // Additional args are: file id to show the uploaded item on Box.
  using TaskCallback = base::OnceCallback<void(Response, const std::string&)>;
  BoxWholeFileUploadApiCallFlow(TaskCallback callback,
                                const std::string& folder_id,
                                const std::string& mime_type,
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
  void ProcessFailure(Response response) override;

  void SetFileReadForTesting(std::string content);

 private:
  // Post a task to ThreadPool to read the local file, forward the
  // parameters from Start() into OnFileRead(), which is the callback that then
  // kicks off OAuth2CallFlow::Start() after file content is read.
  void PostReadFileTask(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token);
  // Callback attached in PostReadFileTask(). Take in read file content and
  // kick off OAuth2CallFlow::Start().
  void OnFileRead(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      absl::optional<std::string> file_content);

  // Task posted to ThreadPool to read the local file. Return type is
  // base::Optional in case file is read successfully but the file content is
  // really empty.
  static absl::optional<std::string> ReadFile(const base::FilePath& path);

  const std::string folder_id_;
  const std::string mime_type_;
  const base::FilePath target_file_name_;
  const base::FilePath local_file_path_;
  const std::string multipart_boundary_;
  std::string file_content_;

  // Callback from the uploader to report success.
  TaskCallback callback_;
  base::WeakPtrFactory<BoxWholeFileUploadApiCallFlow> weak_factory_{this};
};

// Helper for starting an upload session to designated Chrome downloads folder
// in Box.
class BoxCreateUploadSessionApiCallFlow : public BoxApiCallFlow {
 public:
  // Additional callback args are: session endpoints provided in API request
  // response, and part_size for each chunk to be uploaded.
  using TaskCallback = base::OnceCallback<void(Response, base::Value, size_t)>;
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
  void ProcessFailure(Response response) override;

 private:
  void OnSuccessJsonParsed(ParseResult result);

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
  using TaskCallback = base::OnceCallback<void(Response, base::Value)>;
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
  void ProcessFailure(Response response) override;

 private:
  void OnSuccessJsonParsed(ParseResult result);
  TaskCallback callback_;
  const std::string& part_content_;
  const std::string content_range_;
  base::WeakPtrFactory<BoxPartFileUploadApiCallFlow> weak_factory_{this};
};

// Helper for aborting an upload session if there's unrecoverable failure during
// uploading file chunks.
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
  void ProcessFailure(Response response) override;

 private:
  TaskCallback callback_;
};

// Helper for committing an upload session once all the parts are uploaded
// successfully.
class BoxCommitUploadSessionApiCallFlow
    : public BoxChunkedUploadBaseApiCallFlow {
 public:
  // Additional args are: Retry-After header duration, and file id to show the
  // uploaded item on Box.
  using TaskCallback =
      base::OnceCallback<void(Response, base::TimeDelta, const std::string&)>;
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
  void ProcessFailure(Response response) override;

 private:
  TaskCallback callback_;
  const GURL commit_endpoint_;
  const std::string sha_digest_;
  base::Value upload_session_parts_;
  base::TimeDelta retry_after_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_FLOW_H_
