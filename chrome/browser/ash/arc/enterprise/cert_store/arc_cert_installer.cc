// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/cert_store/arc_cert_installer.h"

#include <cert.h>

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/arc/enterprise/cert_store/arc_cert_installer_utils.h"
#include "chrome/browser/ash/policy/remote_commands/user_command_arc_job.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/services/keymanagement/public/mojom/cert_store_types.mojom.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_util_nss.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

CertDescription::CertDescription(crypto::RSAPrivateKey* placeholder_key,
                                 CERTCertificate* nss_cert,
                                 keymanagement::mojom::ChapsSlot slot,
                                 std::string label,
                                 std::string id)
    : placeholder_key(placeholder_key),
      nss_cert(nss_cert),
      slot(slot),
      label(label),
      id(id) {}

CertDescription::CertDescription(CertDescription&& other) = default;

CertDescription& CertDescription::operator=(CertDescription&& other) = default;

CertDescription::~CertDescription() = default;

ArcCertInstaller::ArcCertInstaller(content::BrowserContext* context)
    : ArcCertInstaller(Profile::FromBrowserContext(context),
                       std::make_unique<policy::RemoteCommandsQueue>()) {}

ArcCertInstaller::ArcCertInstaller(
    Profile* profile,
    std::unique_ptr<policy::RemoteCommandsQueue> queue)
    : profile_(profile), queue_(std::move(queue)), weak_ptr_factory_(this) {
  VLOG(1) << "ArcCertInstaller::ArcCertInstaller";

  queue_->AddObserver(this);
}

ArcCertInstaller::~ArcCertInstaller() {
  VLOG(1) << "ArcCertInstaller::~ArcCertInstaller";

  queue_->RemoveObserver(this);
}

std::map<std::string, std::string> ArcCertInstaller::InstallArcCerts(
    std::vector<CertDescription> certificates,
    InstallArcCertsCallback callback) {
  VLOG(1) << "ArcCertInstaller::InstallArcCerts";

  if (callback_) {
    LOG(WARNING) << "The last ARC cert installation has not finished before "
                 << "starting a new one.";
    std::move(callback_).Run(false /* result */);
    pending_status_ = true;
  }

  // Map the certificate name to the dummy RSA SPKI.
  std::map<std::string, std::string> required_cert_names;
  callback_ = std::move(callback);

  for (const auto& certificate : certificates) {
    const net::ScopedCERTCertificate& nss_cert = certificate.nss_cert;
    if (!nss_cert) {
      LOG(ERROR)
          << "An invalid certificate has been passed to ArcCertInstaller";
      continue;
    }

    std::string cert_name =
        x509_certificate_model::GetCertNameOrNickname(nss_cert.get());
    required_cert_names[cert_name] = InstallArcCert(cert_name, certificate);
  }

  // Cleanup |known_cert_names_| according to |required_cert_names|.
  for (auto it = known_cert_names_.begin(); it != known_cert_names_.end();) {
    auto cert_name = it++;
    if (!required_cert_names.count(*cert_name))
      known_cert_names_.erase(cert_name);
  }

  if (pending_commands_.empty() && callback_) {
    std::move(callback_).Run(pending_status_);
    pending_status_ = true;
  }
  return required_cert_names;
}

std::string ArcCertInstaller::InstallArcCert(
    const std::string& name,
    const CertDescription& certificate) {
  VLOG(1) << "ArcCertInstaller::InstallArcCert " << name;

  // Do not install certificate if it is already installed or pending.
  if (known_cert_names_.count(name)) {
    VLOG(1) << "Certificate " << name << " is already installed or pending";
    return "";
  }

  std::string der_cert;
  if (!net::x509_util::GetDEREncoded(certificate.nss_cert.get(), &der_cert)) {
    LOG(ERROR) << "Certificate encoding error: " << name;
    return "";
  }
  known_cert_names_.insert(name);

  // Install certificate.
  std::unique_ptr<policy::RemoteCommandJob> job =
      std::make_unique<policy::UserCommandArcJob>(profile_);
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_USER_ARC_COMMAND);

  command_proto.set_command_id(next_id_);
  command_proto.set_age_of_command(0);

  std::string der_cert64 = base::Base64Encode(der_cert);

  crypto::RSAPrivateKey* rsa = certificate.placeholder_key.get();
  std::string pkcs12 = CreatePkcs12ForKey(name, rsa->key());
  // NOTE: command_proto contains crypto key value. Avoid logging its value out
  // on release build, by using LOG instead of SYSLOG.
  command_proto.set_payload(
      base::StringPrintf("{\"type\":\"INSTALL_KEY_PAIR\","
                         "\"payload\":\"{"
                         "\\\"key\\\":\\\"%s\\\","
                         "\\\"alias\\\":\\\"%s\\\","
                         "\\\"certs\\\":[\\\"%s\\\"],"
                         "\\\"is_user_selectable\\\":false"
                         "}\"}",
                         pkcs12.c_str(), name.c_str(), der_cert64.c_str()));
  LOG(INFO) << "Attempting to install a key pair via remote command.";
  if (!job || !job->Init(queue_->GetNowTicks(), command_proto,
                         enterprise_management::SignedData())) {
    LOG(ERROR) << "Initialization of remote command failed";
    known_cert_names_.erase(name);
    return "";
  } else {
    pending_commands_[next_id_++] = name;
    queue_->AddJob(std::move(job));

    return ExportSpki(rsa);
  }
}

void ArcCertInstaller::OnJobFinished(policy::RemoteCommandJob* command) {
  VLOG(1) << "ArcCertInstaller::OnJobFinished";
  if (!pending_commands_.count(command->unique_id())) {
    LOG(ERROR) << "Received invalid ARC remote command with unrecognized "
               << "unique_id = " << command->unique_id();
    return;
  }

  // If the cert installation is failed, save the status and remove from the
  // |known_cert_names_|. Use the |pending_status_| to notify clients should
  // re-try installation.
  if (command->status() != policy::RemoteCommandJob::Status::SUCCEEDED) {
    LOG(ERROR) << "Failed to install certificate "
               << pending_commands_[command->unique_id()];
    if (known_cert_names_.count(pending_commands_[command->unique_id()])) {
      known_cert_names_.erase(pending_commands_[command->unique_id()]);
      pending_status_ = false;
    }
  }
  pending_commands_.erase(command->unique_id());

  if (pending_commands_.empty() && callback_) {
    std::move(callback_).Run(pending_status_);
    pending_status_ = true;
  }
}

}  // namespace arc
