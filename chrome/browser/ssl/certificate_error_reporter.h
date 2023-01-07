// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CERTIFICATE_ERROR_REPORTER_H_
#define CHROME_BROWSER_SSL_CERTIFICATE_ERROR_REPORTER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

// Provides functionality for sending reports about invalid SSL
// certificate chains to a report collection server.
class CertificateErrorReporter {
 public:
  // Creates a certificate error reporter that will send certificate
  // error reports to |upload_url|, using |url_loader_factory| as the
  // context for the reports.
  CertificateErrorReporter(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& upload_url);

  // Allows tests to use a server public key with known private key.
  // |server_public_key| must outlive the ErrorReporter.
  CertificateErrorReporter(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& upload_url,
      const uint8_t server_public_key[/* 32 */],
      const uint32_t server_public_key_version);

  CertificateErrorReporter(const CertificateErrorReporter&) = delete;
  CertificateErrorReporter& operator=(const CertificateErrorReporter&) = delete;

  virtual ~CertificateErrorReporter();

  // Sends a certificate report to the report collection server. The
  // |serialized_report| is expected to be a serialized protobuf
  // containing information about the hostname, certificate chain, and
  // certificate errors encountered when validating the chain.
  //
  // |SendReport| actually sends the report over the network; callers are
  // responsible for enforcing any preconditions (such as obtaining user
  // opt-in, only sending reports for certain hostnames, checking for
  // incognito mode, etc.).
  //
  // On some platforms (but not all), CertificateErrorReporter can use
  // an HTTP endpoint to send encrypted extended reporting reports. On
  // unsupported platforms, callers must send extended reporting reports
  // over SSL.
  //
  //
  // Calls |success_callback| when the report is successfully sent and the
  // server returns an HTTP 200 response.
  // In all other cases, calls |error_callback| with the URL of the upload,
  // net error and HTTP response code parameters.
  virtual void SendExtendedReportingReport(
      const std::string& serialized_report,
      base::OnceCallback<void()> success_callback,
      base::OnceCallback<void(int /* net_error */,
                              int /* http_response_code */)> error_callback);

  void set_upload_url_for_testing(const GURL& url) { upload_url_ = url; }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  GURL upload_url_;

  const uint8_t* server_public_key_;
  const uint32_t server_public_key_version_;
};

#endif  // CHROME_BROWSER_SSL_CERTIFICATE_ERROR_REPORTER_H_
