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

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_FLOW_H_
