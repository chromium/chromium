// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_default_app_list.h"

#include <string.h>

#include <utility>
#include <vector>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/arc_util.h"
#include "base/barrier_closure.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_scoped_pref_update.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"

namespace {

constexpr char kActivity[] = "activity";
constexpr char kAppPath[] = "app_path";
constexpr char kName[] = "name";
constexpr char kOem[] = "oem";
constexpr char kPackageName[] = "package_name";

constexpr char kDefaultApps[] = "arc.apps.default";
constexpr char kHidden[] = "hidden";

// Sub-directory wher ARC apps forward declarations are stored.
const base::FilePath::CharType kArcDirectory[] = FILE_PATH_LITERAL("arc");
const base::FilePath::CharType kArcTestDirectory[] =
    FILE_PATH_LITERAL("arc_default_apps");
const base::FilePath::CharType kArcTestBoardDirectory[] =
    FILE_PATH_LITERAL("arc_board_default_apps");
const base::FilePath::CharType kArcTestNonAdaptiveDirectory[] =
    FILE_PATH_LITERAL("arc_non_adaptive_default_apps");

bool use_test_apps_directory = false;

std::unique_ptr<ArcDefaultAppList::AppInfoMap> ReadAppsFromFileThread(
    const base::FilePath& base_path) {
  // FileEnumerator does not work with a symbolic link dir. So map link
  // to real folder in case |base_path| specifies a symbolic link.
  std::optional<base::FilePath> link_target =
      base::ReadSymbolicLinkAbsolute(base_path);
  base::FilePath root_dir = link_target.value_or(base_path);

  std::unique_ptr<ArcDefaultAppList::AppInfoMap> apps =
      std::make_unique<ArcDefaultAppList::AppInfoMap>();

  base::FilePath::StringType extension(".json");
  base::FileEnumerator json_files(root_dir,
                                  false,  // Recursive.
                                  base::FileEnumerator::FILES);

  for (base::FilePath file = json_files.Next(); !file.empty();
      file = json_files.Next()) {
    if (!file.MatchesExtension(extension)) {
      DVLOG(1) << "Not considering: " << file.LossyDisplayName()
               << " (does not have a .json extension)";
      continue;
    }

    JSONFileValueDeserializer deserializer(file);
    std::string error_msg;
    auto app_info_ptr = deserializer.Deserialize(nullptr, &error_msg);
    if (!app_info_ptr) {
      VLOG(2) << "Unable to deserialize json data: " << error_msg << " in file "
              << file.value() << ".";
      continue;
    }
    base::Value::Dict app_info = std::move(app_info_ptr->GetDict());

    auto* name = app_info.FindString(kName);
    auto* package_name = app_info.FindString(kPackageName);
    auto* activity = app_info.FindString(kActivity);
    auto* app_path = app_info.FindString(kAppPath);
    bool oem = app_info.FindBool(kOem).value_or(false);

    if (!name || !package_name || !activity || !app_path || name->empty() ||
        package_name->empty() || activity->empty() || app_path->empty()) {
      VLOG(2) << "ARC app declaration is incomplete in file " << file.value()
              << ".";
      continue;
    }

    const std::string app_id =
        ArcAppListPrefs::GetAppId(*package_name, *activity);
    std::unique_ptr<ArcDefaultAppList::AppInfo> app =
        std::make_unique<ArcDefaultAppList::AppInfo>(
            *name, *package_name, *activity, oem, root_dir.Append(*app_path));
    apps.get()->insert(
        std::pair<std::string, std::unique_ptr<ArcDefaultAppList::AppInfo>>(
            app_id, std::move(app)));
  }

  return apps;
}

// Returns true if default app |app_id| is marked as hidden in the prefs.
bool IsAppHidden(const PrefService* prefs, const std::string& app_id) {
  const base::Value::Dict& apps_dict = prefs->GetDict(kDefaultApps);

  const base::Value::Dict* app_dict = apps_dict.FindDict(app_id);
  if (!app_dict)
    return false;
  return app_dict->FindBool(kHidden).value_or(false);
}

std::string GetBoardName(const base::FilePath& build_prop_path) {
  constexpr char kKeyToFind[] = "ro.product.board=";

  std::string content;
  if (!base::ReadFileToString(build_prop_path, &content)) {
    PLOG(ERROR) << "Failed to read " << build_prop_path;
    return std::string();
  }

  const std::vector<std::string> lines = base::SplitString(
      content, "\n", base::WhitespaceHandling::KEEP_WHITESPACE,
      base::SplitResult::SPLIT_WANT_ALL);
  for (const auto& line : lines) {
    if (!base::StartsWith(line, kKeyToFind, base::CompareCase::SENSITIVE))
      continue;
    const std::string board = line.substr(strlen(kKeyToFind));
    VLOG(2) << "Current board is " << board;
    return board;
  }

  LOG(ERROR) << "Failed to find " << kKeyToFind << " in " << build_prop_path;
  return std::string();
}

}  // namespace

// static
void ArcDefaultAppList::UseTestAppsDirectory() {
  use_test_apps_directory = true;
}

// static
void ArcDefaultAppList::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kDefaultApps);
}

// static
std::string ArcDefaultAppList::GetBoardNameForTesting(
    const base::FilePath& build_prop_path) {
  return GetBoardName(build_prop_path);
}

ArcDefaultAppList::ArcDefaultAppList(Profile* profile,
                                     base::OnceClosure ready_callback)
    : profile_(profile), ready_callback_(std::move(ready_callback)) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  arc::ArcSessionManager::Get()->AddObserver(this);
}

ArcDefaultAppList::~ArcDefaultAppList() {
  auto* manager = arc::ArcSessionManager::Get();
  if (manager)  // for unit testing
    manager->RemoveObserver(this);
}

void ArcDefaultAppList::OnPropertyFilesExpanded(bool result) {
  if (!result) {
    // Failed to generate |kGeneratedPropertyFilesPath[Vm]| for whatever reason.
    // Continue anyway not to stall the launcher initialization. In this case,
    // ARC[VM] itself won't start, so not being able to get the board name won't
    // be a huge problem either.
    VLOG(1) << "Unable to get the board name.";
    LoadDefaultApps(std::string());
    return;
  }

  VLOG(1) << "Getting the board name";
  const char* source_file = arc::IsArcVmEnabled()
                                ? arc::kGeneratedCombinedPropertyFilePathVm
                                : arc::kGeneratedBuildPropertyFilePath;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetBoardName, base::FilePath(source_file)),
      base::BindOnce(&ArcDefaultAppList::LoadDefaultApps,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcDefaultAppList::LoadDefaultApps(std::string board_name) {
  VLOG(1) << "Start loading default apps. Board name is "
          << (board_name.empty() ? "<unknown>" : board_name);
  std::vector<base::FilePath> sources;

  base::FilePath base_path;
  if (!use_test_apps_directory) {
    const bool valid_path = base::PathService::Get(
        chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS, &base_path);
    DCHECK(valid_path);
    const base::FilePath base_arc_path = base_path.Append(kArcDirectory);
    sources.push_back(base_arc_path);
    if (!board_name.empty())
      sources.push_back(base_arc_path.Append(board_name));
  } else {
    const bool valid_path =
        base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
    DCHECK(valid_path);
    sources.push_back(base_path.Append(kArcTestDirectory));
    sources.push_back(base_path.Append(kArcTestBoardDirectory));
    sources.push_back(base_path.Append(kArcTestNonAdaptiveDirectory));
  }

  // Using base::Unretained(this) here is safe since we own barrier_closure_.
  barrier_closure_ = base::BarrierClosure(
      sources.size(),
      base::BindOnce(&ArcDefaultAppList::OnAppsReady, base::Unretained(this)));

  // Once ready OnAppsReady is called.
  for (const auto& source : sources) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&ReadAppsFromFileThread, source),
        base::BindOnce(&ArcDefaultAppList::OnAppsRead,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ArcDefaultAppList::OnAppsRead(std::unique_ptr<AppInfoMap> apps) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  const PrefService* const prefs = profile_->GetPrefs();
  for (auto& entry : *apps.get()) {
    AppInfoMap& app_map =
        IsAppHidden(prefs, entry.first) ? hidden_apps_ : visible_apps_;
    app_map[entry.first] = std::move(entry.second);
  }

  barrier_closure_.Run();
}

void ArcDefaultAppList::OnAppsReady() {
  const PrefService* const prefs = profile_->GetPrefs();
  // Register Play Store as default app. Some services and ArcSupportHost may
  // not be available in tests.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  const extensions::Extension* arc_host =
      registry->GetInstalledExtension(arc::kPlayStoreAppId);
  if (arc_host && arc::IsPlayStoreAvailable()) {
    std::unique_ptr<ArcDefaultAppList::AppInfo> play_store_app =
        std::make_unique<ArcDefaultAppList::AppInfo>(arc_host->name(),
                                       arc::kPlayStorePackage,
                                       arc::kPlayStoreActivity,
                                       false /* oem */,
                                       base::FilePath() /* app_path */);
    AppInfoMap& app_map =
        IsAppHidden(prefs, arc::kPlayStoreAppId) ? hidden_apps_ : visible_apps_;
    app_map.insert(
        std::pair<std::string, std::unique_ptr<ArcDefaultAppList::AppInfo>>(
            arc::kPlayStoreAppId, std::move(play_store_app)));
  }

  std::move(ready_callback_).Run();
}

const ArcDefaultAppList::AppInfo* ArcDefaultAppList::GetApp(
    const std::string& app_id) const {
  if ((filter_level_ == FilterLevel::ALL) ||
      (filter_level_ == FilterLevel::OPTIONAL_APPS &&
       app_id != arc::kPlayStoreAppId)) {
    return nullptr;
  }
  const auto it = visible_apps_.find(app_id);
  if (it == visible_apps_.end())
    return nullptr;
  return it->second.get();
}

bool ArcDefaultAppList::HasApp(const std::string& app_id) const {
  return GetApp(app_id) != nullptr;
}

bool ArcDefaultAppList::HasPackage(const std::string& package_name) const {
  for (const auto& it : visible_apps_) {
    if (it.second->package_name == package_name)
      return true;
  }
  return false;
}

bool ArcDefaultAppList::HasHiddenPackage(
    const std::string& package_name) const {
  for (const auto& it : hidden_apps_) {
    if (it.second->package_name == package_name)
      return true;
  }
  return false;
}

void ArcDefaultAppList::SetAppHidden(const std::string& app_id, bool hidden) {
  AppInfoMap& active_map = hidden ? visible_apps_ : hidden_apps_;
  AppInfoMap& inactive_map = hidden ? hidden_apps_ : visible_apps_;

  auto it = active_map.find(app_id);
  if (it == active_map.end())
    return;
  inactive_map[app_id] = std::move(it->second);
  active_map.erase(it);

  // Store hidden flag.
  arc::ArcAppScopedPrefUpdate(profile_->GetPrefs(), app_id, kDefaultApps)
      ->Set(kHidden, hidden);
}

std::map<std::string, const ArcDefaultAppList::AppInfo*>
ArcDefaultAppList::GetActiveApps() const {
  std::map<std::string, const AppInfo*> result;
  for (const auto& it : visible_apps_) {
    if (HasApp(it.first))
      result[it.first] = it.second.get();
  }
  return result;
}

std::unordered_set<std::string> ArcDefaultAppList::GetActivePackages() const {
  std::unordered_set<std::string> result;
  for (const auto& it : GetActiveApps())
    result.insert(it.second->package_name);
  return result;
}

ArcDefaultAppList::AppInfo::AppInfo(const std::string& name,
                                    const std::string& package_name,
                                    const std::string& activity,
                                    bool oem,
                                    const base::FilePath app_path)
    : name(name),
      package_name(package_name),
      activity(activity),
      oem(oem),
      app_path(app_path) {}

ArcDefaultAppList::AppInfo::~AppInfo() {}
