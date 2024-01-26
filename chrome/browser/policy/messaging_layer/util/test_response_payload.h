// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test support library for response payloads.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TEST_RESPONSE_PAYLOAD_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TEST_RESPONSE_PAYLOAD_H_

#include <optional>

#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// Class to build response based on an input request.
// Example:
//
//   auto simple_successful_response = ResponseBuilder(std::move(request))
//                                       .Build();
//   auto failed_response = ResponseBuilder(std::move(request))
//                                       .SetSuccess(false)
//                                       .SetForceConfirm(true)
//                                       .Build();
//
// For the document of what response payload should look like, search for
// "{{{Note}}} ERP Response Payload Overview" in the codebase.
class ResponseBuilder {
 public:
  ResponseBuilder() = default;
  // We need a copy constructor here because base::Value::Dict has deleted its
  // copy constructor and gtest will implicitly copy a ResponseBuilder object
  // when it uses |MakeUploadEncryptedReportAction|, which we cannot work
  // around.
  ResponseBuilder(const ResponseBuilder& other);
  ResponseBuilder(ResponseBuilder&& other) = default;
  explicit ResponseBuilder(const base::Value::Dict& request);
  explicit ResponseBuilder(base::Value::Dict&& request);
  ResponseBuilder& SetForceConfirm(bool force_confirm);
  ResponseBuilder& SetNull(bool null);
  ResponseBuilder& SetRequest(const base::Value::Dict& request);
  ResponseBuilder& SetRequest(base::Value::Dict&& request);
  ResponseBuilder& SetSuccess(bool success);

  // Build the response to be fed to a
  // |::policy::CloudPolicyClient::ResponseCallback| object.
  //
  // Because we are focusing on testing here, the build here won't change
  // members of the |ResponseBuilder| instance so that it can be called repeated
  // for convenience. Additionally, this allows us to add more const qualifiers
  // throughout the tests so that we reduce chances of incorrect test code
  // (which itself isn't tested!).
  StatusOr<base::Value::Dict> Build() const;

 private:
  // The request that was sent to the server.
  base::Value::Dict request_;
  // Parameters that can be tuned.
  struct {
    // Whether response should be a success or not
    bool success = true;
    // Whether forceConfirm is set to true or not
    bool force_confirm = false;
    // Whether response should be null
    bool null = false;
  } params_;
};

// Helper functor to be used with EXPECT_CALL and |UploadEncryptedReport|. It
// takes a |ResponseBuilder| object, and builds the response dict based on it
// and the input request from |UploadEncryptedReport|. Example:
//
//
//   EXPECT_CALL(*client_,
//               UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
//     .WillOnce(MakeUploadEncryptedReportAction(
//        std::move(ResponseBuilder()
//                    .SetForceConfirm(force_confirm_by_server))));
//
// This class should be able to handle the majority of cases for defining mocked
// |UploadEncryptedReport| actions. In other circumstances, consider feeding a
// lambda function that uses |ResponseBuilder| to .WillOnce()/.WillRepeatedly().
//
// We don't define |MakeUploadEncryptedReportAction| a lambda function here
// because the mutable nature and the size of the argument list makes it far
// less readable.
class MakeUploadEncryptedReportAction {
 public:
  explicit MakeUploadEncryptedReportAction(
      ResponseBuilder&& response_builder = ResponseBuilder());
  void operator()(base::Value::Dict request,
                  std::optional<base::Value::Dict> context,
                  ReportingServerConnector::ResponseCallback callback);

 private:
  ResponseBuilder response_builder_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TEST_RESPONSE_PAYLOAD_H_
