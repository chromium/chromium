// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"

#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/policy/dlp/dlp_extract_io_task_observer.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash_utils.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_event_storage.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

namespace policy {
namespace {

// System application URLs.
// Please keep them updated with dlp_files_controller_ash_unittest.cc.
constexpr char kFileManagerUrl[] = "chrome://file-manager/";
constexpr char kImageLoaderUrl[] =
    "chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/";

// Timeout defining when two events having the same properties are considered
// duplicates.
// TODO(crbug.com/1368982): determine the value to use.
constexpr base::TimeDelta kCooldownTimeout = base::Seconds(5);

// The maximum number of entries that can be kept in the
// DlpFilesEventStorage.
// TODO(crbug.com/1366299): determine the value to use.
constexpr size_t kEntriesLimit = 100;

// Returns a `DlpFileDestination` with a source URL or component, based on
// |app_update|. If neither URL nor component can be found, returns nullopt.
std::optional<DlpFileDestination> GetFileDestinationForApp(
    const apps::AppUpdate& app_update) {
  switch (app_update.AppType()) {
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserExtension:
    case apps::AppType::kChromeApp:
      return DlpFileDestination(GURL(base::StrCat(
          {extensions::kExtensionScheme, "://", app_update.AppId()})));
    case apps::AppType::kArc:
      return DlpFileDestination(data_controls::Component::kArc);
    case apps::AppType::kCrostini:
      return DlpFileDestination(data_controls::Component::kCrostini);
    case apps::AppType::kPluginVm:
      return DlpFileDestination(data_controls::Component::kPluginVm);
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
      // Expecting `PublisherId()` to return an URL. For web apps this should be
      // the start URL.
      return DlpFileDestination(GURL(app_update.PublisherId()));
    case apps::AppType::kUnknown:
    case apps::AppType::kBuiltIn:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
    case apps::AppType::kBruschetta:
      return std::nullopt;
  }
  return std::nullopt;
}

// Converts files paths to file system URLs.
std::vector<storage::FileSystemURL> ConvertFilePathsToFileSystemUrls(
    Profile* profile,
    const storage::FileSystemContext& file_system_context,
    const std::vector<base::FilePath>& files_paths) {
  std::vector<storage::FileSystemURL> file_system_urls;

  for (const auto& file_path : files_paths) {
    GURL gurl;
    if (file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
            profile, file_path, file_manager::util::GetFileManagerURL(),
            &gurl)) {
      file_system_urls.push_back(
          file_system_context.CrackURLInFirstPartyContext(gurl));
    }
  }

  return file_system_urls;
}

file_manager::VolumeManager* GetVolumeManager(
    content::BrowserContext* context) {
  CHECK(context);

  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(context);
  if (!volume_manager) {
    return nullptr;
  }
  return volume_manager;
}

// Returns whether `url` represents the URL of a system application.
bool IsSystemAppURL(const GURL& url) {
  static constexpr auto kSystemURLsMap =
      base::MakeFixedFlatSet<std::string_view>(
          {kFileManagerUrl, kImageLoaderUrl});
  return kSystemURLsMap.contains(url.spec());
}

// Return converted `level`. It is converted to kBlock if it is `kWarn` and the
// destination is a system app to avoid spamming the user with warning requests
// for browsing a folder with warned (image) files.
DlpRulesManager::Level ConvertSystemAppWarning(
    DlpRulesManager::Level level,
    const DlpFileDestination& destination) {
  if (level == DlpRulesManager::Level::kWarn && destination.url() &&
      IsSystemAppURL(*destination.url())) {
    return DlpRulesManager::Level::kBlock;
  }
  return level;
}

}  // namespace

// static
DlpFilesControllerAsh* DlpFilesControllerAsh::GetForPrimaryProfile() {
  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  return static_cast<DlpFilesControllerAsh*>(
      rules_manager ? rules_manager->GetDlpFilesController() : nullptr);
}

DlpFilesControllerAsh::DlpFileMetadata::DlpFileMetadata(
    const std::string& source_url,
    const std::string& referrer_url,
    bool is_dlp_restricted,
    bool is_restricted_for_destination)
    : source_url(source_url),
      referrer_url(referrer_url),
      is_dlp_restricted(is_dlp_restricted),
      is_restricted_for_destination(is_restricted_for_destination) {}

DlpFilesControllerAsh::DlpFileRestrictionDetails::DlpFileRestrictionDetails() =
    default;

DlpFilesControllerAsh::DlpFileRestrictionDetails::DlpFileRestrictionDetails(
    DlpFileRestrictionDetails&&) = default;
DlpFilesControllerAsh::DlpFileRestrictionDetails&
DlpFilesControllerAsh::DlpFileRestrictionDetails::operator=(
    DlpFilesControllerAsh::DlpFileRestrictionDetails&&) = default;

DlpFilesControllerAsh::DlpFileRestrictionDetails::~DlpFileRestrictionDetails() =
    default;

DlpFilesControllerAsh::DlpFilesControllerAsh(
    const DlpRulesManager& rules_manager,
    Profile* profile)
    : DlpFilesController(rules_manager),
      profile_(profile),
      event_storage_(std::make_unique<DlpFilesEventStorage>(kCooldownTimeout,
                                                            kEntriesLimit)) {
  CHECK(profile_);

  auto* volume_manager = GetVolumeManager(profile_);
  if (!volume_manager) {
    LOG(ERROR)
        << "DlpFilesControllerAsh failed to find file_manager::VolumeManager";
    return;
  }

  volume_manager->AddObserver(this);

  auto* io_task_controller = volume_manager->io_task_controller();
  if (!io_task_controller) {
    LOG(ERROR) << "DlpFilesControllerAsh failed to find "
                  "file_manager::io_task::IOTaskController";
    return;
  }
  extract_io_task_observer_ =
      std::make_unique<DlpExtractIOTaskObserver>(*io_task_controller);
}

DlpFilesControllerAsh::~DlpFilesControllerAsh() {
  if (extract_io_task_observer_) {
    // If `extract_io_task_observer_` is still alive, it means we are deleting
    // FilesController before VolumeManager, otherwise we would have been
    // notified in `OnShutdownStart`.
    auto* volume_manager = GetVolumeManager(profile_);
    if (volume_manager) {
      volume_manager->RemoveObserver(this);
    }
  }
}

void DlpFilesControllerAsh::CheckIfTransferAllowed(
    std::optional<file_manager::io_task::IOTaskId> task_id,
    const std::vector<storage::FileSystemURL>& transferred_files,
    storage::FileSystemURL destination,
    bool is_move,
    CheckIfTransferAllowedCallback result_callback) {
  auto* file_system_context = GetFileSystemContextForPrimaryProfile();
  if (!file_system_context) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  // If the destination file path is in MyFiles, all files transfers should be
  // allowed.
  if (IsInLocalFileSystem(destination.path())) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  std::vector<storage::FileSystemURL> filtered_files;
  // If the copied file isn't in the local file system, or the file is in the
  // same file system as the destination, no restrictions should be applied.
  for (const auto& file : transferred_files) {
    if (!IsInLocalFileSystem(file.path()) ||
        file.IsInSameFileSystem(destination)) {
      continue;
    }
    filtered_files.push_back(file);
  }
  if (filtered_files.empty()) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  auto* roots_recursion_delegate = new RootsRecursionDelegate(
      file_system_context, std::move(filtered_files),
      base::BindOnce(&DlpFilesControllerAsh::ContinueCheckIfTransferAllowed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(task_id),
                     std::move(destination), is_move,
                     std::move(result_callback)));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RootsRecursionDelegate::Run,
                     // base::Unretained() is safe since |recursion_delegate|
                     // will delete itself after all the files list if ready.
                     base::Unretained(roots_recursion_delegate)));
}

void DlpFilesControllerAsh::GetDlpMetadata(
    const std::vector<storage::FileSystemURL>& files,
    std::optional<DlpFileDestination> destination,
    GetDlpMetadataCallback result_callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::vector<DlpFileMetadata>());
    return;
  }

  ::dlp::GetFilesSourcesRequest request;
  for (const auto& file : files) {
    if (IsInLocalFileSystem(file.path())) {
      request.add_files_paths(file.path().value());
    }
  }
  chromeos::DlpClient::Get()->GetFilesSources(
      request, base::BindOnce(&DlpFilesControllerAsh::ReturnDlpMetadata,
                              weak_ptr_factory_.GetWeakPtr(), std::move(files),
                              destination, std::move(result_callback)));
}

void DlpFilesControllerAsh::FilterDisallowedUploads(
    std::vector<ui::SelectedFileInfo> selected_files,
    const DlpFileDestination& destination,
    FilterDisallowedUploadsCallback result_callback) {
  if (selected_files.empty()) {
    std::move(result_callback).Run(std::move(selected_files));
    return;
  }

  auto* file_system_context = GetFileSystemContextForPrimaryProfile();
  if (!file_system_context) {
    std::move(result_callback).Run(std::move(selected_files));
    return;
  }

  std::vector<base::FilePath> files_paths;
  for (const auto& file : selected_files) {
    files_paths.push_back(file.local_path.empty() ? file.file_path
                                                  : file.local_path);
  }
  std::vector<storage::FileSystemURL> file_system_urls =
      ConvertFilePathsToFileSystemUrls(profile_, *file_system_context,
                                       files_paths);

  if (file_system_urls.empty()) {
    std::move(result_callback).Run(std::move(selected_files));
    return;
  }

  auto* roots_recursion_delegate = new RootsRecursionDelegate(
      file_system_context, std::move(file_system_urls),
      base::BindOnce(&DlpFilesControllerAsh::ContinueFilterDisallowedUploads,
                     weak_ptr_factory_.GetWeakPtr(), std::move(selected_files),
                     std::move(destination), std::move(result_callback)));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RootsRecursionDelegate::Run,
                     // base::Unretained() is safe since |recursion_delegate|
                     // will delete itself after all the files list if ready.
                     base::Unretained(roots_recursion_delegate)));
}

void DlpFilesControllerAsh::CheckIfDownloadAllowed(
    const DlpFileDestination& download_src,
    const base::FilePath& file_path,
    CheckIfDlpAllowedCallback result_callback) {
  if (!download_src.url().has_value()) {
    // Currently we only support urls as sources.
    std::move(result_callback).Run(true);
    return;
  }

  auto dst_component =
      MapFilePathToPolicyComponent(profile_, base::FilePath(file_path));
  if (!dst_component.has_value()) {
    // We may block downloads only if saved to external component, otherwise
    // downloads should be allowed.
    std::move(result_callback).Run(true);
    return;
  }

  DlpFileDestination dlp_destination = DlpFileDestination(*dst_component);
  // TODO(b/290200170): Check whether referrer_url could be set too.
  FileDaemonInfo file_info(/*inode=*/{}, /*crtime=*/{}, file_path,
                           download_src.url()->spec(),
                           /*referrer_url=*/"");

  IsFilesTransferRestricted(
      std::nullopt, {std::move(file_info)}, dlp_destination,
      dlp::FileAction::kDownload,
      base::BindOnce(
          [](CheckIfDlpAllowedCallback result_callback, Profile* profile,
             const std::vector<std::pair<
                 FileDaemonInfo, ::dlp::RestrictionLevel>>& files_levels) {
            bool is_allowed = true;
            base::FilePath file_path;
            for (const auto& [file, level] : files_levels) {
              if (level == ::dlp::RestrictionLevel::LEVEL_BLOCK ||
                  level == ::dlp::RestrictionLevel::LEVEL_WARN_CANCEL) {
                is_allowed = false;
                file_path = file.path;
                break;
              }
            }
            if (!is_allowed) {
              ::policy::files_controller_ash_utils::ShowDlpBlockedFiles(
                  profile, /*task_id=*/std::nullopt, {file_path},
                  dlp::FileAction::kDownload);
            }
            std::move(result_callback).Run(is_allowed);
          },
          std::move(result_callback), profile_));
}

bool DlpFilesControllerAsh::ShouldPromptBeforeDownload(
    const DlpFileDestination& download_src,
    const base::FilePath& file_path) {
  if (download_src.IsFileSystem()) {
    return false;
  }
  auto dst_component =
      MapFilePathToPolicyComponent(profile_, base::FilePath(file_path));
  if (!dst_component.has_value()) {
    // We may block downloads only if saved to external component, otherwise
    // downloads should be allowed.
    return false;
  }

  DlpRulesManager::Level level = rules_manager_->IsRestrictedComponent(
      download_src.url().value(), dst_component.value(),
      DlpRulesManager::Restriction::kFiles, /*out_source_pattern=*/nullptr,
      /*out_rule_metadata=*/nullptr);
  return level == DlpRulesManager::Level::kBlock ||
         level == DlpRulesManager::Level::kWarn;
}

void DlpFilesControllerAsh::CheckIfLaunchAllowed(
    const apps::AppUpdate& app_update,
    apps::IntentPtr intent,
    CheckIfDlpAllowedCallback result_callback) {
  if (intent->files.empty()) {
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }
  std::optional<DlpFileDestination> destination =
      GetFileDestinationForApp(app_update);
  if (!destination.has_value()) {
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }
  CHECK(!destination->IsMyFiles());

  ::dlp::CheckFilesTransferRequest request;
  for (const auto& file : intent->files) {
    auto file_url = apps::GetFileSystemURL(profile_, file->url);
    request.add_files_paths(file_url.path().value());
  }

  request.set_file_action(intent->IsShareIntent() ? ::dlp::FileAction::SHARE
                                                  : ::dlp::FileAction::OPEN);

  if (destination->url().has_value()) {
    request.set_destination_url(destination->url()->spec());
  } else {  // component
    request.set_destination_component(
        dlp::MapPolicyComponentToProto(destination->component().value()));
  }

  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request,
      base::BindOnce(&DlpFilesControllerAsh::ReturnIfActionAllowed,
                     weak_ptr_factory_.GetWeakPtr(), dlp::FileAction::kOpen,
                     std::move(result_callback)));
}

bool DlpFilesControllerAsh::IsLaunchBlocked(const apps::AppUpdate& app_update,
                                            const apps::IntentPtr& intent) {
  if (intent->files.empty()) {
    return false;
  }

  std::optional<DlpFileDestination> destination =
      GetFileDestinationForApp(app_update);

  if (!destination.has_value()) {
    return false;
  }

  for (const auto& file : intent->files) {
    if (!file->dlp_source_url.has_value()) {
      continue;
    }
    if (destination->url().has_value()) {
      DlpRulesManager::Level level = rules_manager_->IsRestrictedDestination(
          GURL(file->dlp_source_url.value()), *destination->url(),
          DlpRulesManager::Restriction::kFiles, /*out_source_pattern=*/nullptr,
          /*out_destination_pattern=*/nullptr, /*out_rule_metadata=*/nullptr);
      if (level == DlpRulesManager::Level::kBlock) {
        return true;
      }
    } else if (destination->component().has_value()) {
      DlpRulesManager::Level level = rules_manager_->IsRestrictedComponent(
          GURL(file->dlp_source_url.value()), destination->component().value(),
          DlpRulesManager::Restriction::kFiles, /*out_source_pattern=*/nullptr,
          /*out_rule_metadata=*/nullptr);
      if (level == DlpRulesManager::Level::kBlock) {
        return true;
      }
    }
  }

  return false;
}

void DlpFilesControllerAsh::IsFilesTransferRestricted(
    std::optional<file_manager::io_task::IOTaskId> task_id,
    const std::vector<FileDaemonInfo>& transferred_files,
    const DlpFileDestination& destination,
    dlp::FileAction files_action,
    IsFilesTransferRestrictedCallback result_callback) {
  DlpFileDestination actual_dst = destination;

  std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>> files_levels;
  std::vector<FileDaemonInfo> warned_files;
  std::vector<std::string> warned_source_patterns;
  std::vector<DlpRulesManager::RuleMetadata> warned_rules_metadata;
  for (const auto& file : transferred_files) {
    DlpRulesManager::Level level;
    std::string source_pattern;
    DlpRulesManager::RuleMetadata rule_metadata;
    if (destination.component()) {
      data_controls::Component dst_component = *destination.component();
      level = rules_manager_->IsRestrictedComponent(
          file.source_url, dst_component, DlpRulesManager::Restriction::kFiles,
          &source_pattern, &rule_metadata);
      actual_dst = DlpFileDestination(dst_component);
      MaybeReportEvent(file.inode, file.crtime, file.path,
                       file.source_url.spec(), actual_dst, rule_metadata,
                       level);
    } else if (destination.IsFileSystem()) {
      level = DlpRulesManager::Level::kAllow;
    } else {
      DCHECK(destination.url().has_value());
      level = rules_manager_->IsRestrictedDestination(
          GURL(file.source_url), GURL(*destination.url()),
          DlpRulesManager::Restriction::kFiles, &source_pattern,
          /*out_destination_pattern=*/nullptr, &rule_metadata);
      if (!IsSystemAppURL(destination.url().value())) {
        MaybeReportEvent(file.inode, file.crtime, file.path,
                         file.source_url.spec(), actual_dst, rule_metadata,
                         level);
      }
    }

    level = ConvertSystemAppWarning(level, destination);

    switch (level) {
      case DlpRulesManager::Level::kBlock: {
        files_levels.emplace_back(file, ::dlp::RestrictionLevel::LEVEL_BLOCK);
        break;
      }
      case DlpRulesManager::Level::kNotSet:
      case DlpRulesManager::Level::kAllow: {
        files_levels.emplace_back(file, ::dlp::RestrictionLevel::LEVEL_ALLOW);
        break;
      }
      case DlpRulesManager::Level::kReport: {
        files_levels.emplace_back(file, ::dlp::RestrictionLevel::LEVEL_REPORT);
        break;
      }
      case DlpRulesManager::Level::kWarn: {
        warned_files.push_back(file);
        warned_source_patterns.emplace_back(source_pattern);
        warned_rules_metadata.emplace_back(rule_metadata);
        break;
      }
    }
  }

  if (warned_files.empty()) {
    std::move(result_callback).Run(std::move(files_levels));
    return;
  }

  auto* fpnm =
      FilesPolicyNotificationManagerFactory::GetForBrowserContext(profile_);
  if (!fpnm) {
    LOG(ERROR) << "No FilesPolicyNotificationManager instantiated,"
                  "can't show policy warning UI";
    return;
  }
  std::vector<base::FilePath> warning_files_paths;
  base::ranges::for_each(warned_files, [&](auto& warned_file) {
    warning_files_paths.push_back(warned_file.path);
  });
  fpnm->ShowDlpWarning(
      base::BindOnce(&DlpFilesControllerAsh::OnDlpWarnDialogReply,
                     weak_ptr_factory_.GetWeakPtr(), std::move(files_levels),
                     std::move(warned_files), std::move(warned_source_patterns),
                     std::move(warned_rules_metadata), actual_dst, files_action,
                     std::move(result_callback)),
      std::move(task_id), std::move(warning_files_paths), actual_dst,
      files_action);
}

std::vector<DlpFilesControllerAsh::DlpFileRestrictionDetails>
DlpFilesControllerAsh::GetDlpRestrictionDetails(const std::string& source_url) {
  const GURL source(source_url);
  const DlpRulesManager::AggregatedDestinations aggregated_destinations =
      rules_manager_->GetAggregatedDestinations(
          source, DlpRulesManager::Restriction::kFiles);
  const DlpRulesManager::AggregatedComponents aggregated_components =
      rules_manager_->GetAggregatedComponents(
          source, DlpRulesManager::Restriction::kFiles);

  std::vector<DlpFilesControllerAsh::DlpFileRestrictionDetails> result;
  // Add levels for which urls are set.
  for (const auto& [level, urls] : aggregated_destinations) {
    DlpFileRestrictionDetails details;
    details.level = level;
    base::ranges::move(urls.begin(), urls.end(),
                       std::back_inserter(details.urls));
    // Add the components for this level, if any.
    const auto it = aggregated_components.find(level);
    if (it != aggregated_components.end()) {
      base::ranges::move(it->second.begin(), it->second.end(),
                         std::back_inserter(details.components));
    }
    result.emplace_back(std::move(details));
  }

  // There might be levels for which only components are set, so we need to add
  // those separately.
  for (const auto& [level, components] : aggregated_components) {
    if (aggregated_destinations.find(level) != aggregated_destinations.end()) {
      // Already added in the previous loop.
      continue;
    }
    DlpFileRestrictionDetails details;
    details.level = level;
    base::ranges::move(components.begin(), components.end(),
                       std::back_inserter(details.components));
    result.emplace_back(std::move(details));
  }

  return result;
}

std::vector<data_controls::Component>
DlpFilesControllerAsh::GetBlockedComponents(const std::string& source_url) {
  const GURL source(source_url);
  const DlpRulesManager::AggregatedComponents aggregated_components =
      rules_manager_->GetAggregatedComponents(
          source, DlpRulesManager::Restriction::kFiles);

  std::vector<data_controls::Component> result;
  const auto it = aggregated_components.find(DlpRulesManager::Level::kBlock);
  if (it != aggregated_components.end()) {
    base::ranges::move(it->second.begin(), it->second.end(),
                       std::back_inserter(result));
  }
  return result;
}

bool DlpFilesControllerAsh::IsDlpPolicyMatched(const FileDaemonInfo& file) {
  bool restricted = false;

  std::string src_pattern;
  DlpRulesManager::RuleMetadata rule_metadata;
  policy::DlpRulesManager::Level level = rules_manager_->IsRestrictedByAnyRule(
      GURL(file.source_url.spec()),
      policy::DlpRulesManager::Restriction::kFiles, &src_pattern,
      &rule_metadata);

  switch (level) {
    case policy::DlpRulesManager::Level::kBlock:
      restricted = true;
      break;
    case policy::DlpRulesManager::Level::kWarn:
      // Normally this case should not be hit as it means that a restricted file
      // was accessed by a flow without requesting access before. We protect
      // warned files the same way as blocked to not allow unauthorized access.
      restricted = true;
      break;
    default:
      break;
  }

  data_controls::DlpHistogramEnumeration(
      data_controls::dlp::kFilesUnknownAccessLevel, level);

  return restricted;
}

void DlpFilesControllerAsh::OnShutdownStart(
    file_manager::VolumeManager* volume_manager) {
  volume_manager->RemoveObserver(this);
  // IOTaskController is destroyed at VolumeManager deletion. Delete the
  // observer since it depends on IOTaskController.
  extract_io_task_observer_.reset();
}

DlpFilesEventStorage* DlpFilesControllerAsh::GetEventStorageForTesting() {
  return event_storage_.get();
}

std::optional<data_controls::Component>
DlpFilesControllerAsh::MapFilePathToPolicyComponent(
    Profile* profile,
    const base::FilePath& file_path) {
  if (base::FilePath(file_manager::util::GetAndroidFilesPath())
          .IsParent(file_path)) {
    return data_controls::Component::kArc;
  }

  if (base::FilePath(file_manager::util::kRemovableMediaPath)
          .IsParent(file_path)) {
    return data_controls::Component::kUsb;
  }

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (integration_service && integration_service->is_enabled() &&
      integration_service->GetMountPointPath().IsParent(file_path)) {
    return data_controls::Component::kDrive;
  }

  if (ash::cloud_upload::IsODFSMounted(profile)) {
    auto* service = ash::file_system_provider::Service::Get(profile);
    auto provider_id =
        ash::file_system_provider::ProviderId::CreateFromExtensionId(
            extension_misc::kODFSExtensionId);
    auto one_drive_file_systems =
        service->GetProvidedFileSystemInfoList(provider_id);
    CHECK_EQ(one_drive_file_systems.size(), 1u);

    if (one_drive_file_systems[0].mount_path().IsParent(file_path)) {
      return data_controls::Component::kOneDrive;
    }
  }

  base::FilePath linux_files =
      file_manager::util::GetCrostiniMountDirectory(profile);
  if (linux_files == file_path || linux_files.IsParent(file_path)) {
    return data_controls::Component::kCrostini;
  }

  return {};
}

bool DlpFilesControllerAsh::IsInLocalFileSystem(
    const base::FilePath& file_path) {
  auto my_files_folder =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  if (my_files_folder == file_path || my_files_folder.IsParent(file_path)) {
    return true;
  }
  return false;
}

void DlpFilesControllerAsh::ShowDlpBlockedFiles(
    std::optional<file_manager::io_task::IOTaskId> task_id,
    std::vector<base::FilePath> blocked_files,
    dlp::FileAction action) {
  ::policy::files_controller_ash_utils::ShowDlpBlockedFiles(
      profile_, std::move(task_id), std::move(blocked_files), action);
}

void DlpFilesControllerAsh::OnDlpWarnDialogReply(
    std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>>
        files_levels,
    std::vector<FileDaemonInfo> warned_files,
    std::vector<std::string> warned_src_patterns,
    std::vector<DlpRulesManager::RuleMetadata> warned_rules_metadata,
    const DlpFileDestination& dst,
    dlp::FileAction files_action,
    IsFilesTransferRestrictedCallback callback,
    std::optional<std::u16string> user_justification,
    bool should_proceed) {
  DCHECK(warned_files.size() == warned_src_patterns.size());
  DCHECK(warned_files.size() == warned_rules_metadata.size());
  for (size_t i = 0; i < warned_files.size(); ++i) {
    if (should_proceed) {
      data_controls::DlpHistogramEnumeration(
          data_controls::dlp::kFileActionWarnProceededUMA, files_action);
      MaybeReportEvent(warned_files[i].inode, warned_files[i].crtime,
                       warned_files[i].path, warned_files[i].source_url.spec(),
                       dst, warned_rules_metadata[i], std::nullopt);
    }
    files_levels.emplace_back(warned_files[i],
                              should_proceed
                                  ? ::dlp::RestrictionLevel::LEVEL_WARN_PROCEED
                                  : ::dlp::RestrictionLevel::LEVEL_WARN_CANCEL);
  }
  std::move(callback).Run(std::move(files_levels));
}

void DlpFilesControllerAsh::ReturnDisallowedFiles(
    std::optional<file_manager::io_task::IOTaskId> task_id,
    base::flat_map<std::string, storage::FileSystemURL> files_map,
    dlp::FileAction file_action,
    CheckIfTransferAllowedCallback result_callback,
    ::dlp::CheckFilesTransferResponse response) {
  std::vector<storage::FileSystemURL> restricted_files_urls;
  std::vector<base::FilePath> restricted_files_paths;
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
    for (const auto& [file_path, file_system_url] : files_map) {
      restricted_files_urls.push_back(file_system_url);
    }
    std::move(result_callback).Run(std::move(restricted_files_urls));
    return;
  }

  for (const auto& file : response.files_paths()) {
    DCHECK(files_map.find(file) != files_map.end());
    restricted_files_urls.push_back(files_map.at(file));
    restricted_files_paths.emplace_back(file);
  }
  if (!restricted_files_paths.empty() &&
      base::FeatureList::IsEnabled(features::kNewFilesPolicyUX) &&
      task_id.has_value()) {
    ::policy::files_controller_ash_utils::ShowDlpBlockedFiles(
        profile_, std::move(task_id), std::move(restricted_files_paths),
        file_action);
  }
  std::move(result_callback).Run(std::move(restricted_files_urls));
}

void DlpFilesControllerAsh::ReturnAllowedUploads(
    std::vector<ui::SelectedFileInfo> selected_files,
    FilterDisallowedUploadsCallback result_callback,
    ::dlp::CheckFilesTransferResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
    std::move(result_callback).Run(std::move(selected_files));
    return;
  }

  if (!response.files_paths().empty()) {
    std::vector<base::FilePath> restricted_files(response.files_paths().begin(),
                                                 response.files_paths().end());
    // If any of the selected files/folders is restricted or contains a
    // restricted file, it'll be removed.
    std::erase_if(
        selected_files,
        [&restricted_files](const ui::SelectedFileInfo& selected_file) -> bool {
          return base::ranges::any_of(
              restricted_files, [&](const base::FilePath& restricted_file) {
                return selected_file.file_path == restricted_file ||
                       selected_file.file_path.IsParent(restricted_file);
              });
        });

    ::policy::files_controller_ash_utils::ShowDlpBlockedFiles(
        profile_, /*task_id=*/std::nullopt, std::move(restricted_files),
        dlp::FileAction::kUpload);
  }

  std::move(result_callback).Run(std::move(selected_files));
}

void DlpFilesControllerAsh::ReturnDlpMetadata(
    const std::vector<storage::FileSystemURL>& files,
    std::optional<DlpFileDestination> destination,
    GetDlpMetadataCallback result_callback,
    const ::dlp::GetFilesSourcesResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get files sources, error: "
               << response.error_message();
  }

  base::flat_map<std::string, DlpFileMetadata> metadata_map;
  for (const auto& metadata : response.files_metadata()) {
    DlpRulesManager::Level level = rules_manager_->IsRestrictedByAnyRule(
        GURL(metadata.source_url()), DlpRulesManager::Restriction::kFiles,
        nullptr, nullptr);
    bool is_dlp_restricted = level != DlpRulesManager::Level::kNotSet &&
                             level != DlpRulesManager::Level::kAllow;
    bool is_restricted_for_destination = false;
    // Only if it's restricted by any rule and the destination is passed, check
    // if this combination is also blocked or not.
    if (level == DlpRulesManager::Level::kBlock && destination.has_value()) {
      std::optional<data_controls::Component> dst_component =
          destination->component();
      if (dst_component.has_value()) {
        DlpRulesManager::Level dst_level =
            rules_manager_->IsRestrictedComponent(
                GURL(metadata.source_url()), dst_component.value(),
                DlpRulesManager::Restriction::kFiles, nullptr, nullptr);
        is_restricted_for_destination =
            dst_level == DlpRulesManager::Level::kBlock;
      } else {
        DCHECK(destination->url());
        DlpRulesManager::Level dst_level =
            rules_manager_->IsRestrictedDestination(
                GURL(metadata.source_url()), *destination->url(),
                DlpRulesManager::Restriction::kFiles, nullptr, nullptr,
                nullptr);
        is_restricted_for_destination =
            dst_level == DlpRulesManager::Level::kBlock;
      }
    }

    metadata_map.emplace(
        metadata.path(),
        DlpFileMetadata(metadata.source_url(), metadata.referrer_url(),
                        is_dlp_restricted, is_restricted_for_destination));
  }

  std::vector<DlpFileMetadata> result;
  for (const auto& file : files) {
    auto metadata_itr = metadata_map.find(file.path().value());
    if (metadata_itr == metadata_map.end()) {
      result.emplace_back("", "", false, false);
    } else {
      result.emplace_back(metadata_itr->second);
    }
  }

  std::move(result_callback).Run(std::move(result));
}

void DlpFilesControllerAsh::MaybeReportEvent(
    ino64_t inode,
    time_t crtime,
    const base::FilePath& path,
    const std::string& source_url,
    const DlpFileDestination& dst,
    const DlpRulesManager::RuleMetadata& rule_metadata,
    std::optional<DlpRulesManager::Level> level) {
  const bool is_warning_proceeded_event = !level.has_value();

  if (!is_warning_proceeded_event &&
      (level.value() == DlpRulesManager::Level::kAllow ||
       level.value() == DlpRulesManager::Level::kNotSet)) {
    return;
  }

  data_controls::DlpReportingManager* reporting_manager =
      rules_manager_->GetReportingManager();
  if (!reporting_manager) {
    return;
  }

  // Warning proceeded events are always user-initiated since they are triggered
  // only when the user interacts with the warning dialog.
  if (!is_warning_proceeded_event &&
      !event_storage_->StoreEventAndCheckIfItShouldBeReported({inode, crtime},
                                                              dst)) {
    return;
  }

  std::unique_ptr<data_controls::DlpPolicyEventBuilder> event_builder =
      is_warning_proceeded_event
          ? data_controls::DlpPolicyEventBuilder::WarningProceededEvent(
                source_url, rule_metadata.name, rule_metadata.obfuscated_id,
                DlpRulesManager::Restriction::kFiles)
          : data_controls::DlpPolicyEventBuilder::Event(
                source_url, rule_metadata.name, rule_metadata.obfuscated_id,
                DlpRulesManager::Restriction::kFiles, level.value());

  event_builder->SetContentName(path.BaseName().value());

  if (dst.url().has_value()) {
    event_builder->SetDestinationUrl(dst.url()->spec());
  }
  if (dst.component().has_value()) {
    event_builder->SetDestinationComponent(dst.component().value());
  }
  reporting_manager->ReportEvent(event_builder->Create());
}

void DlpFilesControllerAsh::ContinueCheckIfTransferAllowed(
    std::optional<file_manager::io_task::IOTaskId> task_id,
    storage::FileSystemURL destination,
    bool is_move,
    CheckIfTransferAllowedCallback result_callback,
    std::vector<storage::FileSystemURL> transferred_files) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  ::dlp::CheckFilesTransferRequest request;
  base::flat_map<std::string, storage::FileSystemURL> transferred_files_map;
  for (const auto& file : transferred_files) {
    auto file_path = file.path().value();
    transferred_files_map[file_path] = file;
    request.add_files_paths(file_path);
  }
  if (transferred_files_map.empty()) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  std::optional<data_controls::Component> component =
      MapFilePathToPolicyComponent(profile_, destination.path());
  ::dlp::DlpComponent proto;
  if (component) {
    proto = dlp::MapPolicyComponentToProto(*component);
  } else {
    proto = ::dlp::DlpComponent::SYSTEM;
  }
  request.set_destination_component(proto);
  request.set_file_action(is_move ? ::dlp::FileAction::MOVE
                                  : ::dlp::FileAction::COPY);
  if (task_id) {
    request.set_io_task_id(task_id.value());
  }

  auto return_transfers_callback =
      base::BindOnce(&DlpFilesControllerAsh::ReturnDisallowedFiles,
                     weak_ptr_factory_.GetWeakPtr(), std::move(task_id),
                     std::move(transferred_files_map),
                     is_move ? dlp::FileAction::kMove : dlp::FileAction::kCopy,
                     std::move(result_callback));
  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request, std::move(return_transfers_callback));
}

void DlpFilesControllerAsh::ContinueFilterDisallowedUploads(
    std::vector<ui::SelectedFileInfo> selected_files,
    const DlpFileDestination& destination,
    FilterDisallowedUploadsCallback result_callback,
    std::vector<storage::FileSystemURL> uploaded_files) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::move(selected_files));
    return;
  }

  ::dlp::CheckFilesTransferRequest request;
  for (const auto& file : uploaded_files) {
    request.add_files_paths(file.path().value());
  }
  if (destination.component().has_value()) {
    request.set_destination_component(
        dlp::MapPolicyComponentToProto(destination.component().value()));
  } else {
    DCHECK(destination.url());
    request.set_destination_url(destination.url()->spec());
  }
  request.set_file_action(::dlp::FileAction::UPLOAD);

  auto return_uploads_callback =
      base::BindOnce(&DlpFilesControllerAsh::ReturnAllowedUploads,
                     weak_ptr_factory_.GetWeakPtr(), std::move(selected_files),
                     std::move(result_callback));
  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request, std::move(return_uploads_callback));
}

}  // namespace policy
