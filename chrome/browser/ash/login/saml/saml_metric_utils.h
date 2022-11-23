// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_SAML_METRIC_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_SAML_METRIC_UTILS_H_

#include <string>

namespace ash {
namespace metrics {

void RecordSAMLProvider(const std::string& provider);

}  // namespace metrics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_SAML_METRIC_UTILS_H_
