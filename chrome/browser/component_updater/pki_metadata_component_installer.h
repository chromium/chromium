// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_COMPONENT_INSTALLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "net/net_buildflags.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace component_updater {

// The service that does the heavy lifting to install the PKI metadata
// component.
class PKIMetadataComponentInstallerService final {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called after the CT Log list data is configured.
    virtual void OnCTLogListConfigured() {}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    // Called after the Chrome Root Store data was configured.
    virtual void OnChromeRootStoreConfigured() {}
#endif
  };

  // Returns the live server instance, creating it if it does not exist.
  static PKIMetadataComponentInstallerService* GetInstance();

  PKIMetadataComponentInstallerService();
  ~PKIMetadataComponentInstallerService() = delete;

  // Sets the PKI metadata configuration on the current network service. This is
  // a no-op if the component is not ready.
  // Reconfiguring happens on an asynchronous task.
  void ReconfigureAfterNetworkRestart();

  // Configure Chrome Root Store data. This is separate from
  // ReconfigureAfterNetworkRestart because Chrome Root Store updates don't
  // go through the network service.
  void ConfigureChromeRootStore();

  // Called when the component is ready to be installed.
  void OnComponentReady(base::FilePath install_dir);

  // Writes arbitrary data to the CT config component.
  [[nodiscard]] bool WriteCTDataForTesting(const base::FilePath& path,
                                           const std::string& contents);

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  [[nodiscard]] bool WriteCRSDataForTesting(const base::FilePath& path,
                                            const std::string& contents);
#endif

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Updates the network service CT list with the component delivered data.
  // |ct_config_bytes| should be a serialized CTLogList proto message.
  void UpdateNetworkServiceCTListOnUI(const std::string& ct_config_bytes);

  // Updates the network service pins list with the component delivered data.
  // |kp_config_bytes| should be a serialized KPConfig proto message.
  void UpdateNetworkServiceKPListOnUI(const std::string& kp_config_bytes);

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  // Updates cert verifiers with the component delivered Chrome Root Store
  // data. `chrome_root_store` should be a wrapped
  // chrome_root_store.RootStore proto message.
  void UpdateChromeRootStoreOnUI(
      std::optional<mojo_base::ProtoWrapper> chrome_root_store);

  // Notifies all observers that the Chrome Root Store data has been
  // configured.
  void NotifyChromeRootStoreConfigured();
#endif

  // Notifies all observers that the CT Log list data has been configured.
  void NotifyCTLogListConfigured();

  // The install folder path. An empty path if the component is not ready.
  base::FilePath install_dir_;
  base::ObserverList<Observer> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PKIMetadataComponentInstallerService> weak_factory_{
      this};
};

// Component installer policy for the PKIMetadata component. This component
// includes any dynamically updateable needed for PKI policies enforcement.
class PKIMetadataComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  PKIMetadataComponentInstallerPolicy();
  PKIMetadataComponentInstallerPolicy(
      const PKIMetadataComponentInstallerPolicy&) = delete;
  PKIMetadataComponentInstallerPolicy operator=(
      const PKIMetadataComponentInstallerPolicy&) = delete;
  ~PKIMetadataComponentInstallerPolicy() override;

  // Converts a protobuf repeated bytes array to an array of uint8_t arrays.
  // Exposed for testing.
  static std::vector<std::vector<uint8_t>> BytesArrayFromProtoBytes(
      google::protobuf::RepeatedPtrField<std::string> proto_bytes);

 private:
  // ComponentInstallerPolicy methods:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

void MaybeRegisterPKIMetadataComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_COMPONENT_INSTALLER_H_
