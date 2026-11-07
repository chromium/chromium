// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/component_updater/component_installer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "net/base/hash_value.h"
#include "net/cert/root_store_proto_lite/mtc_config.pb.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

struct SSLConfigServiceMtcLandmarkInfo;

namespace cert_verifier::mojom {
class PKIMetadataUpdate;
}

namespace component_updater {

// The service that does the heavy lifting to install the PKI Metadata and PKI
// Metadata Fastpush components.
class PKIMetadataComponentInstallerService final {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called after the PKI Metadata component data was processed. This may be
    // called after the update was successfully configured or if the update was
    // ignored due to an error.
    virtual void OnPKIMetadataProcessed() {}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    // Called after the MTC Metadata was processed. This may be called after
    // the update was successfully configured or if the update was ignored due
    // to an error.
    virtual void OnMtcMetadataProcessed() {}
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

  // Loads PKI metadata from `install_dir_` on a background task and updates
  // the network service and (unless `network_service_only` is true) the cert
  // verifier service factory. This is a no-op if `install_dir_` is empty.
  void ConfigurePKIMetadata(bool network_service_only = false);

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

  // If set to true, allows older CT log list updates to override newer CT log
  // list updates.
  void AllowOldCTUpdateForTesting(bool allowed);

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  [[nodiscard]] bool WriteCRSDataForTesting(const base::FilePath& path,
                                            const std::string& contents);
  [[nodiscard]] bool WriteMtcMetadataForTesting(const base::FilePath& path,
                                                const std::string& contents);
  [[nodiscard]] bool WriteSignerSetDataForTesting(const base::FilePath& path,
                                                  const std::string& contents);
#endif

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  struct LoadedPKIMetadata {
    LoadedPKIMetadata();
    ~LoadedPKIMetadata();
    LoadedPKIMetadata(LoadedPKIMetadata&&) noexcept;
    LoadedPKIMetadata& operator=(LoadedPKIMetadata&&) noexcept;

    bool network_service_only = false;
#if BUILDFLAG(IS_CT_SUPPORTED)
    std::string ct_config_bytes;
#endif
#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
    std::string kp_config_bytes;
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    std::optional<mojo_base::ProtoWrapper> chrome_root_store;
    std::optional<mojo_base::ProtoWrapper> mtc_config;
#endif
  };

  // Processes all loaded PKIMetadata on the UI thread, sending updates to the
  // network service and (unless `data.network_service_only` is true) to
  // `CertVerifierServiceFactory::UpdatePKIMetadata`.
  void UpdatePKIMetadataOnUI(LoadedPKIMetadata data);

#if BUILDFLAG(IS_CT_SUPPORTED)
  // Updates the network service and (if `!network_service_only`) populates
  // `cert_verifier_update.ct_config` with the component delivered CT data.
  // `ct_config_bytes` should be a serialized CTConfig proto message.
  // TODO(crbug.com/41392053): remove network service from comment once it is
  // only sent to the cert verifier service.
  void ProcessCTLogListOnUI(
      const std::string& ct_config_bytes,
      bool network_service_only,
      cert_verifier::mojom::PKIMetadataUpdate& cert_verifier_update,
      base::OnceClosure done_callback);
#endif

#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
  // Updates the network service and (if `!network_service_only`) populates
  // `cert_verifier_update.key_pins` with the component delivered pins list
  // data. `kp_config_bytes` should be a serialized PinList proto message.
  // TODO(crbug.com/497882860): remove network service from comment once it is
  // only sent to the cert verifier service.
  void ProcessKeyPinsListOnUI(
      const std::string& kp_config_bytes,
      bool network_service_only,
      cert_verifier::mojom::PKIMetadataUpdate& cert_verifier_update,
      base::OnceClosure done_callback);
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  static std::optional<mojo_base::ProtoWrapper> ParseChromeRootStore(
      const base::FilePath& crs_pb_path);
  static std::optional<chrome_root_store::MtcConfig> ParseMtcConfig(
      const base::FilePath& mtc_config_pb_path);

  // Updates SystemNetworkContextManager Trust Anchor IDs and populates
  // `cert_verifier_update.chrome_root_store` with the component delivered
  // Chrome Root Store data and optional Mtc Config data.
  void ProcessChromeRootStoreOnUI(
      std::optional<mojo_base::ProtoWrapper> chrome_root_store,
      std::optional<mojo_base::ProtoWrapper> mtc_config,
      cert_verifier::mojom::PKIMetadataUpdate& cert_verifier_update);

  // Updates SystemNetworkContextManager and cert verifiers with the component
  // delivered MTC Metadata. `mtc_metadata` should be a wrapped
  // chrome_root_store::MtcMetadata proto message.
  void UpdateMtcMetadataOnUI(
      std::optional<mojo_base::ProtoWrapper> mtc_metadata);

  // Returns the Trust Anchor IDs with trusted landmarks, if available.
  std::optional<SSLConfigServiceMtcLandmarkInfo>
  CalculateTrustAnchorIdsWithLandmarks();

  // Updates the network service with the Trust Anchor IDs, combining the
  // cached data from both the Chrome Root Store and the MTC Metadata.
  // (https://tlswg.org/tls-trust-anchor-ids/draft-ietf-tls-trust-anchor-ids.html)
  void UpdateTrustAnchorIDsImpl();

  // Updates the cached TAI data with the Trust Anchor IDs in
  // `chrome_root_store`. Returns true if updated.
  bool UpdateCRSTrustAnchorIDs(
      const mojo_base::ProtoWrapper& chrome_root_store);
  // Updates the cached TAI data with the Trust Anchor IDs in
  // `mtc_config`. Returns true if updated.
  bool UpdateSignerSetTrustAnchorIDs(const mojo_base::ProtoWrapper& mtc_config);

  // Updates the network service with the Trust Anchor IDs in `mtc_metadata`,
  // and returns true on success. A false returns indicates a problem with the
  // protobuffer data (such as out-of-date metadata, indicating the metadata
  // should not be sent to the CertVerifierServiceFactory either.)
  bool UpdateMtcMetadataTrustAnchorIDs(
      const mojo_base::ProtoWrapper& mtc_metadata);

  // Notifies all observers that the MTC Metadata has been processed.
  void NotifyMtcMetadataProcessed();
#endif

  // Notifies all observers that the PKI metadata component data has been
  // processed.
  void NotifyPKIMetadataProcessed();

  // The install folder path. An empty path if the component is not ready.
  base::FilePath install_dir_;
  base::FilePath fastpush_install_dir_;
  base::ObserverList<Observer> observers_;

  // Cached TAI data from the last component updates (or initialized from the
  // compiled in data if each component hasn't updated yet). Some of the data
  // from both the CRS and MTC Metadata need to be merged together but are
  // updated separately, so the latest necessary data from each is cached.
  std::vector<std::vector<uint8_t>> crs_trust_anchor_ids_;
  absl::flat_hash_set<std::vector<uint8_t>> crs_trusted_mtc_ca_ids_;

  struct MtcCaIdAndLandmarkTrustAnchorIds {
    MtcCaIdAndLandmarkTrustAnchorIds();
    ~MtcCaIdAndLandmarkTrustAnchorIds();
    MtcCaIdAndLandmarkTrustAnchorIds(const MtcCaIdAndLandmarkTrustAnchorIds&);
    MtcCaIdAndLandmarkTrustAnchorIds(
        MtcCaIdAndLandmarkTrustAnchorIds&&) noexcept;
    MtcCaIdAndLandmarkTrustAnchorIds& operator=(
        const MtcCaIdAndLandmarkTrustAnchorIds&);
    MtcCaIdAndLandmarkTrustAnchorIds& operator=(
        MtcCaIdAndLandmarkTrustAnchorIds&&) noexcept;

    std::vector<uint8_t> ca_id;
    std::vector<std::vector<uint8_t>> landmark_trust_anchor_ids;
  };
  std::vector<MtcCaIdAndLandmarkTrustAnchorIds>
      mtc_ca_id_landmark_trust_anchor_ids_;

  // The time after which the MtcMetadata landmark info is no longer useful.
  base::Time mtc_landmark_max_usable_time_;

  bool allow_old_ct_log_list_updates_for_testing_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PKIMetadataComponentInstallerService> weak_factory_{
      this};
};

void MaybeRegisterPKIMetadataComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_COMPONENT_INSTALLER_H_
