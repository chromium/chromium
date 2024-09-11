// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_H_
#define CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_H_

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "sql/database.h"
#include "sql/init_status.h"

namespace net {

// Wraps the SQLite database that provides on-disk storage for user-configured
// TLS certificates. This class is expected to be created and accessed on a
// backend sequence.
class ServerCertificateDatabase {
 public:
  // `storage_dir` will generally be the Profile directory where the DB will be
  // opened from, or created if not exists.
  explicit ServerCertificateDatabase(const base::FilePath& storage_dir);

  ServerCertificateDatabase(const ServerCertificateDatabase&) = delete;
  ServerCertificateDatabase& operator=(const ServerCertificateDatabase&) =
      delete;
  ~ServerCertificateDatabase();

 private:
  sql::InitStatus InitInternal(const base::FilePath& storage_dir);

  // The underlying SQL database.
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_H_
