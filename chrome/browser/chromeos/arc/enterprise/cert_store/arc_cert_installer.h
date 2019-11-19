// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_queue.h"
#include "net/cert/scoped_nss_types.h"

class Profile;

namespace content {

class BrowserContext;

}  // namespace content

namespace arc {

// This class manages the certificates, available to ARC.
// It keeps track of the certificates and installs missing ones via
// ARC remote commands.
class ArcCertInstaller : public policy::RemoteCommandsQueue::Observer {
 public:
  explicit ArcCertInstaller(content::BrowserContext* context);

  // This constructor should be used only for testing.
  ArcCertInstaller(Profile* profile,
                   std::unique_ptr<policy::RemoteCommandsQueue> queue);
  ~ArcCertInstaller() override;

  using InstallArcCertsCallback = base::OnceCallback<void(bool result)>;

  // Install missing certificates via ARC remote commands.
  //
  // Return set of the names of certificates required being installed on ARC.
  // Return false via |callback| in case of any error, and true otherwise.
  // Made virtual for override in test.
  virtual std::set<std::string> InstallArcCerts(
      const std::vector<net::ScopedCERTCertificate>& certs,
      InstallArcCertsCallback callback);

 private:
  // Install ARC certificate if not installed yet.
  void InstallArcCert(const std::string& name,
                      const net::ScopedCERTCertificate& nss_cert);

  // RemoteCommandsQueue::Observer overrides:
  void OnJobStarted(policy::RemoteCommandJob* command) override {}
  void OnJobFinished(policy::RemoteCommandJob* command) override;

  Profile* profile_;  // not owned

  // A valid callback when the caller of |InstallArcCerts| method is awaiting
  // for a response.
  InstallArcCertsCallback callback_;

  // Status of a pending certificate installation query.
  // True by default.
  // False if the installation failed.
  // The |pending_status_| is returned via |callback_|.
  bool pending_status_ = true;

  // Names of certificates installed or pending to be installed on ARC.
  std::set<std::string> known_cert_names_;

  // Map from unique_id of the remote command to the corresponding cert name.
  std::map<int, std::string> pending_commands_;

  // Remote commands queue.
  std::unique_ptr<policy::RemoteCommandsQueue> queue_;

  // The next remote command unique id. Should be increased after every usage.
  int next_id_ = 1;

  base::WeakPtrFactory<ArcCertInstaller> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcCertInstaller);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_H_
