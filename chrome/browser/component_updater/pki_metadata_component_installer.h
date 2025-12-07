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
#include "components/component_updater/component_installer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "net/base/hash_value.h"
#include "net/net_buildflags.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
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

    // Called after the MTC Metadata was configured.
    virtual void OnMtcMetadataConfigured() {}
#endif
  };

  // Returns the live server instance, creating it if it does not exist.
  static PKIMetadataComponentInstallerService* GetInstance();

  // Wraps BytesArrayFromProtoBytes, exposed for testing.
  static std::vector<std::vector<uint8_t>> BytesArrayFromProtoBytesForTesting(
      const google::protobuf::RepeatedPtrField<std::string>& proto_bytes);

  // Wraps SHA256HashValueArrayFromProtoBytes, exposed for testing.
  static std::vector<net::SHA256HashValue>
  SHA256HashValueArrayFromProtoBytesForTesting(
      const google::protobuf::RepeatedPtrField<std::string>& proto_bytes);

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

  // Configure signatureless Merkle Tree Certificate metadata. This is
  // distributed separately from the Chrome Root Store data, in the Fastpush
  // component.
  void ConfigureMtcMetadata();

  // Called when the component is ready to be installed.
  void OnComponentReady(base::FilePath install_dir);

  // Called when the Fastpush component is ready to be installed.
  void OnFastpushComponentReady(base::FilePath install_dir);

  // Writes arbitrary data to the CT config component.
  [[nodiscard]] bool WriteCTDataForTesting(const base::FilePath& path,
                                           const std::string& contents);

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  [[nodiscard]] bool WriteCRSDataForTesting(const base::FilePath& path,
                                            const std::string& contents);
  [[nodiscard]] bool WriteMtcMetadataForTesting(const base::FilePath& path,
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
  // Updates SystemNetworkContextManager and cert verifiers with the component
  // delivered Chrome Root Store data. `chrome_root_store` should be a wrapped
  // chrome_root_store.RootStore proto message.
  void UpdateChromeRootStoreOnUI(
      std::optional<mojo_base::ProtoWrapper> chrome_root_store);

  // Updates SystemNetworkContextManager and cert verifiers with the component
  // delivered MTC Metadata. `mtc_metadata` should be a wrapped
  // chrome_root_store::MtcMetadata proto message.
  void UpdateMtcMetadataOnUI(
      std::optional<mojo_base::ProtoWrapper> mtc_metadata);

  // Updates the network service with the Trust Anchor IDs, combining the
  // cached data from both the Chrome Root Store and the MTC Metadata.
  // (https://tlswg.org/tls-trust-anchor-ids/draft-ietf-tls-trust-anchor-ids.html)
  void UpdateTrustAnchorIDsImpl();

  // Updates the network service with the Trust Anchor IDs in
  // `chrome_root_store`.
  void UpdateCRSTrustAnchorIDs(
      const mojo_base::ProtoWrapper& chrome_root_store);

  // Updates the network service with the Trust Anchor IDs in `mtc_metadata`,
  // and returns true on success. A false returns indicates a problem with the
  // protobuffer data (such as out-of-date metadata, indicating the metadata
  // should not be sent to the CertVerifierServiceFactory either.)
  bool UpdateMtcMetadataTrustAnchorIDs(
      const mojo_base::ProtoWrapper& mtc_metadata);

  // Notifies all observers that the Chrome Root Store data has been
  // configured.
  void NotifyChromeRootStoreConfigured();

  // Notifies all observers that the MTC Metadata has been configured.
  void NotifyMtcMetadataConfigured();
#endif

  // Notifies all observers that the CT Log list data has been configured.
  void NotifyCTLogListConfigured();

  // The install folder path. An empty path if the component is not ready.
  base::FilePath install_dir_;
  base::FilePath fastpush_install_dir_;
  base::ObserverList<Observer> observers_;

  // Cached TAI data from the last component updates (or initialized from the
  // compiled in data if each component hasn't updated yet). Some of the data
  // from both the CRS and MTC Metadata need to be merged together but are
  // updated separately, so the latest necessary data from each is cached.
  std::vector<std::vector<uint8_t>> crs_trust_anchor_ids_;
  absl::flat_hash_set<std::vector<uint8_t>> crs_trusted_mtc_logids_;

  struct MtcLogIdAndLandmarkTrustAnchorId {
    MtcLogIdAndLandmarkTrustAnchorId();
    ~MtcLogIdAndLandmarkTrustAnchorId();
    MtcLogIdAndLandmarkTrustAnchorId(const MtcLogIdAndLandmarkTrustAnchorId&);
    MtcLogIdAndLandmarkTrustAnchorId(MtcLogIdAndLandmarkTrustAnchorId&&);

    std::vector<uint8_t> anchor_log_id;
    std::vector<uint8_t> landmark_trust_anchor_id;
  };
  std::vector<MtcLogIdAndLandmarkTrustAnchorId>
      mtc_log_id_landmark_trust_anchor_ids_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PKIMetadataComponentInstallerService> weak_factory_{
      this};
};

void MaybeRegisterPKIMetadataComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_COMPONENT_INSTALLER_H_
