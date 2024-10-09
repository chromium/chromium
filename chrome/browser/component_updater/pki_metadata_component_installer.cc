// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/component_updater/pki_metadata_component_installer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/net/key_pinning.pb.h"
#include "content/public/browser/network_service_instance.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/key_pinning.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "components/certificate_transparency/certificate_transparency.pb.h"
#include "components/certificate_transparency/certificate_transparency_config.pb.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/base/proto_wrapper_passkeys.h"
#endif

using component_updater::ComponentUpdateService;

namespace {

// This is the last version of CT log lists that this version of Chrome will
// accept. If a list is delivered with a compatibility version higher than this,
// it will be ignored (though the emergency disable flag will still be followed
// if it is set). This should never be decreased since that will cause CT
// enforcement to eventually stop. This should also only be increased if Chrome
// is compatible with the version it is being incremented to.
const uint64_t kMaxSupportedCTCompatibilityVersion = 2;

// This is the last version of key pins lists that this version of Chrome will
// accept. If a list is delivered with a compatibility version higher than this,
// it will be ignored. This should never be decreased since that will cause key
// pinning enforcement to eventually stop. This should also only be increased if
// Chrome is compatible with the version it is being incremented to.
const uint64_t kMaxSupportedKPCompatibilityVersion = 1;

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: efniojlnjndmcbiieegkicadnoecjjef
const uint8_t kPKIMetadataPublicKeySHA256[32] = {
    0x45, 0xd8, 0xe9, 0xbd, 0x9d, 0x3c, 0x21, 0x88, 0x44, 0x6a, 0x82,
    0x03, 0xde, 0x42, 0x99, 0x45, 0x66, 0x25, 0xfe, 0xb3, 0xd1, 0xf8,
    0x11, 0x65, 0xb4, 0x6f, 0xd3, 0x1b, 0x21, 0x89, 0xbe, 0x9c};

const base::FilePath::CharType kCTConfigProtoFileName[] =
    FILE_PATH_LITERAL("ct_config.pb");

const base::FilePath::CharType kKPConfigProtoFileName[] =
    FILE_PATH_LITERAL("kp_pinslist.pb");

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
const base::FilePath::CharType kCRSProtoFileName[] =
    FILE_PATH_LITERAL("crs.pb");
constexpr char kChromeRootStoreProto[] = "chrome_root_store.RootStore";
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

}  // namespace

namespace component_updater {

// PKIMetadataComponentInstallerService:

// static
PKIMetadataComponentInstallerService*
PKIMetadataComponentInstallerService::GetInstance() {
  static base::NoDestructor<PKIMetadataComponentInstallerService> instance;
  return instance.get();
}

PKIMetadataComponentInstallerService::PKIMetadataComponentInstallerService() =
    default;

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
                  base::as_bytes(base::make_span(file_contents)),
                  kChromeRootStoreProto,
                  mojo_base::ProtoWrapperBytes::GetPassKey());
            }
            return std::nullopt;
          },
          install_dir_.Append(kCRSProtoFileName)),
      base::BindOnce(
          &PKIMetadataComponentInstallerService::UpdateChromeRootStoreOnUI,
          weak_factory_.GetWeakPtr()));
#endif
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
void PKIMetadataComponentInstallerService::UpdateChromeRootStoreOnUI(
    std::optional<mojo_base::ProtoWrapper> chrome_root_store) {
  if (chrome_root_store.has_value()) {
    content::GetCertVerifierServiceFactory()->UpdateChromeRootStore(
        std::move(chrome_root_store.value()),
        base::BindOnce(&PKIMetadataComponentInstallerService::
                           NotifyChromeRootStoreConfigured,
                       weak_factory_.GetWeakPtr()));
  }
}

void PKIMetadataComponentInstallerService::NotifyChromeRootStoreConfigured() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnChromeRootStoreConfigured();
  }
}

bool PKIMetadataComponentInstallerService::WriteCRSDataForTesting(
    const base::FilePath& path,
    const std::string& contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  install_dir_ = path;
  return base::WriteFile(path.Append(kCRSProtoFileName), contents);
}
#endif

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
  if (base::FeatureList::IsEnabled(features::kKeyPinningComponentUpdater)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&LoadBinaryProtoFromDisk,
                       install_dir_.Append(kKPConfigProtoFileName)),
        base::BindOnce(&PKIMetadataComponentInstallerService::
                           UpdateNetworkServiceKPListOnUI,
                       weak_factory_.GetWeakPtr()));
  }
}

void PKIMetadataComponentInstallerService::OnComponentReady(
    base::FilePath install_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  install_dir_ = install_dir;
  ReconfigureAfterNetworkRestart();
  ConfigureChromeRootStore();
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
    // TODO(crbug.com/41392053): when CT enforcement is moved to the cert
    // verifier service, the killswitch also needs to be moved to the cert
    // verifier service.
    network_service->SetCtEnforcementEnabled(
        false,
        base::BindOnce(
            &PKIMetadataComponentInstallerService::NotifyCTLogListConfigured,
            weak_factory_.GetWeakPtr()));
    return;
  }

  if (proto->log_list().compatibility_version() >
      kMaxSupportedCTCompatibilityVersion) {
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
  base::Time update_time =
      base::Time::UnixEpoch() +
      base::Seconds(proto->log_list().timestamp().seconds()) +
      base::Nanoseconds(proto->log_list().timestamp().nanos());
  content::GetCertVerifierServiceFactory()->UpdateCtLogList(
      std::move(log_list_mojo), update_time, done_callback);
  network_service->UpdateCtLogList(
      std::move(log_list_mojo_clone_network_service), done_callback);

  // Send the updated popular SCTs list to the network service, if available.
  std::vector<std::vector<uint8_t>> popular_scts =
      component_updater::PKIMetadataComponentInstallerPolicy::
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

  network::mojom::PinListPtr pinlist_ptr = network::mojom::PinList::New();

  for (const auto& pinset : proto->pinsets()) {
    network::mojom::PinSetPtr pinset_ptr = network::mojom::PinSet::New();
    pinset_ptr->name = pinset.name();
    pinset_ptr->static_spki_hashes =
        component_updater::PKIMetadataComponentInstallerPolicy::
            BytesArrayFromProtoBytes(pinset.static_spki_hashes_sha256());
    pinset_ptr->bad_static_spki_hashes =
        component_updater::PKIMetadataComponentInstallerPolicy::
            BytesArrayFromProtoBytes(pinset.bad_static_spki_hashes_sha256());
    pinset_ptr->report_uri = pinset.report_uri();
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

  base::Time update_time = base::Time::UnixEpoch() +
                           base::Seconds(proto->timestamp().seconds()) +
                           base::Nanoseconds(proto->timestamp().nanos());

  network_service->UpdateKeyPinsList(std::move(pinlist_ptr), update_time);
}

void PKIMetadataComponentInstallerService::NotifyCTLogListConfigured() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnCTLogListConfigured();
  }
}

// PKIMetadataComponentInstallerPolicy:

PKIMetadataComponentInstallerPolicy::PKIMetadataComponentInstallerPolicy() =
    default;

PKIMetadataComponentInstallerPolicy::~PKIMetadataComponentInstallerPolicy() =
    default;

// static
std::vector<std::vector<uint8_t>>
PKIMetadataComponentInstallerPolicy::BytesArrayFromProtoBytes(
    google::protobuf::RepeatedPtrField<std::string> proto_bytes) {
  std::vector<std::vector<uint8_t>> bytes;
  bytes.reserve(proto_bytes.size());
  base::ranges::transform(
      proto_bytes, std::back_inserter(bytes), [](std::string element) {
        const auto bytes =
            base::as_bytes(base::make_span(element.data(), element.length()));
        return std::vector<uint8_t>(bytes.begin(), bytes.end());
      });
  return bytes;
}

bool PKIMetadataComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool PKIMetadataComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
PKIMetadataComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& /* install_dir */) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void PKIMetadataComponentInstallerPolicy::OnCustomUninstall() {}

void PKIMetadataComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict /* manifest */) {
  PKIMetadataComponentInstallerService::GetInstance()->OnComponentReady(
      install_dir);
}

// Called during startup and installation before ComponentReady().
bool PKIMetadataComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& install_dir) const {
  if (!base::PathExists(install_dir)) {
    return false;
  }

  return true;
}

base::FilePath PKIMetadataComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("PKIMetadata"));
}

void PKIMetadataComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPKIMetadataPublicKeySHA256),
               std::end(kPKIMetadataPublicKeySHA256));
}

std::string PKIMetadataComponentInstallerPolicy::GetName() const {
  return "PKI Metadata";
}

update_client::InstallerAttributes
PKIMetadataComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void MaybeRegisterPKIMetadataComponent(ComponentUpdateService* cus) {
  bool should_install =
      base::FeatureList::IsEnabled(features::kKeyPinningComponentUpdater);

#if BUILDFLAG(IS_CT_SUPPORTED)
  should_install |= base::FeatureList::IsEnabled(
      features::kCertificateTransparencyAskBeforeEnabling);
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  // If Chrome Root Store is supported, always install the component.
  // Note that if CRS is supported but optional, the CRS setting can change
  // during runtime based on the enterprise policy, so we still have to install
  // the component now so that CRS updates will be processed in case we need
  // them later. (Might be possible to refactor to only install component later
  // when it's needed and if it's not already installed? Probably not worth the
  // trouble though since CRS being optional is only a temporary state.)
  // Note: On Android CRS will continue to be optional in code since chrome
  // browser and webview use the same binary, but eventually it will just be
  // unconditionally enabled in chrome and disabled in webview. This component
  // is not registered in webview so setting it to always install here isn't a
  // problem.
  should_install = true;
#endif

  if (!should_install) {
    return;
  }

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<PKIMetadataComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
