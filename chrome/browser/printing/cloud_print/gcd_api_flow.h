// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace cloud_print {

// API flow for communicating with cloud print and cloud devices.
class GCDApiFlow {
 public:
  // TODO(noamsml): Better error model for this class.
  enum Status {
    SUCCESS,
    ERROR_TOKEN,
    ERROR_NETWORK,
    ERROR_HTTP_CODE,
    ERROR_FROM_SERVER,
    ERROR_MALFORMED_RESPONSE
  };

  // Provides GCDApiFlowImpl with parameters required to make request.
  // Parses results of requests.
  class Request {
   public:
    enum NetworkTrafficAnnotation {
      TYPE_SEARCH,
      TYPE_PRIVET_REGISTER,
    };

    virtual ~Request();

    // Called if the API flow fails.
    virtual void OnGCDApiFlowError(Status status) = 0;

    // Called when the API flow finishes.
    virtual void OnGCDApiFlowComplete(const base::DictionaryValue& value) = 0;

    // Returns the URL for this request.
    virtual GURL GetURL() = 0;

    // Returns the scope parameter for use with OAuth.
    virtual std::string GetOAuthScope() = 0;

    // Returns extra headers, if any, to send with this request.
    virtual std::vector<std::string> GetExtraRequestHeaders() = 0;

    // Returns the network traffic annotation tag for this request.
    virtual NetworkTrafficAnnotation GetNetworkTrafficAnnotationType() = 0;
  };

  GCDApiFlow();
  virtual ~GCDApiFlow();

  static std::unique_ptr<GCDApiFlow> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

  virtual void Start(std::unique_ptr<Request> request) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCDApiFlow);
};

class CloudPrintApiFlowRequest : public GCDApiFlow::Request {
 public:
  CloudPrintApiFlowRequest();
  ~CloudPrintApiFlowRequest() override;

  // GCDApiFlowRequest implementation
  std::string GetOAuthScope() override;
  std::vector<std::string> GetExtraRequestHeaders() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPrintApiFlowRequest);
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_H_
