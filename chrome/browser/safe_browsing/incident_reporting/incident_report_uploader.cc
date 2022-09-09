// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/incident_report_uploader.h"

namespace safe_browsing {

IncidentReportUploader::~IncidentReportUploader() {
}

IncidentReportUploader::IncidentReportUploader(OnResultCallback callback)
    : callback_(std::move(callback)) {}

}  // namespace safe_browsing
