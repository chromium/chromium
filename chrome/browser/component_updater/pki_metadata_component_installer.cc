// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pki_metadata_component_installer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/component_updater/pki_metadata_component_installer_policy.h"
#include "chrome/browser/component_updater/pki_metadata_fastpush_component_installer_policy.h"
#include "chrome/browser/net/key_pinning.pb.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/key_pinning.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "components/certificate_transparency/certificate_transparency.pb.h"
#include "components/certificate_transparency/certificate_transparency_config.pb.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/base/proto_wrapper_passkeys.h"
#include "net/cert/internal/trust_store_chrome.h"
#endif

#if BUILDFLAG(INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST)
#include "net/http/transport_security_state.h"
#endif

namespace {

// This is the last version of CT log lists that this version of Chrome will
// accept. If a list is delivered with a compatibility version higher than this,
// it will be ignored (though the emergency disable flag will still be followed
// if it is set). This should never be decreased since that will cause CT
// enforcement to eventually stop. This should also only be increased if Chrome
// is compatible with the version it is being incremented to.
const uint64_t kMaxSupportedCTCompatibilityVersion = 4;

// This is the last version of key pins lists that this version of Chrome will
// accept. If a list is delivered with a compatibility version higher than this,
// it will be ignored. This should never be decreased since that will cause key
// pinning enforcement to eventually stop. This should also only be increased if
// Chrome is compatible with the version it is being incremented to.
const uint64_t kMaxSupportedKPCompatibilityVersion = 1;

// Ignore any MtcMetadata component update data that is older than this amount.
// The MTC Metadata has a short useful lifetime, and since it impacts Trust
// Anchor ID data that is sent over the wire, using a stale update would just
// result in sending useless data for TAIs that don't work anymore.
constexpr base::TimeDelta kMaxMtcMetadataAge = base::Days(7);

const base::FilePath::CharType kCTConfigProtoFileName[] =
    FILE_PATH_LITERAL("ct_config.pb");

const base::FilePath::CharType kKPConfigProtoFileName[] =
    FILE_PATH_LITERAL("kp_pinslist.pb");

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
const base::FilePath::CharType kCRSProtoFileName[] =
    FILE_PATH_LITERAL("crs.pb");
constexpr char kChromeRootStoreProto[] = "chrome_root_store.RootStore";
const base::FilePath::CharType kMtcMetadataProtoFileName[] =
    FILE_PATH_LITERAL("mtc_metadata.pb");
constexpr char kMtcMetadataProto[] = "chrome_root_store.MtcMetadata";
#endif

std::string LoadBinaryProtoFromDisk(const base::FilePath& pb_path) {
  std::string result;
  if (pb_path.empty()) {
    return result;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  if (!base::ReadFileToString(pb_path, &result)) {
    result.clear();
  }
  return result;
}

// Ideally we'd use EnumTraits for this method, but the conversion is only done
// once here so it's not worth it.
network::mojom::CTLogInfo::LogType ProtoLogTypeToLogType(
    ::chrome_browser_certificate_transparency::CTLog_LogType log_type) {
  switch (log_type) {
    case ::chrome_browser_certificate_transparency::CTLog::LOG_TYPE_UNSPECIFIED:
      return network::mojom::CTLogInfo::LogType::kUnspecified;
    case ::chrome_browser_certificate_transparency::CTLog::RFC6962:
      return network::mojom::CTLogInfo::LogType::kRFC6962;
    case ::chrome_browser_certificate_transparency::CTLog::STATIC_CT_API:
      return network::mojom::CTLogInfo::LogType::kStaticCTAPI;
    default:
      NOTREACHED();
  }
}

// Converts a protobuf repeated bytes array to an array of uint8_t arrays.
std::vector<std::vector<uint8_t>> BytesArrayFromProtoBytes(
    const google::protobuf::RepeatedPtrField<std::string>& proto_bytes) {
  std::vector<std::vector<uint8_t>> bytes;
  bytes.reserve(proto_bytes.size());
  std::ranges::transform(
      proto_bytes, std::back_inserter(bytes), [](const std::string& element) {
        const auto bytes = base::as_byte_span(element);
        return std::vector<uint8_t>(bytes.begin(), bytes.end());
      });
  return bytes;
}

// Converts a protobuf repeated bytes array to an array of SHA256HashValues.
// Elements in `proto_bytes` that are not 32 bytes long are silently ignored.
std::vector<net::SHA256HashValue> SHA256HashValueArrayFromProtoBytes(
    const google::protobuf::RepeatedPtrField<std::string>& proto_bytes) {
  std::vector<net::SHA256HashValue> hashes;
  hashes.reserve(proto_bytes.size());
  for (const auto& proto_hash : proto_bytes) {
    if (proto_hash.size() == crypto::hash::kSha256Size) {
      base::span(hashes.emplace_back())
          .copy_from(base::as_byte_span(proto_hash));
    }
  }
  return hashes;
}

std::vector<uint8_t> MakeTrustAnchorIdForRange(
    base::span<const uint8_t> base_id,
    uint64_t last_landmark) {
  constexpr size_t kMaxBase128Uint64Size = 10;
  bssl::ScopedCBB cbb;
  CHECK(CBB_init(cbb.get(),
                 /*initial_capacity=*/base_id.size() + kMaxBase128Uint64Size) &&
        CBB_add_bytes(cbb.get(), base_id.data(), base_id.size()) &&
        CBB_add_asn1_oid_component(cbb.get(), last_landmark) &&
        CBB_flush(cbb.get()));

  // SAFETY: CBB_data(cbb) returns a pointer to the written data with length
  // CBB_len(cbb).
  return base::ToVector(UNSAFE_BUFFERS(
      base::span<const uint8_t>(CBB_data(cbb.get()), CBB_len(cbb.get()))));
}

std::string TaiToString(base::span<const uint8_t> trust_anchor_id) {
  CBS cbs;
  CBS_init(&cbs, trust_anchor_id.data(), trust_anchor_id.size());
  bssl::UniquePtr<char> text(CBS_asn1_relative_oid_to_text(&cbs));
  if (text) {
    return std::string(text.get());
  }
  return std::string();
}

}  // namespace

namespace component_updater {

// static
PKIMetadataComponentInstallerService*
PKIMetadataComponentInstallerService::GetInstance() {
  static base::NoDestructor<PKIMetadataComponentInstallerService> instance;
  return instance.get();
}

PKIMetadataComponentInstallerService::PKIMetadataComponentInstallerService() {
  // UpdateTrustAnchorIDsImpl() depends on TAI info from both the Chrome Root
  // Store and MTC Metadata protos. Since they are updated separately, we need
  // to initialize the data from the compiled in versions so that on
  // startup/first run the TAI data is calculated correctly regardless which
  // order and timing the components initialize in.
  crs_trust_anchor_ids_ =
      net::TrustStoreChrome::GetTrustAnchorIDsFromCompiledInRootStore();

  // TODO(crbug.com/452983502): Initialize crs_trusted_mtc_logids_ from
  // compiled-in CRS data once they are available. (MTC anchors are not
  // currently compiled into the binary, since they only work when component
  // updater is supplying the trusted subtrees, so the current MTC
  // implementation just depends on both components being loaded, for simplity.)
}

PKIMetadataComponentInstallerService::MtcLogIdAndLandmarkTrustAnchorId::
    MtcLogIdAndLandmarkTrustAnchorId() = default;
PKIMetadataComponentInstallerService::MtcLogIdAndLandmarkTrustAnchorId::
    ~MtcLogIdAndLandmarkTrustAnchorId() = default;
PKIMetadataComponentInstallerService::MtcLogIdAndLandmarkTrustAnchorId::
    MtcLogIdAndLandmarkTrustAnchorId(MtcLogIdAndLandmarkTrustAnchorId&&) =
        default;
PKIMetadataComponentInstallerService::MtcLogIdAndLandmarkTrustAnchorId::
    MtcLogIdAndLandmarkTrustAnchorId(
        const MtcLogIdAndLandmarkTrustAnchorId& other) = default;

void PKIMetadataComponentInstallerService::ConfigureChromeRootStore() {
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& pb_path)
              -> std::optional<mojo_base::ProtoWrapper> {
            std::string file_contents = LoadBinaryProtoFromDisk(pb_path);
            if (file_contents.size()) {
              return mojo_base::ProtoWrapper(
                  base::as_byte_span(file_contents), kChromeRootStoreProto,
                  mojo_base::ProtoWrapperBytes::GetPassKey());
            }
            return std::nullopt;
          },
          install_dir_.Append(kCRSProtoFileName)),
      base::BindOnce(
          &PKIMetadataComponentInstallerService::UpdateChromeRootStoreOnUI,
          weak_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
}

void PKIMetadataComponentInstallerService::ConfigureMtcMetadata() {
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  if (!base::FeatureList::IsEnabled(net::features::kVerifyMTCs)) {
    // If the feature flag is disabled, call the observer immediately. This is
    // slightly weird but allows tests to easily test both the enabled and
    // disabled state.
    NotifyMtcMetadataConfigured();
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& pb_path)
              -> std::optional<mojo_base::ProtoWrapper> {
            std::string file_contents = LoadBinaryProtoFromDisk(pb_path);
            if (file_contents.size()) {
              return mojo_base::ProtoWrapper(
                  base::as_byte_span(file_contents), kMtcMetadataProto,
                  mojo_base::ProtoWrapperBytes::GetPassKey());
            }
            return std::nullopt;
          },
          fastpush_install_dir_.Append(kMtcMetadataProtoFileName)),
      base::BindOnce(
          &PKIMetadataComponentInstallerService::UpdateMtcMetadataOnUI,
          weak_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
void PKIMetadataComponentInstallerService::UpdateChromeRootStoreOnUI(
    std::optional<mojo_base::ProtoWrapper> chrome_root_store) {
  if (chrome_root_store.has_value()) {
    UpdateCRSTrustAnchorIDs(chrome_root_store.value());
    content::GetCertVerifierServiceFactory()->UpdateChromeRootStore(
        std::move(chrome_root_store.value()),
        base::BindOnce(&PKIMetadataComponentInstallerService::
                           NotifyChromeRootStoreConfigured,
                       weak_factory_.GetWeakPtr()));
  }
}

void PKIMetadataComponentInstallerService::UpdateMtcMetadataOnUI(
    std::optional<mojo_base::ProtoWrapper> mtc_metadata) {
  if (!mtc_metadata.has_value()) {
    return;
  }
  if (!UpdateMtcMetadataTrustAnchorIDs(mtc_metadata.value())) {
    // If the Metadata was out of date, don't bother sending it to the cert
    // verifier service either.
    NotifyMtcMetadataConfigured();
    return;
  }
  content::GetCertVerifierServiceFactory()->UpdateMtcMetadata(
      std::move(mtc_metadata.value()),
      base::BindOnce(
          &PKIMetadataComponentInstallerService::NotifyMtcMetadataConfigured,
          weak_factory_.GetWeakPtr()));
}

void PKIMetadataComponentInstallerService::UpdateTrustAnchorIDsImpl() {
  // Start with trust anchor ids of the CRS trusted anchors.
  std::vector<std::vector<uint8_t>> trust_anchor_ids = crs_trust_anchor_ids_;

  std::vector<std::vector<uint8_t>> mtc_trust_anchor_ids;
  if (base::FeatureList::IsEnabled(net::features::kVerifyMTCs)) {
    // Add trust anchor ids for MTC trusted subtrees.
    //
    // Intersect the trusted subtree anchors log_ids from fastpush, with the MTC
    // trust anchor log_ids, and add these subtree TAIs to trust_anchor_ids.
    //
    // The intersection is necessary since the components update on different
    // schedules, so it's possible to have the trusted subtrees for a MTC
    // anchor that isn't trusted in the Chrome Root Store (or vice versa, but
    // that doesn't matter here).
    // A site using such a subtree will not actually be trusted unless the
    // matching anchor is present in the CRS, so advertising support for it in
    // TAI would lead to asking sites to send certs we can't actually verify.
    for (const auto& signatureless_tai :
         mtc_log_id_landmark_trust_anchor_ids_) {
      if (crs_trusted_mtc_logids_.contains(signatureless_tai.anchor_log_id)) {
        DVLOG(1) << "using signatureless TAI "
                 << TaiToString(signatureless_tai.landmark_trust_anchor_id)
                 << " for trusted MTC Anchor log_id="
                 << TaiToString(signatureless_tai.anchor_log_id);
        mtc_trust_anchor_ids.push_back(
            signatureless_tai.landmark_trust_anchor_id);
      } else {
        DVLOG(1) << "ignoring signatureless TAI "
                 << TaiToString(signatureless_tai.landmark_trust_anchor_id)
                 << " as no trusted MTC Anchor found with log_id="
                 << TaiToString(signatureless_tai.anchor_log_id);
      }
    }
  }

  SystemNetworkContextManager* network_context_manager =
      SystemNetworkContextManager::GetInstance();
  CHECK(network_context_manager);
  network_context_manager->UpdateTrustAnchorIDs(
      std::move(trust_anchor_ids), std::move(mtc_trust_anchor_ids));
}

void PKIMetadataComponentInstallerService::UpdateCRSTrustAnchorIDs(
    const mojo_base::ProtoWrapper& chrome_root_store) {
  auto message = chrome_root_store.As<chrome_root_store::RootStore>();
  if (!message.has_value()) {
    LOG(ERROR) << "error parsing proto for Chrome Root Store";
    return;
  }
  if (message->version_major() <= net::CompiledChromeRootStoreVersion()) {
    return;
  }

  std::vector<std::vector<uint8_t>> crs_trust_anchor_ids;
  for (const auto& anchor : message->trust_anchors()) {
    if (anchor.has_trust_anchor_id()) {
      crs_trust_anchor_ids.emplace_back(
          base::ToVector(base::as_byte_span(anchor.trust_anchor_id())));
    }
  }
  for (const auto& additional_cert : message->additional_certs()) {
    if (additional_cert.has_trust_anchor_id() &&
        additional_cert.tls_trust_anchor()) {
      crs_trust_anchor_ids.emplace_back(base::ToVector(
          base::as_byte_span(additional_cert.trust_anchor_id())));
    }
  }

  absl::flat_hash_set<std::vector<uint8_t>> crs_trusted_mtc_logids;
  if (base::FeatureList::IsEnabled(net::features::kVerifyMTCs)) {
    for (const auto& mtc_anchor : message->mtc_anchors()) {
      if (mtc_anchor.tls_trust_anchor()) {
        crs_trusted_mtc_logids.insert(
            base::ToVector(base::as_byte_span(mtc_anchor.log_id())));
        // TODO(crbug.com/452983502): once signatureful MTCs are supported, we
        // should add the log ids for trusted signatureful `mtc_anchors()` to
        // the Trust Anchor IDs that we send. This probably needs to be a
        // different member than `crs_trust_anchor_ids` if we want to have them
        // end up in the `mtc_trust_anchor_ids` config.
        //
        // (The trust anchor ids for signatureless MTCs are handled by
        // UpdateMtcMetadataTrustAnchorIDs.)
      }
    }
  }

  crs_trust_anchor_ids_ = std::move(crs_trust_anchor_ids);
  crs_trusted_mtc_logids_ = std::move(crs_trusted_mtc_logids);

  UpdateTrustAnchorIDsImpl();
}

bool PKIMetadataComponentInstallerService::UpdateMtcMetadataTrustAnchorIDs(
    const mojo_base::ProtoWrapper& mtc_metadata) {
  auto message = mtc_metadata.As<chrome_root_store::MtcMetadata>();
  if (!message.has_value()) {
    LOG(ERROR) << "error parsing proto for MtcMetadata";
    return false;
  }

  // TODO(crbug.com/452986180): should the out-of-date check use the network
  // time rather than system time?
  //
  // TODO(crbug.com/452986180): This check prevents the component updater from
  // loading out-of-date MTC metadata, but there is nothing to stop already
  // loaded metadata from continuing to be used if it becomes out of date
  // without a new component update being received. Should there be something
  // to stop using existing data that becomes out of date if a new component
  // update hasn't been received to replace it?  (Aside from restarting the
  // browser.)
  //
  // Ignore out-of-data component data.
  // (MtcMetadata is not compiled into the binary, so there doesn't need to be
  // a check against any compiled-in data's update_time like there is for other
  // component updaters here.)
  if (!message->has_update_time_seconds() ||
      base::Time::UnixEpoch() + base::Seconds(message->update_time_seconds()) <
          base::Time::Now() - kMaxMtcMetadataAge) {
    DVLOG(1) << "ignored out of date MtcMetadata";
    return false;
  }

  std::vector<MtcLogIdAndLandmarkTrustAnchorId>
      mtc_log_id_signatureless_trust_anchor_ids;

  for (const auto& anchor_data : message->mtc_anchor_data()) {
    if (!anchor_data.has_log_id() ||
        !anchor_data.has_trusted_landmark_ids_range()) {
      LOG(ERROR) << "ignored invalid MtcAnchorData";
      continue;
    }
    const auto& tai_range = anchor_data.trusted_landmark_ids_range();
    if (!tai_range.has_base_id() ||
        !tai_range.has_min_active_landmark_inclusive() ||
        !tai_range.has_last_landmark_inclusive()) {
      LOG(ERROR) << "ignored invalid MtcAnchorData";
      continue;
    }
    MtcLogIdAndLandmarkTrustAnchorId tai_entry;
    tai_entry.anchor_log_id =
        base::ToVector(base::as_byte_span(anchor_data.log_id()));
    tai_entry.landmark_trust_anchor_id =
        MakeTrustAnchorIdForRange(base::as_byte_span(tai_range.base_id()),
                                  tai_range.last_landmark_inclusive());
    mtc_log_id_signatureless_trust_anchor_ids.push_back(std::move(tai_entry));
  }

  mtc_log_id_landmark_trust_anchor_ids_ =
      std::move(mtc_log_id_signatureless_trust_anchor_ids);

  UpdateTrustAnchorIDsImpl();
  return true;
}

void PKIMetadataComponentInstallerService::NotifyChromeRootStoreConfigured() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnChromeRootStoreConfigured();
  }
}

void PKIMetadataComponentInstallerService::NotifyMtcMetadataConfigured() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnMtcMetadataConfigured();
  }
}

bool PKIMetadataComponentInstallerService::WriteCRSDataForTesting(
    const base::FilePath& path,
    const std::string& contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  install_dir_ = path;
  return base::WriteFile(path.Append(kCRSProtoFileName), contents);
}

bool PKIMetadataComponentInstallerService::WriteMtcMetadataForTesting(
    const base::FilePath& path,
    const std::string& contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fastpush_install_dir_ = path;
  return base::WriteFile(path.Append(kMtcMetadataProtoFileName), contents);
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

void PKIMetadataComponentInstallerService::ReconfigureAfterNetworkRestart() {
  // Runs on UI thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (install_dir_.empty()) {
    return;
  }
  if (base::FeatureList::IsEnabled(
          features::kCertificateTransparencyAskBeforeEnabling)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&LoadBinaryProtoFromDisk,
                       install_dir_.Append(kCTConfigProtoFileName)),
        base::BindOnce(&PKIMetadataComponentInstallerService::
                           UpdateNetworkServiceCTListOnUI,
                       weak_factory_.GetWeakPtr()));
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&LoadBinaryProtoFromDisk,
                     install_dir_.Append(kKPConfigProtoFileName)),
      base::BindOnce(
          &PKIMetadataComponentInstallerService::UpdateNetworkServiceKPListOnUI,
          weak_factory_.GetWeakPtr()));
}

void PKIMetadataComponentInstallerService::OnComponentReady(
    base::FilePath install_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  install_dir_ = install_dir;
  ReconfigureAfterNetworkRestart();
  ConfigureChromeRootStore();
}

void PKIMetadataComponentInstallerService::OnFastpushComponentReady(
    base::FilePath install_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fastpush_install_dir_ = install_dir;
  ConfigureMtcMetadata();
}

bool PKIMetadataComponentInstallerService::WriteCTDataForTesting(
    const base::FilePath& path,
    const std::string& contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  install_dir_ = path;
  return base::WriteFile(path.Append(kCTConfigProtoFileName), contents);
}

void PKIMetadataComponentInstallerService::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void PKIMetadataComponentInstallerService::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void PKIMetadataComponentInstallerService::UpdateNetworkServiceCTListOnUI(
    const std::string& ct_config_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CT_SUPPORTED)
  if (ct_config_bytes.empty()) {
    // LoadBinaryProtoFromDisk returns an empty string if it fails to find
    // the file on disk or fails to read. An empty string is valid proto,
    // continuing to process such an empty string will result in stomping
    // on the default disqualified certs in the CT list allowing
    // disqualified certs to be trusted. Treat empty string as invalid proto
    // instead.
    return;
  }

  auto proto =
      std::make_unique<chrome_browser_certificate_transparency::CTConfig>();
  if (!proto->ParseFromString(ct_config_bytes)) {
    return;
  }

  network::mojom::NetworkService* network_service =
      content::GetNetworkService();

  if (proto->disable_ct_enforcement()) {
    // TODO(crbug.com/41392053): The disable_ct_enforcement kill switch is
    // used in both the network service and cert verifier service. Finish
    // refactoring so that it is only sent to cert verifier service.
    base::RepeatingClosure done_callback = BarrierClosure(
        /*num_closures=*/2,
        base::BindOnce(
            &PKIMetadataComponentInstallerService::NotifyCTLogListConfigured,
            weak_factory_.GetWeakPtr()));
    content::GetCertVerifierServiceFactory()->DisableCtEnforcement(
        done_callback);
    network_service->SetCtEnforcementEnabled(false, done_callback);
    return;
  }

  if (proto->log_list().compatibility_version() >
      kMaxSupportedCTCompatibilityVersion) {
    return;
  }

  base::Time proto_timestamp =
      base::Time::UnixEpoch() +
      base::Seconds(proto->log_list().timestamp().seconds()) +
      base::Nanoseconds(proto->log_list().timestamp().nanos());
  // Do not update the CT log list with the component data if it's older than
  // the built in list.
  if (proto_timestamp < certificate_transparency::GetLogListTimestamp()) {
    return;
  }

  // TODO(crbug.com/41392053): Log info needs to be sent to both network
  // service and cert verifier service. Finish refactoring so that it is only
  // sent to cert verifier service.
  std::vector<network::mojom::CTLogInfoPtr> log_list_mojo;
  std::vector<network::mojom::CTLogInfoPtr> log_list_mojo_clone_network_service;

  // The log list shipped via component updater is a single message of CTLogList
  // type, as defined in
  // components/certificate_transparency/certificate_transparency.proto, the
  // included logs are of the CTLog type, but include only the information
  // required by Chrome to enforce its CT policy. Non Chrome used fields are
  // left unset.
  for (const auto& log : proto->log_list().logs()) {
    std::string decoded_id;
    if (!base::Base64Decode(log.log_id(), &decoded_id)) {
      continue;
    }
    std::string decoded_key;
    if (!base::Base64Decode(log.key(), &decoded_key)) {
      continue;
    }
    network::mojom::CTLogInfoPtr log_ptr = network::mojom::CTLogInfo::New();
    log_ptr->id = std::move(decoded_id);
    log_ptr->name = log.description();
    log_ptr->public_key = std::move(decoded_key);
    // Operator history is ordered in inverse chronological order, so the 0th
    // element will be the current operator.
    if (!log.operator_history().empty()) {
      log_ptr->current_operator = log.operator_history().Get(0).name();
      if (log.operator_history().size() > 1) {
        // The protobuffer includes operator history in reverse chronological
        // order, but we need it in chronological order, so we iterate in
        // reverse (and ignoring the current operator).
        for (auto it = log.operator_history().rbegin();
             it != log.operator_history().rend() - 1; ++it) {
          network::mojom::PreviousOperatorEntryPtr previous_operator =
              network::mojom::PreviousOperatorEntry::New();
          previous_operator->name = it->name();
          // We use the next element's start time as the current element end
          // time.
          base::Time end_time =
              base::Time::UnixEpoch() +
              base::Seconds((it + 1)->operator_start().seconds()) +
              base::Nanoseconds((it + 1)->operator_start().nanos());
          previous_operator->end_time = end_time;
          log_ptr->previous_operators.push_back(std::move(previous_operator));
        }
      }
    }

    // State history is ordered in inverse chronological order, so the 0th
    // element will be the current state.
    if (!log.state().empty()) {
      const auto& state = log.state().Get(0);
      if (state.current_state() ==
          chrome_browser_certificate_transparency::CTLog_CurrentState_RETIRED) {
        // If the log was RETIRED, record the timestamp at which it was.
        // Note: RETIRED is a terminal state for the log, so other states do not
        // need to be checked, because once RETIRED, the state will never
        // change.
        base::Time retired_since =
            base::Time::UnixEpoch() +
            base::Seconds(log.state()[0].state_start().seconds()) +
            base::Nanoseconds(log.state()[0].state_start().nanos());
        log_ptr->disqualified_at = retired_since;
      }
    }

    log_ptr->mmd = base::Seconds(log.mmd_secs());
    log_ptr->log_type = ProtoLogTypeToLogType(log.log_type());
    log_list_mojo_clone_network_service.push_back(log_ptr.Clone());
    log_list_mojo.push_back(std::move(log_ptr));
  }

  // We need to wait for both CT log list updates and the popular SCT list
  // update.
  base::RepeatingClosure done_callback = BarrierClosure(
      /*num_closures=*/3,
      base::BindOnce(
          &PKIMetadataComponentInstallerService::NotifyCTLogListConfigured,
          weak_factory_.GetWeakPtr()));
  content::GetCertVerifierServiceFactory()->UpdateCtLogList(
      std::move(log_list_mojo), proto_timestamp, done_callback);
  network_service->UpdateCtLogList(
      std::move(log_list_mojo_clone_network_service), done_callback);

  // Send the updated popular SCTs list to the network service, if available.
  // TODO(crbug.com/41286522): should this also be vector<SHA256HashValue>?
  std::vector<std::vector<uint8_t>> popular_scts =
      BytesArrayFromProtoBytes(proto->popular_scts());
  network_service->UpdateCtKnownPopularSCTs(std::move(popular_scts),
                                            done_callback);
#endif  // BUILDFLAG(IS_CT_SUPPORTED)
}

void PKIMetadataComponentInstallerService::UpdateNetworkServiceKPListOnUI(
    const std::string& kp_config_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto proto = std::make_unique<chrome_browser_key_pinning::PinList>();
  if (!proto->ParseFromString(kp_config_bytes)) {
    return;
  }
  network::mojom::NetworkService* network_service =
      content::GetNetworkService();

  if (proto->compatibility_version() > kMaxSupportedKPCompatibilityVersion) {
    return;
  }

  base::Time proto_timestamp = base::Time::UnixEpoch() +
                               base::Seconds(proto->timestamp().seconds()) +
                               base::Nanoseconds(proto->timestamp().nanos());

#if BUILDFLAG(INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST)
  // Do not update the pins list with the component data if it's older than the
  // built in list.
  if (proto_timestamp <
      net::TransportSecurityState::GetBuiltInPinsListTimestamp()) {
    return;
  }
#endif  // BUILDFLAG(INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST)

  network::mojom::PinListPtr pinlist_ptr = network::mojom::PinList::New();

  for (const auto& pinset : proto->pinsets()) {
    network::mojom::PinSetPtr pinset_ptr = network::mojom::PinSet::New();
    pinset_ptr->name = pinset.name();
    pinset_ptr->static_spki_hashes =
        SHA256HashValueArrayFromProtoBytes(pinset.static_spki_hashes_sha256());
    pinset_ptr->bad_static_spki_hashes = SHA256HashValueArrayFromProtoBytes(
        pinset.bad_static_spki_hashes_sha256());
    pinlist_ptr->pinsets.push_back(std::move(pinset_ptr));
  }

  for (const auto& info : proto->host_pins()) {
    network::mojom::PinSetInfoPtr pininfo_ptr =
        network::mojom::PinSetInfo::New();
    pininfo_ptr->hostname = info.hostname();
    pininfo_ptr->pinset_name = info.pinset_name();
    pininfo_ptr->include_subdomains = info.include_subdomains();
    pinlist_ptr->host_pins.push_back(std::move(pininfo_ptr));
  }

  network_service->UpdateKeyPinsList(std::move(pinlist_ptr), proto_timestamp);
}

void PKIMetadataComponentInstallerService::NotifyCTLogListConfigured() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnCTLogListConfigured();
  }
}

// static
std::vector<std::vector<uint8_t>>
PKIMetadataComponentInstallerService::BytesArrayFromProtoBytesForTesting(
    const google::protobuf::RepeatedPtrField<std::string>& proto_bytes) {
  return BytesArrayFromProtoBytes(proto_bytes);
}

// static
std::vector<net::SHA256HashValue> PKIMetadataComponentInstallerService::
    SHA256HashValueArrayFromProtoBytesForTesting(
        const google::protobuf::RepeatedPtrField<std::string>& proto_bytes) {
  return SHA256HashValueArrayFromProtoBytes(proto_bytes);
}

void MaybeRegisterPKIMetadataComponent(ComponentUpdateService* cus) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<PKIMetadataComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  if (base::FeatureList::IsEnabled(net::features::kVerifyMTCs)) {
    auto fastpush_installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<PKIMetadataFastpushComponentInstallerPolicy>());
    fastpush_installer->Register(cus, base::OnceClosure());
  }
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
}

}  // namespace component_updater
