// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_

#include <memory>

#include "components/enterprise/client_certificates/core/private_key_factory.h"

namespace client_certificates {

std::unique_ptr<PrivateKeyFactory> CreatePrivateKeyFactory();

}  // namespace client_certificates

#endif  // CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_
