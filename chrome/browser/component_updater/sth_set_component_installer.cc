// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sth_set_component_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "components/certificate_transparency/sth_observer.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "crypto/sha2.h"
#include "net/cert/ct_log_response_parser.h"
#include "net/cert/signed_tree_head.h"
#include "services/network/network_service.h"
#include "services/service_manager/public/cpp/service.h"

using component_updater::ComponentUpdateService;

namespace {
const base::FilePath::CharType kSTHsDirName[] = FILE_PATH_LITERAL("sths");

network::mojom::NetworkService* g_network_service_for_testing = nullptr;

base::FilePath& GetInstallDir() {
  static base::NoDestructor<base::FilePath> install_dir;
  return *install_dir;
}

base::FilePath GetInstalledPath(const base::FilePath& base) {
  return base.Append(FILE_PATH_LITERAL("_platform_specific"))
      .Append(FILE_PATH_LITERAL("all"))
      .Append(kSTHsDirName);
}

// Loads the STHs from |sths_path|, which contains a series of files that
// are the in the form "[log_id].sth", where [log_id] is the hex-encoded ID
// of the log, and the contents are JSON STH. Parsed STHs will be posted to
// |callback| on |origin_task_runner|.
void LoadSTHsFromDisk(
    const base::FilePath& sths_path,
    scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
    base::RepeatingCallback<void(const net::ct::SignedTreeHead&)> callback) {
  base::FileEnumerator sth_file_enumerator(sths_path, false,
                                           base::FileEnumerator::FILES,
                                           FILE_PATH_LITERAL("*.sth"));
  base::FilePath sth_file_path;

  while (!(sth_file_path = sth_file_enumerator.Next()).empty()) {
    DVLOG(1) << "Reading STH from file: " << sth_file_path.value();

    const std::string log_id_hex =
        sth_file_path.BaseName().RemoveExtension().MaybeAsASCII();
    if (log_id_hex.empty()) {
      DVLOG(1) << "Error extracting log_id from: "
               << sth_file_path.BaseName().LossyDisplayName();
      continue;
    }

    std::vector<uint8_t> decoding_output;
    if (!base::HexStringToBytes(log_id_hex, &decoding_output)) {
      DVLOG(1) << "Failed to decode Log ID: " << log_id_hex;
      continue;
    }

    const std::string log_id(
        reinterpret_cast<const char*>(decoding_output.data()),
        decoding_output.size());

    std::string json_sth;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          base::BlockingType::MAY_BLOCK);
      if (!base::ReadFileToString(sth_file_path, &json_sth)) {
        DVLOG(1) << "Failed reading from " << sth_file_path.value();
        continue;
      }
    }

    DVLOG(1) << "STH: Successfully read: " << json_sth;

    int error_code = 0;
    std::string error_message;
    std::unique_ptr<base::Value> parsed_json =
        base::JSONReader::ReadAndReturnError(json_sth, base::JSON_PARSE_RFC,
                                             &error_code, &error_message);

    if (!parsed_json || error_code != base::JSONReader::JSON_NO_ERROR) {
      DVLOG(1) << "STH loading failed: " << error_message << " for log: "
               << base::HexEncode(log_id.data(), log_id.length());
      continue;
    }

    DVLOG(1) << "STH parsing success for log: "
             << base::HexEncode(log_id.data(), log_id.length());

    net::ct::SignedTreeHead signed_tree_head;
    if (!net::ct::FillSignedTreeHead(*parsed_json, &signed_tree_head)) {
      LOG(ERROR) << "Failed to fill in signed tree head.";
      continue;
    }

    // The log id is not a part of the response, fill in manually.
    signed_tree_head.log_id = log_id;
    origin_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(callback, signed_tree_head));
  }
}

// Indicates that a new STH has been loaded.
void OnSTHLoaded(const net::ct::SignedTreeHead& sth) {
  network::mojom::NetworkService* network_service =
      g_network_service_for_testing ? g_network_service_for_testing
                                    : content::GetNetworkService();
  network_service->UpdateSignedTreeHead(sth);
}

}  // namespace

namespace component_updater {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: ojjgnpkioondelmggbekfhllhdaimnho
const uint8_t kSthSetPublicKeySHA256[32] = {
    0xe9, 0x96, 0xdf, 0xa8, 0xee, 0xd3, 0x4b, 0xc6, 0x61, 0x4a, 0x57,
    0xbb, 0x73, 0x08, 0xcd, 0x7e, 0x51, 0x9b, 0xcc, 0x69, 0x08, 0x41,
    0xe1, 0x96, 0x9f, 0x7c, 0xb1, 0x73, 0xef, 0x16, 0x80, 0x0a};

const char kSTHSetFetcherManifestName[] = "Signed Tree Heads";

STHSetComponentInstallerPolicy::STHSetComponentInstallerPolicy() {}

STHSetComponentInstallerPolicy::~STHSetComponentInstallerPolicy() = default;

// static
void STHSetComponentInstallerPolicy::ReconfigureAfterNetworkRestart() {
  if (!GetInstallDir().empty())
    ConfigureNetworkService();
}

void STHSetComponentInstallerPolicy::SetNetworkServiceForTesting(
    network::mojom::NetworkService* network_service) {
  DCHECK(network_service);
  g_network_service_for_testing = network_service;
}

bool STHSetComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

// Public data is delivered via this component, no need for encryption.
bool STHSetComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
STHSetComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void STHSetComponentInstallerPolicy::OnCustomUninstall() {}

void STHSetComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  GetInstallDir() = install_dir;
  ConfigureNetworkService();
}

// Called during startup and installation before ComponentReady().
bool STHSetComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath STHSetComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("CertificateTransparency"));
}

void STHSetComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kSthSetPublicKeySHA256),
               std::end(kSthSetPublicKeySHA256));
}

std::string STHSetComponentInstallerPolicy::GetName() const {
  return kSTHSetFetcherManifestName;
}

update_client::InstallerAttributes
STHSetComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> STHSetComponentInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

// static
void STHSetComponentInstallerPolicy::ConfigureNetworkService() {
  // Load and parse the STH JSON on a background task runner, then
  // dispatch back to the current task runner with all of the successfully
  // parsed results.
  auto background_runner = base::MakeRefCounted<AfterStartupTaskUtils::Runner>(
      base::CreateTaskRunnerWithTraits(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()}));
  background_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&LoadSTHsFromDisk, GetInstalledPath(GetInstallDir()),
                     base::SequencedTaskRunnerHandle::Get(),
                     base::BindRepeating(OnSTHLoaded)));
}

void RegisterSTHSetComponent(ComponentUpdateService* cus,
                             const base::FilePath& user_data_dir) {
  DVLOG(1) << "Registering STH Set fetcher component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<STHSetComponentInstallerPolicy>());
  installer->Register(cus, base::Closure());
}

}  // namespace component_updater
