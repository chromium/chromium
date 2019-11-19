// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "chrome/browser/printing/cloud_print/privet_url_loader.h"
#include "net/base/host_port_pair.h"

namespace base {
class RefCountedMemory;
}

namespace gfx {
class Size;
}

namespace printing {
class PwgRasterConverter;
}

namespace cloud_print {

class PrivetHTTPClient;

// Represents a simple request that returns pure JSON.
class PrivetJSONOperation {
 public:
  // If value is null, the operation failed.
  using ResultCallback =
      base::OnceCallback<void(const base::DictionaryValue* /*value*/)>;

  virtual ~PrivetJSONOperation() {}

  virtual void Start() = 0;

  virtual PrivetHTTPClient* GetHTTPClient() = 0;
};

// Privet HTTP client. Must outlive the operations it creates.
class PrivetHTTPClient {
 public:
  virtual ~PrivetHTTPClient() {}

  // A name for the HTTP client, e.g. the device name for the privet device.
  virtual const std::string& GetName() = 0;

  // Creates operation to query basic information about local device.
  virtual std::unique_ptr<PrivetJSONOperation> CreateInfoOperation(
      PrivetJSONOperation::ResultCallback callback) = 0;

  // Creates a URL loader for PrivetV1.
  virtual std::unique_ptr<PrivetURLLoader> CreateURLLoader(
      const GURL& url,
      const std::string& http_method,
      PrivetURLLoader::Delegate* delegate) = 0;

  virtual void RefreshPrivetToken(
      PrivetURLLoader::TokenCallback token_callback) = 0;
};

// Represents a full registration flow (/privet/register), normally consisting
// of calling the start action, the getClaimToken action, and calling the
// complete action. Some intervention from the caller is required to display the
// claim URL to the user (noted in OnPrivetRegisterClaimURL).
class PrivetRegisterOperation {
 public:
  enum FailureReason {
    FAILURE_NETWORK,
    FAILURE_HTTP_ERROR,
    FAILURE_JSON_ERROR,
    FAILURE_MALFORMED_RESPONSE,
    FAILURE_TOKEN,
    FAILURE_UNKNOWN,
  };

  class Delegate {
   public:
    ~Delegate() {}

    // Called when a user needs to claim the printer by visiting the given URL.
    virtual void OnPrivetRegisterClaimToken(
        PrivetRegisterOperation* operation,
        const std::string& token,
        const GURL& url) = 0;

    // TODO(noamsml): Remove all unnecessary parameters.
    // Called in case of an error while registering.  |action| is the
    // registration action taken during the error. |reason| is the reason for
    // the failure. |printer_http_code| is the http code returned from the
    // printer. If it is -1, an internal error occurred while trying to complete
    // the request. |json| may be null if printer_http_code signifies an error.
    virtual void OnPrivetRegisterError(PrivetRegisterOperation* operation,
                                       const std::string& action,
                                       FailureReason reason,
                                       int printer_http_code,
                                       const base::DictionaryValue* json) = 0;

    // Called when the registration is done.
    virtual void OnPrivetRegisterDone(PrivetRegisterOperation* operation,
                                      const std::string& device_id) = 0;
  };

  virtual ~PrivetRegisterOperation() {}

  virtual void Start() = 0;
  // Owner SHOULD call explicitly before destroying operation.
  virtual void Cancel() = 0;
  virtual void CompleteRegistration() = 0;

  virtual PrivetHTTPClient* GetHTTPClient() = 0;
};

class PrivetLocalPrintOperation {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnPrivetPrintingDone(
        const PrivetLocalPrintOperation* print_operation) = 0;
    virtual void OnPrivetPrintingError(
        const PrivetLocalPrintOperation* print_operation, int http_code) = 0;
  };

  virtual ~PrivetLocalPrintOperation() {}

  virtual void Start() = 0;

  // Required print data. MUST be called before calling Start().
  virtual void SetData(scoped_refptr<base::RefCountedMemory> data) = 0;

  // Optional attributes for /submitdoc. Call before calling Start().
  // |ticket| should be in CJT format.
  virtual void SetTicket(base::Value ticket) = 0;

  // |capabilities| should be in CDD format.
  virtual void SetCapabilities(const std::string& capabilities) = 0;

  // Username and jobname are for display only.
  virtual void SetUsername(const std::string& username) = 0;
  virtual void SetJobname(const std::string& jobname) = 0;

  // Document page size.
  virtual void SetPageSize(const gfx::Size& page_size) = 0;

  // For testing, inject an alternative PWG raster converter.
  virtual void SetPwgRasterConverterForTesting(
      std::unique_ptr<printing::PwgRasterConverter> pwg_raster_converter) = 0;

  virtual PrivetHTTPClient* GetHTTPClient() = 0;
};

// Privet HTTP client. Must outlive the operations it creates.
class PrivetV1HTTPClient {
 public:
  virtual ~PrivetV1HTTPClient() {}

  static std::unique_ptr<PrivetV1HTTPClient> CreateDefault(
      std::unique_ptr<PrivetHTTPClient> info_client);

  // A name for the HTTP client, e.g. the device name for the privet device.
  virtual const std::string& GetName() = 0;

  // Creates operation to query basic information about local device.
  virtual std::unique_ptr<PrivetJSONOperation> CreateInfoOperation(
      PrivetJSONOperation::ResultCallback callback) = 0;

  // Creates operation to register local device using Privet v1 protocol.
  virtual std::unique_ptr<PrivetRegisterOperation> CreateRegisterOperation(
      const std::string& user,
      PrivetRegisterOperation::Delegate* delegate) = 0;

  // Creates operation to query capabilities of local printer.
  virtual std::unique_ptr<PrivetJSONOperation> CreateCapabilitiesOperation(
      PrivetJSONOperation::ResultCallback callback) = 0;

  // Creates operation to submit print job to local printer.
  virtual std::unique_ptr<PrivetLocalPrintOperation> CreateLocalPrintOperation(
      PrivetLocalPrintOperation::Delegate* delegate) = 0;
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_H_
