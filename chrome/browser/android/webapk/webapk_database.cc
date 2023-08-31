// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_database.h"

namespace webapk {
WebApkDatabase::WebApkDatabase(AbstractWebApkDatabaseFactory* database_factory,
                               ReportErrorCallback error_callback) {
  // TODO(parsam): implement
}
WebApkDatabase::~WebApkDatabase() {
  // TODO(parsam): implement
}

void WebApkDatabase::OpenDatabase(RegistryOpenedCallback callback) {
  // TODO(parsam): implement
}

void WebApkDatabase::Write(
    const RegistryUpdateData& update_data,
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    CompletionCallback callback) {
  // TODO(parsam): implement
}

}  // namespace webapk
