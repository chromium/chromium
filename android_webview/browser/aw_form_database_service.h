// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_FORM_DATABASE_SERVICE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_FORM_DATABASE_SERVICE_H_

#include "base/files/file_path.h"
#include "components/webdata/common/web_database_service.h"

namespace android_webview {

// Drops the tables created by the old AwFormDatabaseService that was used until
// M132.
class AwFormDatabaseService {
 public:
  explicit AwFormDatabaseService(const base::FilePath path);

  AwFormDatabaseService(const AwFormDatabaseService&) = delete;
  AwFormDatabaseService& operator=(const AwFormDatabaseService&) = delete;

  ~AwFormDatabaseService();

 private:
  scoped_refptr<WebDatabaseService> web_database_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_FORM_DATABASE_SERVICE_H_
