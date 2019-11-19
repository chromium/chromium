// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/authpolicy/kerberos_files_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/network_service_util.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace chromeos {

namespace {

base::FilePath GetKerberosDir() {
  base::FilePath dir;
  base::PathService::Get(base::DIR_HOME, &dir);
  return dir.Append(kKrb5Directory);
}

// Writes |blob| into file <UserPath>/kerberos/|file_name|. First writes into
// temporary file and then replaces existing one. Prints an error or failure.
void WriteFile(const base::FilePath& path, base::Optional<std::string> blob) {
  if (!blob.has_value())
    return;
  if (!base::ImportantFileWriter::WriteFileAtomically(path, blob.value()))
    LOG(ERROR) << "Failed to write file " << path.value();
}

// Deletes file at |path|. Prints an error or failure.
void RemoveFile(const base::FilePath& path) {
  if (!base::DeleteFile(path, false /* recursive */))
    LOG(ERROR) << "Failed to delete file " << path.value();
}

// Writes |krb5cc| to <DIR_HOME>/kerberos/krb5cc and |krb5config| to
// <DIR_HOME>/kerberos/krb5.conf if set. Creates directories if necessary.
void WriteFiles(base::Optional<std::string> krb5cc,
                base::Optional<std::string> krb5config) {
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
base::Optional<std::string> MaybeAdjustConfig(
    base::Optional<std::string> config,
    bool is_dns_cname_enabled) {
  if (!config.has_value())
    return base::nullopt;
  std::string adjusted_config = base::StringPrintf(
      kKrb5CnameSettings, is_dns_cname_enabled ? "true" : "false");
  adjusted_config.append(config.value());
  return adjusted_config;
}

}  // namespace

const char kKrb5CnameSettings[] =
    "[libdefaults]\n"
    "\tdns_canonicalize_hostname = %s\n"
    "\trdns = false\n";
const char kKrb5CCEnvName[] = "KRB5CCNAME";
const char kKrb5ConfEnvName[] = "KRB5_CONFIG";
const char kKrb5CCFilePrefix[] = "FILE:";
const char kKrb5Directory[] = "kerberos";
const char kKrb5CCFile[] = "krb5cc";
const char kKrb5ConfFile[] = "krb5.conf";

KerberosFilesHandler::KerberosFilesHandler(
    base::RepeatingClosure get_kerberos_files)
    : get_kerberos_files_(std::move(get_kerberos_files)) {
  // Set environment variables for GSSAPI library.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath path;
  CHECK(base::PathService::Get(base::DIR_HOME, &path));
  path = path.Append(kKrb5Directory);
  std::string krb5cc_env_value =
      kKrb5CCFilePrefix + path.Append(kKrb5CCFile).value();
  std::string krb5conf_env_value = path.Append(kKrb5ConfFile).value();
  env->SetVar(kKrb5CCEnvName, krb5cc_env_value);
  env->SetVar(kKrb5ConfEnvName, krb5conf_env_value);

  // Send the environment variables to the network service if it's running
  // out of process.
  if (content::IsOutOfProcessNetworkService()) {
    std::vector<network::mojom::EnvironmentVariablePtr> environment;
    environment.push_back(network::mojom::EnvironmentVariable::New(
        kKrb5CCEnvName, krb5cc_env_value));
    environment.push_back(network::mojom::EnvironmentVariable::New(
        kKrb5ConfEnvName, krb5conf_env_value));
    content::GetNetworkService()->SetEnvironment(std::move(environment));
  }

  // Listen to kDisableAuthNegotiateCnameLookup pref. It might change the
  // Kerberos config.
  negotiate_disable_cname_lookup_.Init(
      prefs::kDisableAuthNegotiateCnameLookup, g_browser_process->local_state(),
      base::BindRepeating(
          &KerberosFilesHandler::OnDisabledAuthNegotiateCnameLookupChanged,
          weak_factory_.GetWeakPtr()));
}

KerberosFilesHandler::~KerberosFilesHandler() = default;

void KerberosFilesHandler::SetFiles(base::Optional<std::string> krb5cc,
                                    base::Optional<std::string> krb5conf) {
  krb5conf =
      MaybeAdjustConfig(krb5conf, !negotiate_disable_cname_lookup_.GetValue());
  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&WriteFiles, std::move(krb5cc), std::move(krb5conf)),
      base::BindOnce(&KerberosFilesHandler::OnFilesChanged,
                     weak_factory_.GetWeakPtr()));
}

void KerberosFilesHandler::DeleteFiles() {
  // These files contain user credentials, so use BLOCK_SHUTDOWN here to make
  // sure they do get deleted.
  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
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

}  // namespace chromeos
