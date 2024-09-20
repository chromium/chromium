// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/server_certificate_database.h"

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace net {

inline constexpr base::FilePath::CharType kServerCertificateDatabaseName[] =
    FILE_PATH_LITERAL("ServerCertificate");

// These database versions should roll together unless we develop migrations.
constexpr int kLowestSupportedDatabaseVersion = 1;
constexpr int kCurrentDatabaseVersion = 1;

namespace {

[[nodiscard]] bool CreateTable(sql::Database& db) {
  static constexpr char kSqlCreateTablePassages[] =
      "CREATE TABLE IF NOT EXISTS certificates("
      // sha256 hash (in hex) of certificate.
      "sha256hash_hex TEXT PRIMARY KEY,"
      // The certificate, DER-encoded.
      "der_cert BLOB NOT NULL,"
      // Trust settings for the certificate.
      "trust_settings BLOB NOT NULL);";

  return db.Execute(kSqlCreateTablePassages);
}

}  // namespace

ServerCertificateDatabase::ServerCertificateDatabase(
    const base::FilePath& storage_dir) {
  InitInternal(storage_dir);
}

ServerCertificateDatabase::~ServerCertificateDatabase() = default;

sql::InitStatus ServerCertificateDatabase::InitInternal(
    const base::FilePath& storage_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.set_histogram_tag("ServerCertificate");

  base::FilePath db_file_path =
      storage_dir.Append(kServerCertificateDatabaseName);
  if (!db_.Open(db_file_path)) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Raze old incompatible databases.
  if (sql::MetaTable::RazeIfIncompatible(&db_, kLowestSupportedDatabaseVersion,
                                         kCurrentDatabaseVersion) ==
      sql::RazeIfIncompatibleResult::kFailed) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Wrap initialization in a transaction to make it atomic.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Initialize the current version meta table. Safest to leave the compatible
  // version equal to the current version - unless we know we're making a very
  // safe backwards-compatible schema change.
  sql::MetaTable meta_table;
  if (!meta_table.Init(&db_, kCurrentDatabaseVersion,
                       /*compatible_version=*/kCurrentDatabaseVersion)) {
    return sql::InitStatus::INIT_FAILURE;
  }
  if (meta_table.GetCompatibleVersionNumber() > kCurrentDatabaseVersion) {
    return sql::INIT_TOO_NEW;
  }

  if (!CreateTable(db_)) {
    return sql::INIT_FAILURE;
  }

  if (!transaction.Commit()) {
    return sql::INIT_FAILURE;
  }

  return sql::InitStatus::INIT_OK;
}

bool ServerCertificateDatabase::InsertOrUpdateCert(
    const CertInformation& cert_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string proto_bytes;
  // If we can't serialize the proto to an array for some reason, bail.
  if (!cert_info.cert_metadata.SerializeToString(&proto_bytes)) {
    return false;
  }

  sql::Statement insert_statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO certificates(sha256hash_hex, der_cert, "
      "trust_settings) VALUES(?,?,?)"));
  insert_statement.BindString(0, cert_info.sha256hash_hex);
  insert_statement.BindBlob(1, cert_info.der_cert);
  insert_statement.BindBlob(2, base::as_byte_span(proto_bytes));
  return insert_statement.Run();
}

std::vector<ServerCertificateDatabase::CertInformation>
ServerCertificateDatabase::RetrieveAllCertificates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<ServerCertificateDatabase::CertInformation> certs;
  static constexpr char kSqlSelectAllCerts[] =
      "SELECT sha256hash_hex, der_cert, trust_settings FROM certificates";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectAllCerts));
  while (statement.Step()) {
    ServerCertificateDatabase::CertInformation cert_info;
    cert_info.sha256hash_hex = statement.ColumnString(0);
    statement.ColumnBlobAsVector(1, &cert_info.der_cert);

    std::string trust_bytes;
    statement.ColumnBlobAsString(2, &trust_bytes);

    if (cert_info.cert_metadata.ParseFromString(trust_bytes)) {
      certs.push_back(std::move(cert_info));
    }
  }

  return certs;
}

ServerCertificateDatabase::CertInformation::CertInformation() = default;
ServerCertificateDatabase::CertInformation::~CertInformation() = default;
ServerCertificateDatabase::CertInformation::CertInformation(
    ServerCertificateDatabase::CertInformation&&) = default;
ServerCertificateDatabase::CertInformation&
ServerCertificateDatabase::CertInformation::operator=(
    ServerCertificateDatabase::CertInformation&& other) = default;

}  // namespace net
