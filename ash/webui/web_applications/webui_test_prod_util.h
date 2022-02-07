// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_WEB_APPLICATIONS_WEBUI_TEST_PROD_UTIL_H_
#define ASH_WEBUI_WEB_APPLICATIONS_WEBUI_TEST_PROD_UTIL_H_

#include "content/public/browser/web_ui_data_source.h"

// If the current process is running for tests, configures |host_source| for
// testing by installing a request filter that can be satisfied by tests wanting
// to provide custom resources. Note that if these resources are scripts,
// further CSP changes (e.g. trusted-types) may be required in order for them to
// load. If `real_[should_]handle_request_callback` arguments are provided, the
// test request filter will be configured to handle these as well.
// If not running for tests, `real_[should_]handle_request_callback` arguments
// are passed directly to host_source->SetRequestFilter().
// Returns true if the testing request filter was installed.
bool MaybeConfigureTestableDataSource(
    content::WebUIDataSource* host_source,
    const content::WebUIDataSource::ShouldHandleRequestCallback&
        real_should_handle_request_callback = {},
    const content::WebUIDataSource::HandleRequestCallback&
        real_handle_request_callback = {});

// Configures the data source request filter used for testing. Only testing code
// should call this.
void SetTestableDataSourceRequestHandlerForTesting(
    content::WebUIDataSource::ShouldHandleRequestCallback should_handle,
    content::WebUIDataSource::HandleRequestCallback handler);

#endif  // ASH_WEBUI_WEB_APPLICATIONS_WEBUI_TEST_PROD_UTIL_H_
