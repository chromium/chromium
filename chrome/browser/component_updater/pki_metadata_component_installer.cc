// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pki_metadata_component_installer.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_service_buildflags.h"

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "components/certificate_transparency/certificate_transparency.pb.h"
#include "components/certificate_transparency/certificate_transparency_config.pb.h"
#include "components/certificate_transparency/ct_features.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#endif

using component_updater::ComponentUpdateService;

namespace {

const char kGoogleOperatorName[] = "Google";

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: efniojlnjndmcbiieegkicadnoecjjef
const uint8_t kPKIMetadataPublicKeySHA256[32] = {
    0x45, 0xd8, 0xe9, 0xbd, 0x9d, 0x3c, 0x21, 0x88, 0x44, 0x6a, 0x82,
    0x03, 0xde, 0x42, 0x99, 0x45, 0x66, 0x25, 0xfe, 0xb3, 0xd1, 0xf8,
    0x11, 0x65, 0xb4, 0x6f, 0xd3, 0x1b, 0x21, 0x89, 0xbe, 0x9c};

const base::FilePath::CharType kCTConfigProtoFileName[] =
    FILE_PATH_LITERAL("ct_config.pb");

std::string LoadCTBinaryProtoFromDisk(const base::FilePath& pb_path) {
  std::string result;
  if (pb_path.empty())
    return result;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  if (!base::ReadFileToString(pb_path.Append(kCTConfigProtoFileName),
                              &result)) {
    result.clear();
  }
  return result;
}

}  // namespace

namespace component_updater {

PKIMetadataComponentInstallerPolicy::PKIMetadataComponentInstallerPolicy() =
    default;

PKIMetadataComponentInstallerPolicy::~PKIMetadataComponentInstallerPolicy() =
    default;

bool PKIMetadataComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool PKIMetadataComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
PKIMetadataComponentInstallerPolicy::OnCustomInstall(
    const base::Value& /* manifest */,
    const base::FilePath& /* install_dir */) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void PKIMetadataComponentInstallerPolicy::OnCustomUninstall() {}

void PKIMetadataComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value /* manifest */) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&LoadCTBinaryProtoFromDisk, install_dir),
      base::BindOnce(
          &PKIMetadataComponentInstallerPolicy::UpdateNetworkServiceOnUI,
          base::Unretained(this)));
}

// Called during startup and installation before ComponentReady().
bool PKIMetadataComponentInstallerPolicy::VerifyInstallation(
    const base::Value& /* manifest */,
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

void PKIMetadataComponentInstallerPolicy::UpdateNetworkServiceOnUI(
    const std::string& ct_config_bytes) {
#if BUILDFLAG(IS_CT_SUPPORTED)
  auto proto =
      std::make_unique<chrome_browser_certificate_transparency::CTConfig>();
  if (!proto->ParseFromString(ct_config_bytes)) {
    return;
  }

  network::mojom::NetworkService* network_service =
      content::GetNetworkService();

  if (proto->disable_ct_enforcement()) {
    network_service->SetCtEnforcementEnabled(false);
    return;
  }

  std::vector<network::mojom::CTLogInfoPtr> log_list_mojo;

  // The log list shipped via component updater is a single message of CTLogList
  // type, as defined in
  // components/certificate_transparency/certificate_transparency.proto, the
  // included logs are of the CTLog type, but include only the information
  // required by Chrome to enforce its CT policy. Non Chrome used fields are
  // left unset.
  for (auto log : proto->log_list().logs()) {
    std::string decoded_key;
    if (!base::Base64Decode(log.key(), &decoded_key)) {
      continue;
    }
    network::mojom::CTLogInfoPtr log_ptr = network::mojom::CTLogInfo::New();
    log_ptr->name = log.description();
    log_ptr->public_key = decoded_key;
    // Operator history is ordered in inverse chronological order, so the 0th
    // element will be the current operator.
    if (!log.operator_history().empty()) {
      if (log.operator_history().Get(0).name() == kGoogleOperatorName) {
        log_ptr->operated_by_google = true;
      }
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
          base::TimeDelta end_time =
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
        base::TimeDelta retired_since =
            base::Seconds(log.state()[0].state_start().seconds()) +
            base::Nanoseconds(log.state()[0].state_start().nanos());
        log_ptr->disqualified_at = retired_since;
      }
    }
    log_list_mojo.push_back(std::move(log_ptr));
  }

  base::Time update_time =
      base::Time::UnixEpoch() +
      base::Seconds(proto->log_list().timestamp().seconds()) +
      base::Nanoseconds(proto->log_list().timestamp().nanos());
  network_service->UpdateCtLogList(std::move(log_list_mojo), update_time);
#endif  // BUILDFLAG(IS_CT_SUPPORTED)
}

void MaybeRegisterPKIMetadataComponent(ComponentUpdateService* cus) {
  // Currently the component is only used for the CT log list, so we no-op if CT
  // is not supported.
#if BUILDFLAG(IS_CT_SUPPORTED)
  if (!base::FeatureList::IsEnabled(
          certificate_transparency::features::
              kCertificateTransparencyComponentUpdater)) {
    return;
  }
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<PKIMetadataComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
#endif  // BUILDFLAG(IS_CT_SUPPORTED)
}

}  // namespace component_updater
