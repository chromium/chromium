// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/https_forwarder.h"

#include <cstring>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/values.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/spawned_test_server/base_test_server.h"
#include "net/test/spawned_test_server/local_test_server.h"
#include "net/test/test_data_directory.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"

namespace chromeos {

// A net::LocalTestServer that handles the actual forwarding to another server.
// Requires that the root certificate used by minica.py be marked as trusted
// before it is used.
class ForwardingServer : public net::LocalTestServer {
 public:
  ForwardingServer(const std::string& ssl_host, const GURL& forward_target);

  // net::LocalTestServer:
  base::Optional<std::vector<base::FilePath>> GetPythonPath() const override;
  bool GetTestServerPath(base::FilePath* testserver_path) const override;
  bool GenerateAdditionalArguments(
      base::DictionaryValue* arguments) const override;

 private:
  const std::string ssl_host_;
  const GURL forward_target_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingServer);
};

ForwardingServer::ForwardingServer(const std::string& ssl_host,
                                   const GURL& forward_target)
    : net::LocalTestServer(net::LocalTestServer::TYPE_HTTPS,
                           SSLOptions(SSLOptions::CERT_AUTO),
                           base::FilePath()),
      ssl_host_(ssl_host),
      forward_target_(forward_target) {}

base::Optional<std::vector<base::FilePath>> ForwardingServer::GetPythonPath()
    const {
  base::Optional<std::vector<base::FilePath>> ret =
      net::LocalTestServer::GetPythonPath();
  if (!ret)
    return base::nullopt;

  base::FilePath net_testserver_path;
  if (!LocalTestServer::GetTestServerPath(&net_testserver_path))
    return base::nullopt;
  ret->push_back(net_testserver_path.DirName());
  return ret;
}

bool ForwardingServer::GetTestServerPath(
    base::FilePath* testserver_path) const {
  base::FilePath source_root_dir;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir))
    return false;

  *testserver_path = source_root_dir.Append("chrome")
                         .Append("browser")
                         .Append("chromeos")
                         .Append("login")
                         .Append("test")
                         .Append("https_forwarder.py");
  return true;
}

bool ForwardingServer::GenerateAdditionalArguments(
    base::DictionaryValue* arguments) const {
  base::FilePath source_root_dir;
  if (!net::LocalTestServer::GenerateAdditionalArguments(arguments) ||
      !base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir))
    return false;

  arguments->SetString("ssl-host", ssl_host_);
  arguments->SetString("forward-target", forward_target_.spec());

  return true;
}

HTTPSForwarder::HTTPSForwarder() {}

HTTPSForwarder::~HTTPSForwarder() {}

GURL HTTPSForwarder::GetURLForSSLHost(const std::string& path) const {
  CHECK(forwarding_server_);
  url::Replacements<char> replacements;
  replacements.SetHost(ssl_host_.c_str(), url::Component(0, ssl_host_.size()));
  return forwarding_server_->GetURL(path).ReplaceComponents(replacements);
}

bool HTTPSForwarder::Initialize(const std::string& ssl_host,
                                const GURL& forward_target) {
  // Mark the root certificate used by minica.py as trusted. It will remain
  // trusted for as long as the HTTPSForwarder object exists. This root cert
  // will be used by the Python part of the HTTPSForwarder to generate a
  // certificate for |ssl_host_|.
  scoped_refptr<net::X509Certificate> root_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "ocsp-test-root.pem");
  if (!root_cert)
    return false;
  test_root_.reset(new net::ScopedTestRoot(root_cert.get()));

  ssl_host_ = ssl_host;
  forwarding_server_.reset(new ForwardingServer(ssl_host, forward_target));
  return forwarding_server_->Start();
}

bool HTTPSForwarder::Stop() {
  return forwarding_server_->Stop();
}

}  // namespace chromeos
