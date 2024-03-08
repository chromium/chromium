// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_UPLOADER_TEST_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_UPLOADER_TEST_UTILS_H_

#include "chrome/browser/safe_browsing/cloud_content_scanning/connector_data_pipe_getter.h"

namespace enterprise_connectors::test {

std::string GetBodyFromFileOrPageRequest(
    safe_browsing::ConnectorDataPipeGetter* data_pipe_getter);

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_UPLOADER_TEST_UTILS_H_
