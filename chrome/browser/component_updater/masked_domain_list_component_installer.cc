// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/masked_domain_list_component_installer.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/installer_policies/masked_domain_list_component_installer_policy.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/masked_domain_list.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace component_updater {

namespace {

constexpr base::FilePath::CharType kDefaultMdlFileName[] =
    FILE_PATH_LITERAL("Default MDL");

constexpr base::FilePath::CharType kRegularBrowsingMdlFileName[] =
    FILE_PATH_LITERAL("Regular Browsing MDL");

bool UseFlatbuffer() {
  return base::FeatureList::IsEnabled(
      network::features::kMaskedDomainListFlatbufferImpl);
}

struct BuildFlatbufferResult {
  base::File default_mdl_file;
  uint64_t default_mdl_size = 0;
  base::File regular_browsing_mdl_file;
  uint64_t regular_browsing_mdl_size = 0;
};

void BuildFlatbuffer(
    std::optional<mojo_base::ProtoWrapper> masked_domain_list) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      // Build the flatbuffer file in a thread, as it does some CPU-intensive
      // processing and writes to disk, which can block.
      base::BindOnce(
          [](std::optional<mojo_base::ProtoWrapper> masked_domain_list)
              -> std::optional<BuildFlatbufferResult> {
            base::FilePath user_data_path;
            if (!base::PathService::Get(chrome::DIR_USER_DATA,
                                        &user_data_path)) {
              VLOG(1) << "DIR_USER_DATA does not exist";
              return std::nullopt;
            }

            base::FilePath default_mdl_file_path =
                user_data_path.Append(kDefaultMdlFileName);
            base::FilePath regular_browsing_mdl_file_path =
                user_data_path.Append(kRegularBrowsingMdlFileName);

            std::optional<masked_domain_list::MaskedDomainList> mdl =
                masked_domain_list->As<masked_domain_list::MaskedDomainList>();
            if (!mdl.has_value()) {
              VLOG(1) << "Masked Domain List was empty";
              return std::nullopt;
            }
            ip_protection::Telemetry().MdlSize(mdl->ByteSizeLong());
            const base::Time start_time = base::Time::Now();
            if (!ip_protection::MaskedDomainList::BuildFromProto(
                    *mdl, default_mdl_file_path,
                    regular_browsing_mdl_file_path)) {
              VLOG(1) << "Masked Domain List flatbuffer build failed";
              return std::nullopt;
            }
            base::UmaHistogramTimes(
                "NetworkService.IpProtection.ProxyAllowList."
                "FlatbufferBuildTime",
                base::Time::Now() - start_time);

            std::optional<int64_t> default_mdl_size =
                base::GetFileSize(default_mdl_file_path);
            if (!default_mdl_size.has_value()) {
              VLOG(1) << "Could not get size of " << default_mdl_file_path;
              return std::nullopt;
            }
            ip_protection::Telemetry().MdlEstimatedDiskUsage(*default_mdl_size);

            std::optional<int64_t> regular_browsing_mdl_size =
                base::GetFileSize(regular_browsing_mdl_file_path);
            if (!regular_browsing_mdl_size.has_value()) {
              VLOG(1) << "Could not get size of "
                      << regular_browsing_mdl_file_path;
              return std::nullopt;
            }
            ip_protection::Telemetry().MdlEstimatedDiskUsage(
                *regular_browsing_mdl_size);

            // Open the files read-only, and share those file handles to the
            // network service.
            int flags = base::File::AddFlagsForPassingToUntrustedProcess(
                base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ |
                base::File::Flags::FLAG_DELETE_ON_CLOSE);

            base::File default_mdl_file(default_mdl_file_path, flags);
            base::File regular_browsing_mdl_file(regular_browsing_mdl_file_path,
                                                 flags);

            BuildFlatbufferResult result{
                .default_mdl_file = std::move(default_mdl_file),
                .default_mdl_size =
                    base::checked_cast<uint64_t>(*default_mdl_size),
                .regular_browsing_mdl_file =
                    std::move(regular_browsing_mdl_file),
                .regular_browsing_mdl_size =
                    base::checked_cast<uint64_t>(*regular_browsing_mdl_size),
            };
            return result;
          },
          std::move(masked_domain_list)),
      // Call to the network service on the main thread.
      base::BindOnce([](std::optional<BuildFlatbufferResult> result) {
        if (result.has_value()) {
          content::GetNetworkService()->UpdateMaskedDomainListFlatbuffer(
              std::move(result->default_mdl_file), result->default_mdl_size,
              std::move(result->regular_browsing_mdl_file),
              result->regular_browsing_mdl_size);
        }
      }));
}

}  // namespace

void OnMaskedDomainListReady(
    base::Version version,
    std::optional<mojo_base::ProtoWrapper> masked_domain_list) {
  base::UmaHistogramBoolean(
      "NetworkService.IpProtection.ProxyAllowList."
      "UpdateSuccess",
      masked_domain_list.has_value());
  if (masked_domain_list.has_value()) {
    VLOG(1) << "Received Masked Domain List";

    if (UseFlatbuffer()) {
      BuildFlatbuffer(std::move(masked_domain_list));
    } else {
      content::GetNetworkService()->UpdateMaskedDomainList(
          std::move(masked_domain_list).value(),
          /*exclusion_list=*/std::vector<std::string>());
    }
  } else {
    VLOG(1) << "Could not read Masked Domain List file";
  }
}
void RegisterMaskedDomainListComponent(ComponentUpdateService* cus) {
  if (!MaskedDomainListComponentInstallerPolicy::IsEnabled()) {
    return;
  }

  VLOG(1) << "Registering Masked Domain List component.";

  auto policy = std::make_unique<MaskedDomainListComponentInstallerPolicy>(
      base::BindRepeating(OnMaskedDomainListReady));

  base::MakeRefCounted<ComponentInstaller>(std::move(policy),
                                           /*action_handler=*/nullptr,
                                           base::TaskPriority::USER_BLOCKING)
      ->Register(cus, base::OnceClosure());
}
}  // namespace component_updater
