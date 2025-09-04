// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"

class PrefService;
namespace network {
class SharedURLLoaderFactory;
}  // namespace network
namespace policy {
class DeviceManagementService;
}  // namespace policy

namespace client_certificates {

class CertificateProvisioningService;
class CertificateStore;
class PrivateKeyFactory;

std::unique_ptr<PrivateKeyFactory> CreatePrivateKeyFactory();

// Creates and returns a CertificateProvisioningService.
std::unique_ptr<client_certificates::CertificateProvisioningService>
CreateBrowserCertificateProvisioningService(
    PrefService* local_state,
    client_certificates::CertificateStore* certificate_store,
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

}  // namespace client_certificates

#endif  // CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_
