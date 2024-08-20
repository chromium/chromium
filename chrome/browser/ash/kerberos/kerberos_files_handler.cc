// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/kerberos/kerberos_files_handler.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"

namespace ash {

namespace {

base::FilePath GetKerberosDir() {
  base::FilePath dir;
  base::PathService::Get(base::DIR_HOME, &dir);
  return dir.Append(kKrb5Directory);
}

// Writes |blob| into file <UserPath>/kerberos/|file_name|. First writes into
// temporary file and then replaces existing one. Prints an error or failure.
void WriteFile(const base::FilePath& path, std::optional<std::string> blob) {
  if (!blob.has_value())
    return;
  if (!base::ImportantFileWriter::WriteFileAtomically(path, blob.value()))
    LOG(ERROR) << "Failed to write file " << path.value();
}

// Deletes file at |path|. Prints an error or failure.
void RemoveFile(const base::FilePath& path) {
  if (!base::DeleteFile(path))
    LOG(ERROR) << "Failed to delete file " << path.value();
}

// Writes |krb5cc| to <DIR_HOME>/kerberos/krb5cc and |krb5config| to
// <DIR_HOME>/kerberos/krb5.conf if set. Creates directories if necessary.
void WriteFiles(std::optional<std::string> krb5cc,
                std::optional<std::string> krb5config) {
  base::FilePath dir = GetKerberosDir();
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(dir, &error)) {
    LOG(ERROR) << "Failed to create '" << dir.value()
               << "' directory: " << base::File::ErrorToString(error);
    return;
  }

  WriteFile(dir.Append(kKrb5CCFile), std::move(krb5cc));
  WriteFile(dir.Append(kKrb5ConfFile), std::move(krb5config));
}

// Deletes <DIR_HOME>/kerberos/krb5cc and <DIR_HOME>/kerberos/krb5.conf.
void RemoveFiles() {
  base::FilePath dir = GetKerberosDir();
  RemoveFile(dir.Append(kKrb5CCFile));
  RemoveFile(dir.Append(kKrb5ConfFile));
}

// If |config| has a value, puts canonicalization settings first depending on
// user policy. Whatever setting comes first wins, so even if krb5.conf sets
// rdns or dns_canonicalize_hostname below, it would get overridden.
std::optional<std::string> MaybeAdjustConfig(std::optional<std::string> config,
                                             bool is_dns_cname_enabled) {
  if (!config.has_value())
    return std::nullopt;
  static constexpr char kKrb5CnameSettings[] =
      "[libdefaults]\n"
      "\tdns_canonicalize_hostname = %s\n"
      "\trdns = false\n";
  std::string adjusted_config = base::StringPrintf(
      kKrb5CnameSettings, is_dns_cname_enabled ? "true" : "false");
  adjusted_config.append(config.value());
  return adjusted_config;
}

}  // namespace

const char kKrb5Directory[] = "kerberos";
const char kKrb5CCFile[] = "krb5cc";
const char kKrb5ConfFile[] = "krb5.conf";

KerberosFilesHandler::KerberosFilesHandler(
    base::RepeatingClosure get_kerberos_files)
    : get_kerberos_files_(std::move(get_kerberos_files)) {
  // Listen to kDisableAuthNegotiateCnameLookup pref. It might change the
  // Kerberos config.
  negotiate_disable_cname_lookup_.Init(
      prefs::kDisableAuthNegotiateCnameLookup, g_browser_process->local_state(),
      base::BindRepeating(
          &KerberosFilesHandler::OnDisabledAuthNegotiateCnameLookupChanged,
          weak_factory_.GetWeakPtr()));
}

KerberosFilesHandler::~KerberosFilesHandler() = default;

void KerberosFilesHandler::SetFiles(std::optional<std::string> krb5cc,
                                    std::optional<std::string> krb5conf) {
  krb5conf =
      MaybeAdjustConfig(krb5conf, !negotiate_disable_cname_lookup_.GetValue());
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&WriteFiles, std::move(krb5cc), std::move(krb5conf)),
      base::BindOnce(&KerberosFilesHandler::OnFilesChanged,
                     weak_factory_.GetWeakPtr()));
}

void KerberosFilesHandler::DeleteFiles() {
  // These files contain user credentials, so use BLOCK_SHUTDOWN here to make
  // sure they do get deleted.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&RemoveFiles),
      base::BindOnce(&KerberosFilesHandler::OnFilesChanged,
                     weak_factory_.GetWeakPtr()));
}

void KerberosFilesHandler::SetFilesChangedForTesting(
    base::OnceClosure callback) {
  files_changed_for_testing_ = std::move(callback);
}

void KerberosFilesHandler::OnDisabledAuthNegotiateCnameLookupChanged() {
  // Refresh kerberos files to adjust config for changed pref.
  get_kerberos_files_.Run();
}

void KerberosFilesHandler::OnFilesChanged() {
  if (files_changed_for_testing_)
    std::move(files_changed_for_testing_).Run();
}

}  // namespace ash
