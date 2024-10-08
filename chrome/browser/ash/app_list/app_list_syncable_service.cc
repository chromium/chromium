// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_syncable_service.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/one_shot_event.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_sync_model_sanitizer.h"
#include "chrome/browser/ash/app_list/app_list_util.h"
#include "chrome/browser/ash/app_list/app_service/app_service_app_model_builder.h"
#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_model_builder.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/reorder/app_list_reorder_core.h"
#include "chrome/browser/ash/app_list/reorder/app_list_reorder_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/extensions/default_app_order.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"

using syncer::SyncChange;

namespace app_list {

namespace {

constexpr char kNameKey[] = "name";
constexpr char kParentIdKey[] = "parent_id";
constexpr char kPositionKey[] = "position";
constexpr char kPinPositionKey[] = "pin_position";
constexpr char kTypeKey[] = "type";
constexpr char kBackgroundColorKey[] = "background_color";
constexpr char kHueKey[] = "hue";
constexpr char kEmptyItemOrdinalFixable[] = "empty_item_ordinal_fixable";
constexpr char kIsUserPinned[] = "is_user_pinned";
constexpr char kPromisePackageIdKey[] = "promise_package_id";

void GetSyncSpecificsFromSyncItem(const AppListSyncableService::SyncItem* item,
                                  sync_pb::AppListSpecifics* specifics) {
  DCHECK(specifics);
  specifics->set_item_id(item->item_id);
  specifics->set_item_type(item->item_type);
  specifics->set_item_name(item->item_name);
  specifics->set_promise_package_id(item->promise_package_id);
  specifics->set_parent_id(item->parent_id);
  specifics->set_item_ordinal(item->item_ordinal.IsValid()
                                  ? item->item_ordinal.ToInternalValue()
                                  : std::string());
  specifics->set_item_pin_ordinal(item->item_pin_ordinal.IsValid()
                                      ? item->item_pin_ordinal.ToInternalValue()
                                      : std::string());
  if (item->is_user_pinned.has_value() &&
      ash::features::IsRemoveStalePolicyPinnedAppsFromShelfEnabled()) {
    specifics->set_is_user_pinned(*item->is_user_pinned);
  }

  if (item->item_color.IsValid()) {
    specifics->mutable_item_color()->set_background_color(
        item->item_color.background_color());
    specifics->mutable_item_color()->set_hue(item->item_color.hue());
  }
}

syncer::SyncData GetSyncDataFromSyncItem(
    const AppListSyncableService::SyncItem* item) {
  sync_pb::EntitySpecifics specifics;
  GetSyncSpecificsFromSyncItem(item, specifics.mutable_app_list());
  return syncer::SyncData::CreateLocalData(item->item_id, item->item_id,
                                           specifics);
}

void CopyAttributesToSyncItem(const AppListSyncableService::SyncItem* source,
                              AppListSyncableService::SyncItem* target) {
  CHECK_EQ(source->item_type, target->item_type);

  target->item_ordinal = source->item_ordinal;
  target->item_pin_ordinal = source->item_pin_ordinal;
  target->parent_id = source->parent_id;
  target->is_user_pinned = source->is_user_pinned;
  target->item_color = source->item_color;
  target->item_name = source->item_name;
}

bool AppIsDefault(Profile* profile, const std::string& id) {
  // Querying the extension system is legacy logic from the time that we only
  // had extension apps.
  if (extensions::ExtensionPrefs::Get(profile)->WasInstalledByDefault(id))
    return true;

  bool result = false;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(id, [&result](const apps::AppUpdate& update) {
        result = update.InstallReason() == apps::InstallReason::kDefault;
      });
  return result;
}

void SetAppIsDefaultForTest(Profile* profile, const std::string& id) {
  apps::AppPtr delta =
      std::make_unique<apps::App>(apps::AppType::kChromeApp, id);
  delta->install_reason = apps::InstallReason::kDefault;

  std::vector<apps::AppPtr> deltas;
  deltas.push_back(std::move(delta));
  apps::AppServiceProxyFactory::GetForProfile(profile)->OnApps(
      std::move(deltas), apps::AppType::kChromeApp,
      false /* should_notify_initialized */);
}

bool IsUnRemovableDefaultApp(const std::string& id) {
  return id == app_constants::kChromeAppId ||
         id == extensions::kWebStoreAppId ||
         id == file_manager::kFileManagerAppId;
}

void UninstallExtension(extensions::ExtensionService* service,
                        extensions::ExtensionRegistry* registry,
                        const std::string& id) {
  if (service && registry->GetInstalledExtension(id)) {
    service->UninstallExtension(id, extensions::UNINSTALL_REASON_SYNC,
                                nullptr /* error */);
  }
}

sync_pb::AppListSpecifics::AppListItemType GetAppListItemType(
    const ChromeAppListItem* item) {
  if (item->is_folder())
    return sync_pb::AppListSpecifics::TYPE_FOLDER;
  else
    return sync_pb::AppListSpecifics::TYPE_APP;
}

void RemoveSyncItemFromLocalStorage(Profile* profile,
                                    const std::string& item_id) {
  ScopedDictPrefUpdate(profile->GetPrefs(), prefs::kAppListLocalState)
      ->Remove(item_id);
}

void UpdateSyncItemInLocalStorage(
    Profile* profile,
    const AppListSyncableService::SyncItem* sync_item) {
  // Do not persist ephemeral sync items to local state.
  if (sync_item->is_ephemeral)
    return;

  ScopedDictPrefUpdate pref_update(profile->GetPrefs(),
                                   prefs::kAppListLocalState);
  base::Value::Dict* dict_item = pref_update->EnsureDict(sync_item->item_id);
  dict_item->Set(kNameKey, sync_item->item_name);
  dict_item->Set(kPromisePackageIdKey, !sync_item->promise_package_id.empty()
                                           ? sync_item->promise_package_id
                                           : std::string());
  dict_item->Set(kParentIdKey, sync_item->parent_id);
  dict_item->Set(kPositionKey, sync_item->item_ordinal.IsValid()
                                   ? sync_item->item_ordinal.ToInternalValue()
                                   : std::string());
  dict_item->Set(kPinPositionKey,
                 sync_item->item_pin_ordinal.IsValid()
                     ? sync_item->item_pin_ordinal.ToInternalValue()
                     : std::string());
  dict_item->Set(kTypeKey, static_cast<int>(sync_item->item_type));
  dict_item->Set(kEmptyItemOrdinalFixable,
                 sync_item->item_ordinal.IsValid() ||
                     sync_item->empty_item_ordinal_fixable);

  if (sync_item->is_user_pinned.has_value() &&
      ash::features::IsRemoveStalePolicyPinnedAppsFromShelfEnabled()) {
    dict_item->Set(kIsUserPinned, *sync_item->is_user_pinned);
  } else {
    dict_item->Remove(kIsUserPinned);
  }

  // Handle the item color.
  if (sync_item->item_color.IsValid()) {
    dict_item->Set(kBackgroundColorKey,
                   sync_pb::AppListSpecifics::ColorGroup_Name(
                       sync_item->item_color.background_color()));
    dict_item->Set(kHueKey, sync_item->item_color.hue());
  } else if (dict_item->Find(kBackgroundColorKey)) {
    dict_item->Remove(kBackgroundColorKey);
    DCHECK(dict_item->Find(kHueKey));
    dict_item->Remove(kHueKey);
  }
}

AppListSyncableService::ModelUpdaterFactoryCallback*
    g_model_updater_factory_callback_for_test_ = nullptr;

// Returns true if the sync item does not have parent.
bool IsTopLevelAppItem(const AppListSyncableService::SyncItem& sync_item) {
  return sync_item.parent_id.empty();
}

// Returns true if the sync item is a page break item.
bool IsPageBreakItem(const AppListSyncableService::SyncItem& sync_item) {
  return sync_item.item_type == sync_pb::AppListSpecifics::TYPE_PAGE_BREAK;
}

bool IsSystemCreatedSyncFolder(
    const AppListSyncableService::SyncItem& folder_item) {
  if (folder_item.item_type != sync_pb::AppListSpecifics::TYPE_FOLDER)
    return false;
  return folder_item.is_system_folder;
}

// Updates `target` if `target` is different from a valid new value. Returns
// true if `target` gets updated.
bool SetIconColorIfChanged(const ash::IconColor& new_color,
                           ash::IconColor* target) {
  if (!new_color.IsValid())
    return false;

  if (!target->IsValid() || *target != new_color) {
    *target = new_color;
    return true;
  }

  return false;
}

// Returns a result after `lhs` and before `rhs` if they are valid, else returns
// initial-ordinal.
syncer::StringOrdinal CreateBetween(const syncer::StringOrdinal& lhs,
                                    const syncer::StringOrdinal& rhs) {
  if (lhs.IsValid() && rhs.IsValid()) {
    return lhs.CreateBetween(rhs);
  }
  if (lhs.IsValid()) {
    return lhs.CreateAfter();
  }
  if (rhs.IsValid()) {
    return rhs.CreateBefore();
  }
  return syncer::StringOrdinal::CreateInitialOrdinal();
}

}  // namespace

// static
std::unique_ptr<base::ScopedClosureRunner>
AppListSyncableService::SetScopedModelUpdaterFactoryForTest(
    ModelUpdaterFactoryCallback callback) {
  // The idea is to bind both `callback` and `resetter`-s lifetimes to the
  // lifetime of the returned ScopedClosureRunner so that on destruction the
  // resetter will set the test factory to nullptr, and the callback itself will
  // be released too. `callback_on_heap` ensures pointer stability for
  // `g_model_updater_factory_callback_for_test_`.
  auto callback_on_heap =
      std::make_unique<ModelUpdaterFactoryCallback>(std::move(callback));
  g_model_updater_factory_callback_for_test_ = callback_on_heap.get();
  return std::make_unique<base::ScopedClosureRunner>(base::BindOnce(
      [](std::unique_ptr<ModelUpdaterFactoryCallback>) {
        g_model_updater_factory_callback_for_test_ = nullptr;
      },
      std::move(callback_on_heap)));
}

// AppListSyncableService::SyncItem

AppListSyncableService::SyncItem::SyncItem(
    const std::string& id,
    sync_pb::AppListSpecifics::AppListItemType type,
    bool is_new)
    : item_id(id), item_type(type), is_new(is_new) {}

AppListSyncableService::SyncItem::~SyncItem() = default;

// AppListSyncableService::Observer

AppListSyncableService::Observer::~Observer() {
  CHECK(!IsInObserverList());
}

// AppListSyncableService::ModelUpdaterObserver

class AppListSyncableService::ModelUpdaterObserver
    : public AppListModelUpdaterObserver {
 public:
  explicit ModelUpdaterObserver(AppListSyncableService* owner) : owner_(owner) {
    DVLOG(2) << owner_ << ": ModelUpdaterObserver Added";
    owner_->GetModelUpdater()->AddObserver(this);
  }
  ModelUpdaterObserver(const ModelUpdaterObserver&) = delete;
  ModelUpdaterObserver& operator=(const ModelUpdaterObserver&) = delete;
  ~ModelUpdaterObserver() override {
    owner_->GetModelUpdater()->RemoveObserver(this);
    DVLOG(2) << owner_ << ": ModelUpdaterObserver Removed";
  }

  void set_active(bool active) { active_ = active; }

 private:
  // ChromeAppListModelUpdaterObserver
  void OnAppListItemAdded(ChromeAppListItem* item) override {
    if (!active_)
      return;

    // Only sync folders and page breaks which are added from Ash.
    if (!item->is_folder())
      return;
    DCHECK(adding_item_id_.empty());
    adding_item_id_ = item->id();  // Ignore updates while adding an item.
    VLOG(2) << owner_ << " OnAppListItemAdded: " << item->ToDebugString();
    owner_->AddOrUpdateFromSyncItem(item);
    adding_item_id_.clear();

    // Sync OEM name if it was created on demand on ash side.
    if (item->id() == ash::kOemFolderId &&
        item->name() != owner_->oem_folder_name_) {
      owner_->GetModelUpdater()->SetItemName(item->id(),
                                             owner_->oem_folder_name_);
    }
  }

  void OnAppListItemWillBeDeleted(ChromeAppListItem* item) override {
    if (!active_)
      return;

    DCHECK(adding_item_id_.empty());
    VLOG(2) << owner_ << " OnAppListItemDeleted: " << item->ToDebugString();
    // Don't sync folder removal in case the folder still exists on another
    // device (e.g. with device specific items in it). Empty folders will be
    // deleted when the last item is removed (in PruneEmptySyncFolders()).
    if (item->is_folder())
      return;

    owner_->RemoveSyncItem(item->id());
  }

  void OnAppListItemUpdated(ChromeAppListItem* item) override {
    if (!active_)
      return;

    if (!adding_item_id_.empty()) {
      // Adding an item may trigger update notifications which should be
      // ignored.
      DCHECK_EQ(adding_item_id_, item->id());
      return;
    }
    VLOG(2) << owner_ << " OnAppListItemUpdated: " << item->ToDebugString();
    owner_->UpdateSyncItem(item);
  }

  const raw_ptr<AppListSyncableService> owner_;
  std::string adding_item_id_;

  // Whether the observer should handle model updated updates. The value is
  // managed by the owning `AppListSyncableService`, which will make sure the
  // observer is inactive while the model is being updated from the service.
  bool active_ = false;
};

// AppListSyncableService

// static
void AppListSyncableService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kAppListLocalState);
  registry->RegisterIntegerPref(
      prefs::kAppListPreferredOrder,
      static_cast<int>(ash::AppListSortOrder::kCustom),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

// static
bool AppListSyncableService::AppIsDefaultForTest(Profile* profile,
                                                 const std::string& id) {
  return AppIsDefault(profile, id);
}

// static
void AppListSyncableService::SetAppIsDefaultForTest(Profile* profile,
                                                    const std::string& id) {
  app_list::SetAppIsDefaultForTest(profile, id);
}

AppListSyncableService::AppListSyncableService(Profile* profile)
    : profile_(profile),
      extension_system_(extensions::ExtensionSystem::Get(profile)),
      extension_registry_(extensions::ExtensionRegistry::Get(profile)) {
  sync_model_sanitizer_ = std::make_unique<AppListSyncModelSanitizer>(this);
  if (g_model_updater_factory_callback_for_test_) {
    model_updater_ = g_model_updater_factory_callback_for_test_->Run(this);
  } else {
    model_updater_ = std::make_unique<ChromeAppListModelUpdater>(
        profile, this, sync_model_sanitizer_.get());
  }

  model_updater_observer_ = std::make_unique<ModelUpdaterObserver>(this);

  if (!extension_system_) {
    LOG(ERROR) << "AppListSyncableService created with no ExtensionSystem";
    return;
  }

  oem_folder_name_ =
      l10n_util::GetStringUTF8(IDS_APP_LIST_OEM_DEFAULT_FOLDER_NAME);

  auto ordinal = syncer::StringOrdinal::CreateInitialOrdinal();
  for (const auto& item :
       chromeos::default_app_order::GetAppPreloadServiceDefaults()) {
    preload_service_ordinals_[item] = ordinal;
    ordinal = ordinal.CreateAfter();
  }
  if (auto* app_preload_service = apps::AppPreloadService::Get(profile_)) {
    app_preload_service->GetLauncherOrdering(
        base::BindOnce(&AppListSyncableService::OnGetLauncherOrdering,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (IsExtensionServiceReady()) {
    BuildModel();
  } else {
    extension_system_->ready().Post(
        FROM_HERE, base::BindOnce(&AppListSyncableService::BuildModel,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

AppListSyncableService::~AppListSyncableService() {
  // Remove observers.
  model_updater_observer_.reset();

  model_updater_.reset();
}

bool AppListSyncableService::IsExtensionServiceReady() const {
  return extension_system_->is_ready();
}

void AppListSyncableService::InitFromLocalStorage() {
  // This should happen before sync and model is built.
  DCHECK(!sync_processor_.get());
  DCHECK(!IsInitialized());

  // Restore initial state from local storage.
  const base::Value::Dict& local_items =
      profile_->GetPrefs()->GetDict(prefs::kAppListLocalState);
  local_state_initially_empty_ = local_items.empty();

  for (auto [item_id, item] : local_items) {
    auto* item_dict = item.GetIfDict();
    if (!item_dict) {
      LOG(ERROR) << "Dictionary not found for " << item_id + ".";
      continue;
    }
    std::optional<int> type = item_dict->FindInt(kTypeKey);
    if (!type) {
      LOG(ERROR) << "Item type is not set in local storage for " << *item_dict
                 << ".";
      continue;
    }

    SyncItem* sync_item = CreateSyncItem(
        item_id, static_cast<sync_pb::AppListSpecifics::AppListItemType>(*type),
        /*is_new=*/false);

    const std::string* maybe_item_name = item_dict->FindString(kNameKey);
    if (maybe_item_name)
      sync_item->item_name = *maybe_item_name;
    const std::string* maybe_parent_id = item_dict->FindString(kParentIdKey);

    const std::string* maybe_promise_package_id =
        item_dict->FindString(kPromisePackageIdKey);
    if (maybe_promise_package_id && !maybe_promise_package_id->empty()) {
      sync_item->promise_package_id = *maybe_promise_package_id;
    }

    if (maybe_parent_id)
      sync_item->parent_id = *maybe_parent_id;

    const std::string* position = item_dict->FindString(kPositionKey);
    const std::string* pin_position = item_dict->FindString(kPinPositionKey);
    if (position && !position->empty())
      sync_item->item_ordinal = syncer::StringOrdinal(*position);
    if (pin_position && !pin_position->empty())
      sync_item->item_pin_ordinal = syncer::StringOrdinal(*pin_position);

    sync_item->empty_item_ordinal_fixable =
        item_dict->FindBool(kEmptyItemOrdinalFixable).value_or(true);

    // Fetch icon colors from `dict_item` if any.
    if (auto* background_color_internal_string =
            item_dict->FindString(kBackgroundColorKey)) {
      // Retrieve the background color.
      sync_pb::AppListSpecifics::ColorGroup background_color;
      sync_pb::AppListSpecifics::ColorGroup_Parse(
          background_color_internal_string ? *background_color_internal_string
                                           : std::string(),
          &background_color);

      // Retrieve the hue.
      DCHECK(item_dict->Find(kHueKey));
      int hue =
          item_dict->FindInt(kHueKey).value_or(ash::IconColor::kHueInvalid);

      sync_item->item_color = ash::IconColor(background_color, hue);

      // Assume that the color saved in pref is valid.
      DCHECK(sync_item->item_color.IsValid());
    }

    ProcessNewSyncItem(sync_item);
  }
}

bool AppListSyncableService::IsInitialized() const {
  return app_service_apps_builder_.get();
}

bool AppListSyncableService::IsSyncing() const {
  return sync_processor_.get();
}

void AppListSyncableService::BuildModel() {
  InitFromLocalStorage();

  DCHECK(IsExtensionServiceReady());
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListControllerDelegate* controller = client;

  app_service_apps_builder_ =
      std::make_unique<AppServiceAppModelBuilder>(controller);
  if (ash::features::ArePromiseIconsEnabled()) {
    app_service_promise_apps_builder_ =
        std::make_unique<AppServicePromiseAppModelBuilder>(controller);
  }

  DCHECK(profile_);
  SyncStarted();

  app_service_apps_builder_->Initialize(this, profile_, model_updater_.get());
  if (ash::features::ArePromiseIconsEnabled()) {
    app_service_promise_apps_builder_->Initialize(this, profile_,
                                                  model_updater_.get());
  }

  HandleUpdateFinished(false /* clean_up_after_init_sync */);

  if (wait_until_ready_to_sync_cb_)
    std::move(wait_until_ready_to_sync_cb_).Run();
}

void AppListSyncableService::AddObserverAndStart(Observer* observer) {
  observer_list_.AddObserver(observer);
  SyncStarted();
}

void AppListSyncableService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AppListSyncableService::OnFirstSync(
    base::OnceCallback<void(bool was_first_sync_ever)> callback) {
  // NOTE: Do not use `base::Unretained(this)` with `on_first_sync_.Post()`
  // since `base::OneShotEvent` does not own the underlying task runner.
  on_first_sync_.Post(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<const AppListSyncableService>& self,
             base::OnceCallback<void(bool was_first_sync_ever)> callback) {
            if (self) {
              CHECK(self->first_sync_was_first_sync_ever_.has_value());
              std::move(callback).Run(*self->first_sync_was_first_sync_ever_);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AppListSyncableService::NotifyObserversSyncUpdated() {
  for (auto& observer : observer_list_)
    observer.OnSyncModelUpdated();
}

const AppListSyncableService::SyncItem* AppListSyncableService::GetSyncItem(
    const std::string& id) const {
  auto iter = sync_items_.find(id);
  if (iter != sync_items_.end())
    return iter->second.get();
  return nullptr;
}

void AppListSyncableService::AppListSyncableService::AddPageBreakItem(
    const std::string& id,
    const syncer::StringOrdinal& position) {
  SyncItem* page_break = CreateSyncItem(
      id, sync_pb::AppListSpecifics::TYPE_PAGE_BREAK, /*is_new=*/true);
  page_break->item_ordinal = position;
  ProcessNewSyncItem(page_break);
  UpdateSyncItemInLocalStorage(profile_, page_break);
  SendSyncChange(page_break, SyncChange::ACTION_ADD);
}

bool AppListSyncableService::TransferItemAttributes(
    const std::string& from_app_id,
    const std::string& to_app_id) {
  const SyncItem* from_item = FindSyncItem(from_app_id);
  if (!from_item ||
      from_item->item_type != sync_pb::AppListSpecifics::TYPE_APP) {
    return false;
  }

  auto attributes = std::make_unique<SyncItem>(
      from_app_id, sync_pb::AppListSpecifics::TYPE_APP, /*is_new=*/false);
  attributes->promise_package_id = from_item->promise_package_id;
  attributes->parent_id = from_item->parent_id;
  attributes->item_ordinal = from_item->item_ordinal;
  attributes->item_pin_ordinal = from_item->item_pin_ordinal;
  attributes->item_color = from_item->item_color;
  attributes->is_user_pinned = from_item->is_user_pinned;

  SyncItem* to_item = FindSyncItem(to_app_id);
  if (to_item) {
    // |to_app_id| already exists. Can apply attributes right now.
    ApplyAppAttributes(to_app_id, std::move(attributes));
  } else {
    // |to_app_id| does not exist at this moment. Store attributes to apply it
    // later once app appears on this device.
    pending_transfer_map_[to_app_id] = std::move(attributes);
  }

  return true;
}

void AppListSyncableService::ApplyAppAttributes(
    const std::string& app_id,
    std::unique_ptr<SyncItem> attributes) {
  SyncItem* item = FindSyncItem(app_id);
  if (!item || item->item_type != sync_pb::AppListSpecifics::TYPE_APP) {
    LOG(ERROR) << "Failed to apply app attributes, app " << app_id
               << " does not exist.";
    return;
  }

  HandleUpdateStarted();

  item->promise_package_id = attributes->promise_package_id;
  item->parent_id = attributes->parent_id;
  item->item_ordinal = attributes->item_ordinal;
  item->item_pin_ordinal = attributes->item_pin_ordinal;
  item->is_user_pinned = attributes->is_user_pinned;
  item->item_color = attributes->item_color;

  UpdateSyncItemInLocalStorage(profile_, item);
  SendSyncChange(item, SyncChange::ACTION_UPDATE);
  ProcessExistingSyncItem(item);

  HandleUpdateFinished(false /* clean_up_after_init_sync */);
}

void AppListSyncableService::SetOemFolderName(const std::string& name) {
  oem_folder_name_ = name;

  // Update OEM folder item if it was already created. If it is not created yet
  // then on creation it will take right name.
  model_updater_->SetItemName(ash::kOemFolderId, oem_folder_name_);
}

AppListModelUpdater* AppListSyncableService::GetModelUpdater() {
  return model_updater_.get();
}

void AppListSyncableService::HandleUpdateStarted() {
  // Don't observe the model while processing update changes.
  model_updater_observer_->set_active(false);
}

void AppListSyncableService::HandleUpdateFinished(
    bool clean_up_after_init_sync) {
  // Processing an update may create folders without setting their positions.
  // Resolve them now.
  ResolveFolderPositions();

  if (clean_up_after_init_sync) {
    PruneEmptySyncFolders();
  }

  // Resume or start observing app list model changes.
  model_updater_observer_->set_active(true);

  NotifyObserversSyncUpdated();
}

void AppListSyncableService::AddItem(
    std::unique_ptr<ChromeAppListItem> app_item) {
  bool using_default_position = false;
  const bool use_default_positions_for_new_users_only =
      IsAppDefaultPositionedForNewUsersOnly(app_item->id());

  // Values are set if AppPreloadService is used for setting position.
  syncer::StringOrdinal default_position;
  std::string folder_id;
  std::string folder_name;
  syncer::StringOrdinal folder_position;

  // Sets `app_item`'s position before adding the sync item so that the created
  // sync item has the valid position.
  if (!app_item->position().IsValid()) {
    const bool consider_default_position =
        !use_default_positions_for_new_users_only ||
        ((!initial_sync_data_processed_ || first_app_list_sync_) &&
         local_state_initially_empty_);

    if (base::FeatureList::IsEnabled(
            apps::kAppPreloadServiceEnableLauncherOrder) &&
        GetAppPreloadServiceInfo(app_item.get(), &default_position, &folder_id,
                                 &folder_name, &folder_position)) {
    } else {
      default_position = app_item->CalculateDefaultPositionIfApplicable();
    }
    if (consider_default_position && default_position.IsValid()) {
      app_item->SetChromePosition(default_position);
      using_default_position = true;
    } else {
      InitNewItemPosition(app_item.get());
    }
  }

  // When `app_item` is installed from the local device, `app_item`'s sync data
  // does not exist until `FindOrAddSyncItem()` is called.
  const bool is_item_new = !FindSyncItem(app_item->id());

  SyncItem* sync_item = FindOrAddSyncItem(app_item.get());
  if (!sync_item)
    return;  // Item is not valid.

  if (use_default_positions_for_new_users_only && using_default_position &&
      !initial_sync_data_processed_) {
    sync_item->ordinal_to_undo_on_non_empty_initial_sync = app_item->position();
  }

  if (app_item->is_folder()) {
    model_updater_->AddItem(std::move(app_item));
  } else if (!folder_id.empty()) {
    VLOG(2) << this << ": AddItem to APS folder id: " << folder_id
            << ", name: " << folder_name
            << ", pos: " << folder_position.ToDebugString()
            << ", item: " << sync_item->ToString();
    EnsureFolderExists(folder_id, folder_name, folder_position);
    model_updater_->AddAppItemToFolder(std::move(app_item), folder_id,
                                       is_item_new);
  } else if (AppIsOem(app_item->id())) {
    VLOG(2) << this << ": AddItem to OEM folder: " << sync_item->ToString();
    EnsureFolderExists(ash::kOemFolderId, oem_folder_name_,
                       syncer::StringOrdinal());
    model_updater_->AddAppItemToFolder(std::move(app_item), ash::kOemFolderId,
                                       is_item_new);
  } else {
    folder_id = sync_item->parent_id;
    VLOG(2) << this << ": AddItem: " << sync_item->ToString() << " Folder: '"
            << folder_id << "'";

    if (folder_id == ash::kCrostiniFolderId ||
        folder_id == ash::kBruschettaFolderId) {
      MaybeAddOrUpdateGuestOsFolderSyncData(folder_id);
    }

    // Create a folder if `app_item`'s parent folder does not exist.
    if (!folder_id.empty()) {
      const bool folder_exists =
          MaybeCreateFolderBeforeAddingItem(app_item.get(), folder_id);
      // If `MaybeCreateFolderBeforeAddingItem()` failed to create the folder,
      // move the app to the root app item list.
      if (!folder_exists)
        folder_id.clear();
    }

    model_updater_->AddAppItemToFolder(std::move(app_item), folder_id,
                                       is_item_new);
  }

  PruneRedundantPageBreakItems();
}

AppListSyncableService::SyncItem* AppListSyncableService::FindOrAddSyncItem(
    const ChromeAppListItem* app_item) {
  const std::string& item_id = app_item->id();
  if (item_id.empty()) {
    LOG(ERROR) << "ChromeAppListItem item with empty ID";
    return nullptr;
  }
  SyncItem* sync_item = FindSyncItem(item_id);
  if (sync_item) {
    // If there is an existing, non-REMOVE_DEFAULT entry, return it.
    if (sync_item->item_type !=
        sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
      DVLOG(2) << this << ": AddItem already exists: " << sync_item->ToString();
      return sync_item;
    }

    if (RemoveDefaultApp(app_item, sync_item))
      return nullptr;

    // Fall through. The REMOVE_DEFAULT_APP entry has been deleted, now a new
    // App entry can be added.
  }

  return CreateSyncItemFromAppItem(app_item);
}

AppListSyncableService::SyncItem*
AppListSyncableService::CreateSyncItemFromAppItem(
    const ChromeAppListItem* app_item) {
  sync_pb::AppListSpecifics::AppListItemType type =
      GetAppListItemType(app_item);
  VLOG(2) << this << " CreateSyncItemFromAppItem:" << app_item->ToDebugString();
  SyncItem* sync_item = CreateSyncItem(app_item->id(), type, /*is_new=*/true);
  DCHECK(app_item->position().IsValid());
  UpdateSyncItemFromAppItem(app_item, sync_item);
  UpdateSyncItemInLocalStorage(profile_, sync_item);
  SendSyncChange(sync_item, SyncChange::ACTION_ADD);
  return sync_item;
}

syncer::StringOrdinal AppListSyncableService::GetPinPosition(
    const std::string& app_id) {
  SyncItem* sync_item = FindSyncItem(app_id);
  if (!sync_item)
    return syncer::StringOrdinal();
  return sync_item->item_pin_ordinal;
}

void AppListSyncableService::SetPinPosition(
    const std::string& app_id,
    const syncer::StringOrdinal& item_pin_ordinal,
    bool pinned_by_policy) {
  DCHECK(item_pin_ordinal.IsValid());

  // Pin position can be set only after model is built.
  DCHECK(IsInitialized());

  SyncItem* sync_item = FindSyncItem(app_id);
  SyncChange::SyncChangeType sync_change_type;
  if (sync_item) {
    sync_change_type = SyncChange::ACTION_UPDATE;
  } else {
    // Pin position for apps that don't have a sync item can be set for
    // installed/pinned by default apps. Don't mark those apps as new, as they
    // are considered internally installed.
    sync_item = CreateSyncItem(app_id, sync_pb::AppListSpecifics::TYPE_APP,
                               /*is_new=*/false);
    sync_change_type = SyncChange::ACTION_ADD;
    // Prevent item ordinal from getting set by "fixing empty ordinals" until
    // the app gets installed, and item ordinal gets set to its default value.
    // At this point, sync item is added primarily to initialize default shelf
    // pin order, and the associnated app may not be fully initialized.
    sync_item->empty_item_ordinal_fixable = false;
  }

  sync_item->item_pin_ordinal = item_pin_ordinal;
  if (ash::features::IsRemoveStalePolicyPinnedAppsFromShelfEnabled()) {
    // If `is_user_pinned` is currently `true`, it cannot become `false` unless
    // the user decides to unpin the app manually.
    // Conversely, `is_user_pinned` which is currently `false` can only become
    // `true` if the policy changes and the app gets removed from the shelf,
    // after which the user decides to pin the app again.
    // In other words, transitions from `true` to `false` and vice versa must
    // involve the `std::nullopt` phase.
    if (!sync_item->is_user_pinned.has_value()) {
      sync_item->is_user_pinned = !pinned_by_policy;
    }
  }

  UpdateSyncItemInLocalStorage(profile_, sync_item);
  SendSyncChange(sync_item, sync_change_type);

  const auto promised_sync_item = items_linked_to_promise_item_.find(app_id);
  if (promised_sync_item != items_linked_to_promise_item_.end() &&
      !promised_sync_item->second.empty()) {
    SetPinPosition(promised_sync_item->second, item_pin_ordinal,
                   pinned_by_policy);
  }
}

void AppListSyncableService::CopyPromiseItemAttributesToItem(
    const std::string& promise_app_id,
    const std::string& target_id) {
  const SyncItem* promise_item = FindSyncItem(promise_app_id);
  if (!promise_item) {
    return;
  }

  CHECK_EQ(promise_item->item_type, sync_pb::AppListSpecifics::TYPE_APP);
  CHECK(promise_item->is_ephemeral);

  bool changed = false;
  SyncItem* sync_item = FindSyncItem(target_id);
  if (sync_item && sync_item->item_type ==
      sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    DeleteSyncItem(target_id);
    sync_item = nullptr;
  }
  SyncChange::SyncChangeType sync_change_type;
  if (sync_item) {
    CHECK_EQ(sync_item->item_type, sync_pb::AppListSpecifics::TYPE_APP);
    sync_change_type = SyncChange::ACTION_UPDATE;
  } else {
    changed = true;
    sync_item = CreateSyncItem(target_id, sync_pb::AppListSpecifics::TYPE_APP,
                               /*is_new=*/true);
    sync_change_type = SyncChange::ACTION_ADD;
  }

  if (sync_item->parent_id != promise_item->parent_id) {
    changed = true;
    sync_item->parent_id = promise_item->parent_id;
  }

  if (sync_item->item_ordinal != promise_item->item_ordinal) {
    changed = true;
    sync_item->item_ordinal = promise_item->item_ordinal;
  }

  if (sync_item->item_pin_ordinal != promise_item->item_pin_ordinal) {
    changed = true;
    sync_item->item_pin_ordinal = promise_item->item_pin_ordinal;
  }

  if (sync_item->promise_package_id != promise_app_id) {
    changed = true;
    sync_item->promise_package_id = promise_app_id;
  }

  if (!changed) {
    return;
  }

  UpdateSyncItemInLocalStorage(profile_, sync_item);
  SendSyncChange(sync_item, sync_change_type);
}

void AppListSyncableService::SetIsPolicyPinned(const std::string& app_id) {
  // Pin position can be set only after model is built.
  DCHECK(IsInitialized());

  SyncItem* sync_item = FindSyncItem(app_id);
  CHECK(sync_item);
  CHECK(sync_item->item_pin_ordinal.IsValid());
  CHECK(!sync_item->is_user_pinned.has_value());
  sync_item->is_user_pinned = false;

  UpdateSyncItemInLocalStorage(profile_, sync_item);
  SendSyncChange(sync_item, SyncChange::ACTION_UPDATE);
}

void AppListSyncableService::RemovePinPosition(const std::string& app_id) {
  // Pin position can be set only after model is built.
  DCHECK(IsInitialized());

  SyncItem* sync_item = FindSyncItem(app_id);
  // No need to default-initialize already removed items.
  if (!sync_item) {
    return;
  }

  sync_item->item_pin_ordinal = syncer::StringOrdinal();
  sync_item->is_user_pinned = std::nullopt;

  UpdateSyncItemInLocalStorage(profile_, sync_item);
  SendSyncChange(sync_item, syncer::SyncChange::SyncChangeType::ACTION_UPDATE);
}

void AppListSyncableService::AddOrUpdateFromSyncItem(
    const ChromeAppListItem* app_item) {
  for (auto& observer : observer_list_)
    observer.OnAddOrUpdateFromSyncItemForTest();

  DCHECK(app_item->position().IsValid());

  SyncItem* sync_item = FindSyncItem(app_item->id());
  if (sync_item) {
    model_updater_->UpdateAppItemFromSyncItem(
        sync_item,
        sync_item->item_id !=
            ash::kOemFolderId,  // Don't sync oem folder's name.
        false);                 // Don't sync its folder here.
    if (!sync_item->item_ordinal.IsValid()) {
      UpdateSyncItem(app_item);
      VLOG(2) << "Flushing position to sync item " << sync_item;
    }
    return;
  }
  CreateSyncItemFromAppItem(app_item);
}

bool AppListSyncableService::RemoveDefaultApp(const ChromeAppListItem* item,
                                              SyncItem* sync_item) {
  CHECK_EQ(sync_item->item_type,
           sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP);

  // If there is an existing REMOVE_DEFAULT_APP entry, and the app is
  // installed as a Default app, uninstall the app instead of adding it.
  if (sync_item->item_type == sync_pb::AppListSpecifics::TYPE_APP &&
      AppIsDefault(profile_, item->id())) {
    VLOG(2) << this
            << ": HandleDefaultApp: Uninstall: " << sync_item->ToString();
    UninstallExtension(extension_system_->extension_service(),
                       extension_registry_, item->id());
    return true;
  }

  // Otherwise, we are adding the app as a non-default app (i.e. an app that
  // was installed by Default and removed is getting installed explicitly by
  // the user), so delete the REMOVE_DEFAULT_APP.
  DeleteSyncItem(sync_item->item_id);
  return false;
}

bool AppListSyncableService::InterceptDeleteDefaultApp(SyncItem* sync_item) {
  if (sync_item->item_type != sync_pb::AppListSpecifics::TYPE_APP ||
      !AppIsDefault(profile_, sync_item->item_id)) {
    return false;
  }

  // This is a Default app; update the entry to a REMOVE_DEFAULT entry.
  // This will overwrite any existing entry for the item.
  VLOG(2) << this << " -> SYNC UPDATE: REMOVE_DEFAULT: " << sync_item->item_id;
  sync_item->item_type = sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP;
  UpdateSyncItemInLocalStorage(profile_, sync_item);
  SendSyncChange(sync_item, SyncChange::ACTION_UPDATE);
  return true;
}

void AppListSyncableService::DeleteSyncItem(const std::string& item_id) {
  SyncItem* sync_item = FindSyncItem(item_id);
  if (!sync_item) {
    LOG(ERROR) << "DeleteSyncItem: no sync item: " << item_id;
    return;
  }
  if (SyncStarted()) {
    VLOG(2) << this << " -> SYNC DELETE: " << sync_item->ToString();
    SyncChange sync_change(FROM_HERE, SyncChange::ACTION_DELETE,
                           GetSyncDataFromSyncItem(sync_item));
    sync_processor_->ProcessSyncChanges(FROM_HERE,
                                        syncer::SyncChangeList(1, sync_change));
  }
  RemoveSyncItemFromLocalStorage(profile_, item_id);
  sync_items_.erase(item_id);
}

void AppListSyncableService::UpdateSyncItem(const ChromeAppListItem* app_item) {
  SyncItem* sync_item = FindSyncItem(app_item->id());
  if (!sync_item) {
    LOG(ERROR) << "UpdateItem: no sync item: " << app_item->id();
    return;
  }
  bool changed = UpdateSyncItemFromAppItem(app_item, sync_item);
  if (!changed) {
    DVLOG(2) << this << " - Update: SYNC NO CHANGE: " << sync_item->ToString();
    return;
  }
  UpdateSyncItemInLocalStorage(profile_, sync_item);
  SendSyncChange(sync_item, SyncChange::ACTION_UPDATE);

  if (!app_item->GetPromisedItemId().empty()) {
    CopyPromiseItemAttributesToItem(app_item->id(),
                                    app_item->GetPromisedItemId());
  }

  PruneRedundantPageBreakItems();
}

syncer::StringOrdinal AppListSyncableService::GetDefaultOemFolderPosition()
    const {
  // Calculate the OEM folder position:
  // *   If OEM folder is in sync data, respect the existing item position.
  // *   If the user has non-default apps in sync, the OEM folder is added as
  //     the last item in the model.
  // *   If the user has only default apps in sync data, add OEM folder after
  //     webstore item.
  if (first_app_list_sync_) {
    syncer::StringOrdinal position_after_webstore =
        GetPositionAfterApp(extensions::kWebStoreAppId);
    if (position_after_webstore.IsValid())
      return position_after_webstore;
  }

  return GetLastPosition();
}

syncer::StringOrdinal AppListSyncableService::GetLastPosition() const {
  syncer::StringOrdinal largest_ordinal;
  for (const auto& [item_id, sync_item] : sync_items_) {
    if (sync_item->item_ordinal.IsValid() &&
        (!largest_ordinal.IsValid() ||
         sync_item->item_ordinal.GreaterThan(largest_ordinal))) {
      largest_ordinal = sync_item->item_ordinal;
    }
  }
  if (largest_ordinal.IsValid())
    return largest_ordinal.CreateAfter();
  return syncer::StringOrdinal::CreateInitialOrdinal();
}

syncer::StringOrdinal AppListSyncableService::GetPositionAfterApp(
    const std::string& app_id) const {
  const SyncItem* app_item = GetSyncItem(app_id);
  if (!app_item || !app_item->item_ordinal.IsValid())
    return syncer::StringOrdinal();

  syncer::StringOrdinal next_item;
  for (const auto& [item_id, sync_item] : sync_items_) {
    if (sync_item->item_ordinal.IsValid() &&
        sync_item->item_ordinal.GreaterThan(app_item->item_ordinal) &&
        (!next_item.IsValid() ||
         next_item.GreaterThan(sync_item->item_ordinal))) {
      next_item = sync_item->item_ordinal;
    }
  }

  if (next_item.IsValid())
    return app_item->item_ordinal.CreateBetween(next_item);

  return app_item->item_ordinal.CreateAfter();
}

std::optional<AppListSyncableService::LinkedPromiseAppSyncItem>
AppListSyncableService::CreateLinkedPromiseSyncItemIfAvailable(
    const std::string& promise_package_id) {
  auto linked_item_it = items_linked_to_promise_item_.find(promise_package_id);
  if (linked_item_it != items_linked_to_promise_item_.end()) {
    if (linked_item_it->second.empty()) {
      return std::nullopt;
    }
    return LinkedPromiseAppSyncItem{
        .linked_item_id = linked_item_it->second,
        .promise_item = FindSyncItem(linked_item_it->first)};
  }

  const SyncItem* linked_sync_item = nullptr;
  for (const auto& [item_id, sync_item] : sync_items_) {
    if (sync_item->item_type == sync_pb::AppListSpecifics::TYPE_APP &&
        sync_item->promise_package_id == promise_package_id &&
        sync_item->item_id != promise_package_id) {
      linked_sync_item = sync_item.get();
      break;
    }
  }

  // If a linked sync item does not exist, register an empty target item ID, so
  // subsequent `CreateLinkedPromiseSyncItemIfAvailable()` calls consistently
  // return no linkage (even in the edge case a sync item appears while promise
  // app is installing - in this case the promise app item attributes will be
  // moved to the target app sync item once the promise app installs).
  if (!linked_sync_item) {
    items_linked_to_promise_item_.emplace(promise_package_id, "");
    return std::nullopt;
  }

  SyncItem* sync_item = CreateSyncItem(
      promise_package_id, linked_sync_item->item_type, /*is_new=*/false);
  sync_item->is_ephemeral = true;
  CopyAttributesToSyncItem(linked_sync_item, sync_item);

  items_linked_to_promise_item_.emplace(promise_package_id,
                                        linked_sync_item->item_id);
  return LinkedPromiseAppSyncItem{.linked_item_id = linked_sync_item->item_id,
                                  .promise_item = sync_item};
}

void AppListSyncableService::RemoveItem(const std::string& id,
                                        bool is_uninstall) {
  RemoveSyncItem(id);
  model_updater_->RemoveItem(id, is_uninstall);

  items_linked_to_promise_item_.erase(id);

  PruneEmptySyncFolders();
  PruneRedundantPageBreakItems();
}

void AppListSyncableService::UpdateItem(const ChromeAppListItem* app_item) {
  // Check to see if the item needs to be moved to/from the OEM folder.
  bool is_oem = AppIsOem(app_item->id());
  if (!is_oem && app_item->folder_id() == ash::kOemFolderId)
    model_updater_->SetItemFolderId(app_item->id(), "");
  else if (is_oem && app_item->folder_id() != ash::kOemFolderId)
    model_updater_->SetItemFolderId(app_item->id(), ash::kOemFolderId);
}

void AppListSyncableService::RemoveSyncItem(const std::string& id) {
  VLOG(2) << this << ": RemoveSyncItem: " << id.substr(0, 8);
  auto iter = sync_items_.find(id);
  if (iter == sync_items_.end()) {
    DVLOG(2) << this << " : RemoveSyncItem: No Item.";
    return;
  }

  // Check for existing RemoveDefault sync item.
  const auto& [item_id, sync_item] = *iter;
  sync_pb::AppListSpecifics::AppListItemType type = sync_item->item_type;
  if (type == sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    // RemoveDefault item exists, just return.
    DVLOG(2) << this << " : RemoveDefault Item exists.";
    return;
  }

  // Check if we're asked to remove a default-installed app.
  if (InterceptDeleteDefaultApp(sync_item.get())) {
    return;
  }

  DeleteSyncItem(item_id);
}

void AppListSyncableService::ResolveFolderPositions() {
  VLOG(2) << "ResolveFolderPositions.";
  for (const auto& [item_id, sync_item] : sync_items_) {
    if (sync_item->item_type != sync_pb::AppListSpecifics::TYPE_FOLDER)
      continue;

    model_updater_->UpdateAppItemFromSyncItem(
        sync_item.get(),
        sync_item->item_id !=
            ash::kOemFolderId,  // Don't sync oem folder's name.
        false);                 // Don't sync its folder here.
  }
}

void AppListSyncableService::PruneEmptySyncFolders() {
  std::set<std::string> parent_ids;
  for (const auto& [item_id, sync_item] : sync_items_) {
    parent_ids.insert(sync_item->parent_id);
  }

  for (auto iter = sync_items_.begin(); iter != sync_items_.end();) {
    SyncItem* sync_item = (iter++)->second.get();
    if (sync_item->item_type != sync_pb::AppListSpecifics::TYPE_FOLDER)
      continue;

    // Do not prune OEM folder - OEM app sync items will not have the parent
    // ID set to OEM folder, so OEM folder will not be listed in `parent_ids`.
    // Additionally, even if the folder is empty / not needed on this device,
    // it may exist on another user's device. Deleting it from sync would
    // invalidate the folder position on other devices.
    if (sync_item->item_id == ash::kOemFolderId)
      continue;

    if (!base::Contains(parent_ids, sync_item->item_id))
      DeleteSyncItem(sync_item->item_id);
  }
}

void AppListSyncableService::PopulateSyncItemsForTest(
    std::vector<std::unique_ptr<SyncItem>>&& items) {
  for (auto& sync_item : items) {
    const bool success =
        sync_items_
            .emplace(std::make_pair(sync_item->item_id, std::move(sync_item)))
            .second;
    DCHECK(success);
  }
}

const AppListSyncableService::SyncItemMap& AppListSyncableService::sync_items()
    const {
  return sync_items_;
}

void AppListSyncableService::WaitUntilReadyToSync(base::OnceClosure done) {
  DCHECK(!wait_until_ready_to_sync_cb_);

  if (IsInitialized()) {
    std::move(done).Run();
  } else {
    // Wait until initialization is completed in BuildModel();
    wait_until_ready_to_sync_cb_ = std::move(done);
  }
}

std::optional<syncer::ModelError>
AppListSyncableService::MergeDataAndStartSyncing(
    syncer::DataType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) {
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());

  HandleUpdateStarted();

  // Reset local state and recreate from sync info.
  ScopedDictPrefUpdate pref_update(profile_->GetPrefs(),
                                   prefs::kAppListLocalState);
  pref_update->clear();

  sync_processor_ = std::move(sync_processor);

  VLOG(2) << this << ": MergeDataAndStartSyncing: " << initial_sync_data.size();

  // Copy all sync items to |unsynced_items|.
  std::set<std::string> unsynced_items;
  for (const auto& [item_id, sync_item] : sync_items_) {
    unsynced_items.insert(item_id);
  }

  // Create SyncItem entries for initial_sync_data.
  for (const auto& data : initial_sync_data) {
    const auto& specifics = data.GetSpecifics().app_list();
    const std::string& item_id = specifics.item_id();
    DVLOG(2) << this << "  Initial Sync Item: " << item_id
             << " Type: " << specifics.item_type();
    DCHECK_EQ(syncer::APP_LIST, data.GetDataType());
    ProcessSyncItemSpecifics(specifics);
    if (specifics.item_type() != sync_pb::AppListSpecifics::TYPE_FOLDER &&
        !IsUnRemovableDefaultApp(item_id) && !AppIsOem(item_id) &&
        !AppIsDefault(profile_, item_id)) {
      VLOG(2) << "Syncing non-default item: " << item_id;
      first_app_list_sync_ = false;
    }
    unsynced_items.erase(item_id);
  }
  // Initial sync data has been processed, it is safe now to add new sync
  // items.
  initial_sync_data_processed_ = true;

  // Send unsynced items.
  syncer::SyncChangeList change_list;
  for (const auto& item_id : unsynced_items) {
    SyncItem* sync_item = FindSyncItem(item_id);
    // Sync can cause an item to change folders, causing an unsynced folder
    // item to be removed.
    if (!sync_item)
      continue;

    VLOG(2) << this << " -> SYNC ADD: " << sync_item->ToString();

    if (!first_app_list_sync_ &&
        GetPermanentSortingOrder() == ash::AppListSortOrder::kCustom &&
        sync_item->ordinal_to_undo_on_non_empty_initial_sync ==
            sync_item->item_ordinal) {
      sync_item->item_ordinal = CalculateGlobalFrontPosition();
      model_updater_->UpdateAppItemFromSyncItem(sync_item, false, false);
    }
    sync_item->ordinal_to_undo_on_non_empty_initial_sync.reset();

    if (sync_item->item_id == ash::kOemFolderId &&
        oem_folder_using_provisional_default_position_) {
      sync_item->item_ordinal = GetDefaultOemFolderPosition();
      model_updater_->UpdateAppItemFromSyncItem(sync_item, false, false);
    }

    UpdateSyncItemInLocalStorage(profile_, sync_item);
    change_list.emplace_back(FROM_HERE, SyncChange::ACTION_ADD,
                             GetSyncDataFromSyncItem(sync_item));
  }

  oem_folder_using_provisional_default_position_ = false;

  // Fix items that do not contain valid app list position, required for
  // builds prior to M53 (crbug.com/677647).
  for (const auto& [item_id, sync_item] : sync_items_) {
    sync_item->ordinal_to_undo_on_non_empty_initial_sync.reset();
    if (sync_item->item_type != sync_pb::AppListSpecifics::TYPE_APP ||
        sync_item->item_ordinal.IsValid() ||
        !sync_item->empty_item_ordinal_fixable) {
      continue;
    }

    const ChromeAppListItem* app_item =
        model_updater_->FindItem(sync_item->item_id);
    if (app_item) {
      if (UpdateSyncItemFromAppItem(app_item, sync_item.get())) {
        VLOG(2) << "Fixing sync item from existing app: " << sync_item;
      } else {
        sync_item->item_ordinal = syncer::StringOrdinal::CreateInitialOrdinal();
        VLOG(2) << "Failed to fix sync item from existing app. "
                << "Generating new position ordinal: " << sync_item;
      }
    } else {
      sync_item->item_ordinal = syncer::StringOrdinal::CreateInitialOrdinal();
      VLOG(2) << "Fixing sync item by generating new position ordinal: "
              << sync_item;
    }
    change_list.emplace_back(FROM_HERE, SyncChange::ACTION_UPDATE,
                             GetSyncDataFromSyncItem(sync_item.get()));
  }

  if (ash::features::IsRemoveStalePolicyPinnedAppsFromShelfEnabled()) {
    std::vector<std::string> policy_pinned_apps =
        ChromeShelfPrefs::GetAppsPinnedByPolicy(profile_);
    if (!policy_pinned_apps.empty()) {
      for (const auto& [item_id, sync_item] : sync_items_) {
        // Only current policy-pinned apps that do not yet have a pinning source
        // defined are processed to minimize the number of sync messages
        // exchanged. This helps keep the logic flexible and gives the admins a
        // chance to unpin unwanted apps later during the transition period.
        if (sync_item->item_pin_ordinal.IsValid() &&
            !sync_item->is_user_pinned.has_value() &&
            base::Contains(policy_pinned_apps, item_id)) {
          sync_item->is_user_pinned = false;
          change_list.emplace_back(FROM_HERE, SyncChange::ACTION_UPDATE,
                                   GetSyncDataFromSyncItem(sync_item.get()));
        }
      }
    }
  }

  sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);

  HandleUpdateFinished(true /* clean_up_after_init_sync */);

  // Signal completion of the first sync in the session once and only once.
  if (!on_first_sync_.is_signaled()) {
    first_sync_was_first_sync_ever_ = first_app_list_sync_;
    on_first_sync_.Signal();
  }

  return std::nullopt;
}

void AppListSyncableService::StopSyncing(syncer::DataType type) {
  DCHECK_EQ(type, syncer::APP_LIST);

  sync_processor_.reset();
}

syncer::SyncDataList AppListSyncableService::GetAllSyncDataForTesting() const {
  VLOG(2) << this << ": GetAllSyncData: " << sync_items_.size();
  syncer::SyncDataList list;
  for (const auto& [item_id, sync_item] : sync_items_) {
    VLOG(2) << this << " -> SYNC: " << sync_item->ToString();
    list.push_back(GetSyncDataFromSyncItem(sync_item.get()));
  }
  return list;
}

std::optional<syncer::ModelError> AppListSyncableService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!sync_processor_.get()) {
    return syncer::ModelError(FROM_HERE,
                              "App List syncable service is not started.");
  }

  HandleUpdateStarted();

  VLOG(2) << this << ": ProcessSyncChanges: " << change_list.size();
  for (const auto& change : change_list) {
    VLOG(2) << this << "  Change: "
            << change.sync_data().GetSpecifics().app_list().item_id() << " ("
            << change.change_type() << ")";
    if (change.change_type() == SyncChange::ACTION_ADD ||
        change.change_type() == SyncChange::ACTION_UPDATE) {
      ProcessSyncItemSpecifics(change.sync_data().GetSpecifics().app_list());
    } else if (change.change_type() == SyncChange::ACTION_DELETE) {
      DeleteSyncItemSpecifics(change.sync_data().GetSpecifics().app_list());
    } else {
      LOG(ERROR) << "Invalid sync change";
    }
  }

  HandleUpdateFinished(false /* clean_up_after_init_sync */);

  return std::nullopt;
}

base::WeakPtr<syncer::SyncableService> AppListSyncableService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AppListSyncableService::Shutdown() {
  app_service_apps_builder_.reset();
  if (ash::features::ArePromiseIconsEnabled()) {
    app_service_promise_apps_builder_.reset();
  }
}

void AppListSyncableService::SetAppListPreferredOrder(
    ash::AppListSortOrder order) {
  // Update the preferred order that is shared among syncable devices.
  profile_->GetPrefs()->SetInteger(prefs::kAppListPreferredOrder,
                                   static_cast<int>(order));

  if (order == ash::AppListSortOrder::kCustom) {
    return;
  }

  // Too few sync items. Return early.
  if (sync_items_.size() < 2)
    return;

  const auto reorder_params =
      reorder::GenerateReorderParamsForSyncItems(order, sync_items_);
  for (const auto& reorder_param : reorder_params) {
    sync_pb::AppListSpecifics specifics;
    SyncItem* sync_item = FindSyncItem(reorder_param.sync_item_id);
    const syncer::StringOrdinal& old_ordinal = sync_item->item_ordinal;
    const syncer::StringOrdinal& new_ordinal = reorder_param.ordinal;

    // If the old ordinal is valid, the new ordinal should be different.
    DCHECK(!old_ordinal.IsValid() || !old_ordinal.Equals(new_ordinal));

    // The new ordinal should be valid.
    DCHECK(new_ordinal.IsValid());

    sync_item->item_ordinal = new_ordinal;
    ProcessExistingSyncItem(sync_item);
    UpdateSyncItemInLocalStorage(profile_, sync_item);
    SendSyncChange(FindSyncItem(reorder_param.sync_item_id),
                   SyncChange::ACTION_UPDATE);
  }

  sync_model_sanitizer_->SanitizePageBreaks(
      model_updater_->GetTopLevelItemIds(), /*reset_page_breaks=*/true);
}

syncer::StringOrdinal AppListSyncableService::CalculateGlobalFrontPosition()
    const {
  return reorder::CalculateFrontPosition(sync_items_);
}

bool AppListSyncableService::CalculateItemPositionInPermanentSortOrder(
    const ash::AppListItemMetadata& metadata,
    syncer::StringOrdinal* target_position) const {
  // TODO(https://crbug.com/1260877): ideally we would not have to create a
  // one-off vector of items using `GetItems()`.
  return reorder::CalculateItemPositionInOrder(
      GetPermanentSortingOrder(), metadata, model_updater_->GetItems(),
      &sync_items_, target_position);
}

ash::AppListSortOrder AppListSyncableService::GetPermanentSortingOrder() const {
  return static_cast<ash::AppListSortOrder>(
      profile_->GetPrefs()->GetInteger(prefs::kAppListPreferredOrder));
}

// AppListSyncableService private

void AppListSyncableService::ProcessSyncItemSpecifics(
    const sync_pb::AppListSpecifics& specifics) {
  const std::string& item_id = specifics.item_id();
  if (item_id.empty()) {
    LOG(ERROR) << "AppList item with empty ID";
    return;
  }
  SyncItem* sync_item = FindSyncItem(item_id);
  if (sync_item) {
    // If an item of the same type exists, update it.
    if (sync_item->item_type == specifics.item_type()) {
      UpdateSyncItemFromSync(specifics, sync_item);
      ProcessExistingSyncItem(sync_item);
      UpdateSyncItemInLocalStorage(profile_, sync_item);
      VLOG(2) << this << " <- SYNC UPDATE: " << sync_item->ToString();
      return;
    }
    // Otherwise, one of the entries should be TYPE_REMOVE_DEFAULT_APP.
    if (sync_item->item_type !=
            sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP &&
        specifics.item_type() !=
            sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
      LOG(ERROR) << "Synced item type: " << specifics.item_type()
                 << " != existing sync item type: " << sync_item->item_type
                 << " Deleting item from model!";
      model_updater_->RemoveItem(item_id, /*is_uninstall=*/false);
    }
    VLOG(2) << this << " - ProcessSyncItem: Delete existing entry: "
            << sync_item->ToString();
    sync_items_.erase(item_id);
  }

  sync_item = CreateSyncItem(item_id, specifics.item_type(), /*is_new=*/false);
  UpdateSyncItemFromSync(specifics, sync_item);
  ProcessNewSyncItem(sync_item);
  UpdateSyncItemInLocalStorage(profile_, sync_item);
  VLOG(2) << this << " <- SYNC ADD: " << sync_item->ToString();
}

void AppListSyncableService::ProcessNewSyncItem(SyncItem* sync_item) {
  VLOG(2) << "ProcessNewSyncItem: " << sync_item->ToString();
  switch (sync_item->item_type) {
    case sync_pb::AppListSpecifics::TYPE_APP: {
      // New apps are added through ExtensionAppModelBuilder.
      // TODO(stevenjb): Determine how to handle app items in sync that
      // are not installed (e.g. default / OEM apps).
      return;
    }
    case sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP: {
      VLOG(2) << this << ": Uninstall: " << sync_item->ToString();
      UninstallExtension(extension_system_->extension_service(),
                         extension_registry_, sync_item->item_id);
      return;
    }
    case sync_pb::AppListSpecifics::TYPE_FOLDER: {
      // We don't create new folders here, the model will do that.
      model_updater_->UpdateAppItemFromSyncItem(
          sync_item,
          sync_item->item_id !=
              ash::kOemFolderId,  // Don't sync oem folder's name.
          false);                 // It's a folder itself.
      return;
    }
    case sync_pb::AppListSpecifics::TYPE_OBSOLETE_URL:
    case sync_pb::AppListSpecifics::TYPE_PAGE_BREAK:
      return;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unrecognized sync item type: " << sync_item->ToString();
}

void AppListSyncableService::ProcessExistingSyncItem(SyncItem* sync_item) {
  if (sync_item->item_type ==
      sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    return;
  }
  VLOG(2) << "ProcessExistingSyncItem: " << sync_item->ToString();

  // The only place where sync can change an item's folder. Prevent moving OEM
  // item to the folder, other than OEM folder.
  const bool update_folder = !AppIsOem(sync_item->item_id);
  model_updater_->UpdateAppItemFromSyncItem(
      sync_item,
      sync_item->item_id != ash::kOemFolderId,  // Don't sync oem folder's name.
      update_folder);

  const auto linked_promise_item = base::ranges::find_if(
      items_linked_to_promise_item_, [&sync_item](const auto& linked_item) {
        return linked_item.second == sync_item->item_id;
      });
  if (linked_promise_item != items_linked_to_promise_item_.end()) {
    SyncItem* promise_item = FindSyncItem(linked_promise_item->first);
    if (promise_item) {
      CopyAttributesToSyncItem(sync_item, promise_item);

      model_updater_->UpdateAppItemFromSyncItem(
          promise_item,
          promise_item->item_id !=
              ash::kOemFolderId,  // Don't sync oem folder's name.
          update_folder);
    }
  }
}

bool AppListSyncableService::SyncStarted() {
  if (sync_processor_.get())
    return true;
  if (flare_.is_null()) {
    VLOG(2) << this << ": SyncStarted: Flare.";
    flare_ = sync_start_util::GetFlareForSyncableService(profile_->GetPath());
    flare_.Run(syncer::APP_LIST);
  }
  return false;
}

void AppListSyncableService::SendSyncChange(
    SyncItem* sync_item,
    SyncChange::SyncChangeType sync_change_type) {
  // Do not sync ephemeral sync items.
  if (sync_item->is_ephemeral)
    return;

  if (!SyncStarted()) {
    DVLOG(2) << this << " - SendSyncChange: SYNC NOT STARTED: "
             << sync_item->ToString();
    return;
  }
  if (!initial_sync_data_processed_ &&
      sync_change_type == SyncChange::ACTION_ADD) {
    // This can occur if an initial item is created before its folder item.
    // A sync item should already exist for the folder, so we do not want to
    // send an ADD event, since that would trigger a CHECK in the sync code.
    DCHECK(sync_item->item_type == sync_pb::AppListSpecifics::TYPE_FOLDER);
    DVLOG(2) << this << " - SendSyncChange: ADD before initial data processed: "
             << sync_item->ToString();
    return;
  }
  if (sync_change_type == SyncChange::ACTION_ADD)
    VLOG(2) << this << " -> SYNC ADD: " << sync_item->ToString();
  else
    VLOG(2) << this << " -> SYNC UPDATE: " << sync_item->ToString();
  SyncChange sync_change(FROM_HERE, sync_change_type,
                         GetSyncDataFromSyncItem(sync_item));
  sync_processor_->ProcessSyncChanges(FROM_HERE,
                                      syncer::SyncChangeList(1, sync_change));
}

AppListSyncableService::SyncItem* AppListSyncableService::FindSyncItem(
    const std::string& item_id) {
  return const_cast<SyncItem*>(GetSyncItem(item_id));
}

AppListSyncableService::SyncItem* AppListSyncableService::CreateSyncItem(
    const std::string& item_id,
    sync_pb::AppListSpecifics::AppListItemType item_type,
    bool is_new) {
  DCHECK(!base::Contains(sync_items_, item_id));
  sync_items_[item_id] = std::make_unique<SyncItem>(item_id, item_type, is_new);

  // In case we have pending attributes to apply, process it asynchronously.
  if (base::Contains(pending_transfer_map_, item_id)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AppListSyncableService::ApplyAppAttributes,
                                  weak_ptr_factory_.GetWeakPtr(), item_id,
                                  std::move(pending_transfer_map_[item_id])));
    pending_transfer_map_.erase(item_id);
  }

  return sync_items_[item_id].get();
}

void AppListSyncableService::DeleteSyncItemSpecifics(
    const sync_pb::AppListSpecifics& specifics) {
  const std::string& item_id = specifics.item_id();
  if (item_id.empty()) {
    LOG(ERROR) << "Delete AppList item with empty ID";
    return;
  }
  VLOG(2) << this << ": DeleteSyncItemSpecifics: " << item_id.substr(0, 8);
  auto iter = sync_items_.find(item_id);
  if (iter == sync_items_.end())
    return;

  // Check if we're asked to remove a default-installed app.
  auto* sync_item = iter->second.get();
  if (InterceptDeleteDefaultApp(sync_item)) {
    return;
  }

  sync_pb::AppListSpecifics::AppListItemType item_type = sync_item->item_type;
  VLOG(2) << this << " <- SYNC DELETE: " << sync_item->ToString();
  RemoveSyncItemFromLocalStorage(profile_, item_id);
  sync_items_.erase(iter);

  // Only delete apps and page break from the model. Folders will be deleted
  // when all children have been deleted.
  if (item_type == sync_pb::AppListSpecifics::TYPE_APP ||
      item_type == sync_pb::AppListSpecifics::TYPE_PAGE_BREAK) {
    model_updater_->RemoveItem(item_id, /*is_uninstall=*/false);
  }
}

bool AppListSyncableService::AppIsOem(const std::string& id) {
  // For Arc and web apps, it is sufficient to check the install reason.
  apps::InstallReason install_reason = apps::InstallReason::kUnknown;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(id, [&install_reason](const apps::AppUpdate& update) {
        install_reason = update.InstallReason();
      });
  if (install_reason == apps::InstallReason::kOem)
    return true;

  if (!extension_system_->extension_service())
    return false;
  const extensions::Extension* extension =
      extension_registry_->GetExtensionById(
          id, extensions::ExtensionRegistry::EVERYTHING);
  return extension && extension->was_installed_by_oem();
}

std::string AppListSyncableService::SyncItem::ToString() const {
  std::string res = item_id.substr(0, 8);
  if (item_type == sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    res += " { RemoveDefault }";
  } else if (item_type == sync_pb::AppListSpecifics::TYPE_PAGE_BREAK) {
    res += " { PageBreakItem }";
    res += " [" + item_ordinal.ToDebugString() + "]";
  } else {
    res += " { " + item_name + " }";
    if (!promise_package_id.empty()) {
      res += " { " + promise_package_id + " }";
    }
    res += " [" + item_ordinal.ToDebugString() + "]";
    if (!parent_id.empty()) {
      res += " <" + parent_id.substr(0, 8) + ">";
    }
    res += " [" + item_pin_ordinal.ToDebugString() + "(up=" +
           (is_user_pinned.has_value() ? (*is_user_pinned ? "true" : "false")
                                       : "?") +
           ")]";
  }

  if (item_color.IsValid()) {
    res += " (" +
           sync_pb::AppListSpecifics::ColorGroup_Name(
               item_color.background_color()) +
           " ," + base::NumberToString(item_color.hue()) + " )";
  } else {
    res += "(INVALID COLOR)";
  }

  return res;
}

std::vector<AppListSyncableService::SyncItem*>
AppListSyncableService::GetSortedTopLevelSyncItems() const {
  // Filter out items in folder.
  std::vector<SyncItem*> sync_items;
  for (const auto& [item_id, sync_item] : sync_items_) {
    if (IsTopLevelAppItem(*sync_item) && sync_item->item_ordinal.IsValid()) {
      sync_items.emplace_back(sync_item.get());
    }
  }

  // Sort remaining items based on their positions.
  base::ranges::sort(sync_items, syncer::StringOrdinal::LessThanFn(),
                     &SyncItem::item_ordinal);
  return sync_items;
}

void AppListSyncableService::PruneRedundantPageBreakItems() {
  auto top_level_sync_items = GetSortedTopLevelSyncItems();

  // If the first item is a "page break" item, delete it. If there are
  // contiguous "page break" items, delete duplicate.
  bool was_page_break = true;
  for (auto iter = top_level_sync_items.begin();
       iter != top_level_sync_items.end();) {
    if (!IsPageBreakItem(**iter)) {
      was_page_break = false;
      ++iter;
      continue;
    }
    auto current_iter = iter++;
    if (was_page_break) {
      DeleteSyncItem((*current_iter)->item_id);
      iter = top_level_sync_items.erase(current_iter);
    } else {
      was_page_break = true;
    }
  }

  // Remove the trailing "page break" item if it exists.
  if (!top_level_sync_items.empty() &&
      IsPageBreakItem(*top_level_sync_items.back())) {
    DeleteSyncItem(top_level_sync_items.back()->item_id);
  }

  // Remove all the "page break" items that are in folder. No such item should
  // exist in folder. It should be safe to remove them if it do occur.
  for (auto iter = sync_items_.begin(); iter != sync_items_.end();) {
    const auto* sync_item = (iter++)->second.get();
    if (IsTopLevelAppItem(*sync_item) || !IsPageBreakItem(*sync_item))
      continue;

    LOG(ERROR) << "Delete a page break item in folder: " << sync_item->item_id;
    DeleteSyncItem(sync_item->item_id);
  }
}

void AppListSyncableService::UpdateSyncItemFromSync(
    const sync_pb::AppListSpecifics& specifics,
    AppListSyncableService::SyncItem* item) {
  DCHECK_EQ(item->item_id, specifics.item_id());
  item->item_type = specifics.item_type();
  item->item_name = specifics.item_name();
  if (specifics.has_promise_package_id()) {
    item->promise_package_id = specifics.promise_package_id();
  }

  // Ignore update to put item into the OEM folder in case app is not OEM.
  // This can happen when app is installed on several devices where app is OEM
  // on one device and not on another devices.
  if (specifics.parent_id() != ash::kOemFolderId || AppIsOem(item->item_id))
    item->parent_id = specifics.parent_id();
  if (specifics.has_item_ordinal())
    item->item_ordinal = syncer::StringOrdinal(specifics.item_ordinal());
  if (specifics.has_item_pin_ordinal()) {
    item->item_pin_ordinal =
        syncer::StringOrdinal(specifics.item_pin_ordinal());
  }
  if (ash::features::IsRemoveStalePolicyPinnedAppsFromShelfEnabled()) {
    if (specifics.has_is_user_pinned()) {
      item->is_user_pinned = specifics.is_user_pinned();
      // Valid `item_pin_ordinal` without set `is_user_pinned` in the proto
      // means an update from an older version -- do not overwrite the saved
      // value. Note that this is a best-effort heuristic which is not
      // consistent with the behavior in MergeDataAndStartSyncing.
    } else if (!item->item_pin_ordinal.IsValid()) {
      item->is_user_pinned = std::nullopt;
    }
  } else {
    // Nullify pin info if the feature is not supported.
    item->is_user_pinned = std::nullopt;
  }
  // `is_user_pinned` cannot be set while `item_pin_ordinal` is invalid.
  DCHECK(
      !(!item->item_pin_ordinal.IsValid() && item->is_user_pinned.has_value()));

  if (specifics.has_item_color()) {
    const sync_pb::AppListSpecifics_IconColor& specifics_icon_color =
        specifics.item_color();
    const bool has_data = (specifics_icon_color.has_background_color() &&
                           specifics_icon_color.has_hue());

    if (has_data) {
      ash::IconColor new_item_color(specifics_icon_color.background_color(),
                                    specifics_icon_color.hue());
      if (new_item_color.IsValid() &&
          (!item->item_color.IsValid() || item->item_color != new_item_color))
        item->item_color = new_item_color;
    }
  }
}

bool AppListSyncableService::UpdateSyncItemFromAppItem(
    const ChromeAppListItem* app_item,
    AppListSyncableService::SyncItem* sync_item) {
  DCHECK_EQ(sync_item->item_id, app_item->id());

  bool changed = false;
  // Allow sync changes for parent only for non OEM app.
  if (sync_item->parent_id != app_item->folder_id() &&
      !AppIsOem(app_item->id())) {
    sync_item->parent_id = app_item->folder_id();
    changed = true;
  }
  if (sync_item->item_name != app_item->name()) {
    sync_item->item_name = app_item->name();
    changed = true;
  }
  if (sync_item->promise_package_id != app_item->promise_package_id()) {
    sync_item->promise_package_id = app_item->promise_package_id();
    changed = true;
  }
  if (!sync_item->item_ordinal.IsValid() ||
      !app_item->position().Equals(sync_item->item_ordinal)) {
    sync_item->item_ordinal = app_item->position();
    changed = true;
  }

  if (SetIconColorIfChanged(app_item->icon_color(), &sync_item->item_color)) {
    changed = true;
  }

  if (sync_item->is_system_folder != app_item->is_system_folder()) {
    DCHECK(!sync_item->is_system_folder);
    sync_item->is_system_folder = app_item->is_system_folder();
    // Do not mark the item as changed - the persistent value is not expected to
    // be persisted to local state, nor synced. Also, it's expected to be set as
    // part of folder item creation flow, so no further processing should be
    // necessary.
  }

  if (sync_item->is_ephemeral != app_item->is_ephemeral()) {
    DCHECK(!sync_item->is_ephemeral);
    sync_item->is_ephemeral = app_item->is_ephemeral();
    // Do not mark the item as changed - the ephemeral value is not expected to
    // be persisted to local state, nor synced. Ephemeral apps and folders are
    // not synced. The ChromeAppListItem will always have the is_ephemeral flag
    // set first.
  }

  return changed;
}

bool AppListSyncableService::GetAppPreloadServiceInfo(
    const ChromeAppListItem* new_item,
    syncer::StringOrdinal* position,
    std::string* folder_id,
    std::string* folder_name,
    syncer::StringOrdinal* folder_position) const {
  std::optional<apps::PackageId> package_id;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(new_item->id(), [&package_id](const apps::AppUpdate& update) {
        package_id = update.InstallerPackageId();
      });
  if (!package_id) {
    return false;
  }

  auto ordinal_it = preload_service_ordinals_.find(*package_id);
  if (ordinal_it == preload_service_ordinals_.end()) {
    return false;
  }
  *position = ordinal_it->second;

  constexpr auto oem_type =
      apps::proto::AppPreloadListResponse_LauncherType_LAUNCHER_TYPE_FOLDER_OEM;
  for (auto const& [folder, item_map] : preload_service_order_) {
    // Find the folder that includes `new_item`.
    if (folder.empty() || !item_map.contains(*package_id)) {
      continue;
    }
    // Look up folder details in root folder.
    auto root_it = preload_service_order_.find(std::string());
    if (root_it == preload_service_order_.end()) {
      continue;
    }
    const auto& root_folder_items = root_it->second;
    auto it = root_folder_items.find(folder);
    if (it == root_folder_items.end()) {
      continue;
    }
    // Get ordinal of folder inside root.
    ordinal_it = preload_service_ordinals_.find(folder);
    if (ordinal_it == preload_service_ordinals_.end()) {
      continue;
    }
    *folder_id =
        it->second.type == oem_type ? ash::kOemFolderId : "folder:" + folder;
    *folder_name = folder;
    *folder_position = ordinal_it->second;
    break;
  }
  return true;
}

void AppListSyncableService::SetOemFolderNameFromAppPreloadService(
    const apps::LauncherOrdering& launcher_ordering) {
  auto root_folder = launcher_ordering.find(std::string());
  if (root_folder == launcher_ordering.end()) {
    return;
  }
  constexpr auto oem_type =
      apps::proto::AppPreloadListResponse_LauncherType_LAUNCHER_TYPE_FOLDER_OEM;
  for (auto const& [item, data] : root_folder->second) {
    if (data.type == oem_type && absl::holds_alternative<std::string>(item)) {
      oem_folder_name_ = absl::get<std::string>(item);
      return;
    }
  }
}

void AppListSyncableService::InitNewItemPosition(ChromeAppListItem* new_item) {
  DCHECK(!model_updater_->FindItem(new_item->id()));
  DCHECK(!new_item->position().IsValid());

  // TODO(https://crbug.com/1260875): handle the case that `new_item` is a
  // folder.
  // Calculating the crostini folder's position with the sort order serves as a
  // quick fix for https://crbug.com/1353237. Right now, folders except for the
  // crostini folder still use the first available position as the initial
  // position due to the concern over the possible regression in OEM folders.
  bool use_first_available_position =
      new_item->is_folder() && new_item->id() != ash::kCrostiniFolderId;
  if (use_first_available_position) {
    new_item->SetChromePosition(model_updater_->GetFirstAvailablePosition());
    return;
  }

  // The code below initializes the app's position when the app list sort
  // feature is enabled.

  // The target position of `new_item`.
  syncer::StringOrdinal position;

  bool is_successful = CalculateItemPositionInPermanentSortOrder(
      new_item->metadata(), &position);

  // If `new_item` cannot be placed following the specified order, `new_item`
  // should be placed at front. Also reset the sorting order.
  if (!is_successful) {
    DCHECK(!position.IsValid());
    position = CalculateGlobalFrontPosition();
    SetAppListPreferredOrder(ash::AppListSortOrder::kCustom);
  }

  DCHECK(position.IsValid());
  new_item->SetChromePosition(position);
}

void AppListSyncableService::EnsureFolderExists(
    const std::string& folder_id,
    const std::string& folder_name,
    syncer::StringOrdinal folder_position) {
  if (model_updater_->FindItem(folder_id)) {
    return;
  }

  auto folder = std::make_unique<ChromeAppListItem>(profile_, folder_id,
                                                    model_updater_.get());
  folder->SetChromeName(folder_name);
  folder->SetIsSystemFolder(true);
  folder->SetChromeIsFolder(true);

  SyncItem* current_sync_data = FindSyncItem(folder_id);
  if (current_sync_data) {
    folder_position = current_sync_data->item_ordinal;
  } else if (folder_id == ash::kOemFolderId && !folder_position.IsValid()) {
    oem_folder_using_provisional_default_position_ =
        !initial_sync_data_processed_;
    folder_position = GetDefaultOemFolderPosition();
  }

  if (!folder_position.IsValid()) {
    folder_position = GetLastPosition();
  }
  folder->SetChromePosition(folder_position);

  if (current_sync_data) {
    UpdateSyncItem(folder.get());
  } else {
    CreateSyncItemFromAppItem(folder.get());
  }

  model_updater_->AddItem(std::move(folder));
}

void AppListSyncableService::MaybeAddOrUpdateGuestOsFolderSyncData(
    const std::string& folder_id) {
  if (model_updater_->FindItem(folder_id)) {
    // The folder exists. Therefore its sync data is up-to-date.
    return;
  }

  std::string folder_name;
  if (folder_id == ash::kCrostiniFolderId) {
    folder_name =
        l10n_util::GetStringUTF8(IDS_APP_LIST_CROSTINI_DEFAULT_FOLDER_NAME);
  } else if (folder_id == ash::kBruschettaFolderId) {
    folder_name =
        l10n_util::GetStringFUTF8(IDS_APP_LIST_BRUSCHETTA_DEFAULT_FOLDER_NAME,
                                  bruschetta::GetOverallVmName(profile_));
  } else {
    return;
  }
  ChromeAppListItem folder(profile_, folder_id, model_updater_.get());
  folder.SetChromeName(folder_name);
  folder.SetIsSystemFolder(true);
  folder.SetChromeIsFolder(true);

  // Calculate the Crostini folder's position.
  const SyncItem* current_sync_data = GetSyncItem(folder_id);
  if (current_sync_data) {
    const syncer::StringOrdinal& item_position =
        current_sync_data->item_ordinal;
    DCHECK(item_position.IsValid());
    folder.SetChromePosition(item_position);
  } else {
    InitNewItemPosition(&folder);
  }

  // Add or update the folder's sync data.
  // Note that we cannot call `AddOrUpdateFromSyncItem()` here because
  // the folder is not added to `model_updater_` yet.
  if (current_sync_data) {
    UpdateSyncItem(&folder);
  } else {
    CreateSyncItemFromAppItem(&folder);
  }
}

bool AppListSyncableService::MaybeCreateFolderBeforeAddingItem(
    ChromeAppListItem* app_item,
    const std::string& folder_id) {
  DCHECK(!folder_id.empty());

  const SyncItem* folder_sync_item = FindSyncItem(folder_id);
  if (!folder_sync_item) {
    app_item->SetChromeFolderId("");
    return false;
  }

  ChromeAppListItem* folder_item = model_updater_->FindItem(folder_id);
  DCHECK(!folder_item || folder_item->is_folder());

  // The folder item specified by `folder_id` already exists. Nothing to do.
  if (folder_item)
    return true;

  auto new_folder_item = std::make_unique<ChromeAppListItem>(
      profile_, folder_id, model_updater_.get());
  new_folder_item->SetMetadata(
      app_list::GenerateItemMetadataFromSyncItem(*folder_sync_item));
  if (IsSystemCreatedSyncFolder(*folder_sync_item))
    new_folder_item->SetIsSystemFolder(true);
  model_updater_->AddItem(std::move(new_folder_item));
  return true;
}

bool AppListSyncableService::IsAppDefaultPositionedForNewUsersOnly(
    const std::string& app_id) const {
  if (app_default_positioned_for_new_users_only_ == app_id) {
    return true;
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (chromeos::features::IsContainerAppPreinstallEnabled() &&
      app_id == web_app::kContainerAppId) {
    return true;
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
}

void AppListSyncableService::OnGetLauncherOrdering(
    const apps::LauncherOrdering& launcher_ordering) {
  preload_service_order_ = launcher_ordering;
  SetOemFolderNameFromAppPreloadService(launcher_ordering);

  // Set ordinals for all packages and folders.
  for (auto const& [folder, item_map] : preload_service_order_) {
    // Sort ordering for items in the same folder.
    std::vector<std::pair<apps::LauncherItem, apps::LauncherItemData>>
        folder_order(item_map.begin(), item_map.end());
    base::ranges::sort(
        folder_order, {},
        [](const std::pair<apps::LauncherItem, apps::LauncherItemData>& p) {
          return p.second.order;
        });
    // Non-root folders are simple since there is no merging.
    if (!folder.empty()) {
      auto ordinal = syncer::StringOrdinal::CreateInitialOrdinal();
      for (const auto& [item, data] : folder_order) {
        preload_service_ordinals_[item] = ordinal;
        ordinal = ordinal.CreateAfter();
      }
      continue;
    }

    // Root folder has defaults that we must merge into. Items in `folder_order`
    // that already exist in `defaults` will keep their ordinal. Other items
    // will be inserted at `merge_index` which moves only forwards and updates
    // each time we match an existing item.
    base::span<const apps::LauncherItem> defaults =
        chromeos::default_app_order::GetAppPreloadServiceDefaults();
    size_t merge_index = 0;
    // Items from `folder_order` get inserted between `lhs` and `rhs`. `lhs`
    // starts as invalid, then takes the value of each item as it is assigned,
    // or the value of `merge_index` if it is updated. `rhs` starts as the first
    // item in `defaults` and points to values along `defaults` as we match
    // items and becomes invalid once we match the last item.
    syncer::StringOrdinal lhs;
    // `preload_service_ordinals_` already contains all items from `defaults`.
    syncer::StringOrdinal rhs = preload_service_ordinals_[defaults.front()];
    for (const auto& [item, data] : folder_order) {
      if (!preload_service_ordinals_.contains(item)) {
        // If item is not in defaults, then insert it between lhs and rhs.
        syncer::StringOrdinal ordinal = CreateBetween(lhs, rhs);
        preload_service_ordinals_[item] = ordinal;
        lhs = ordinal;
      } else {
        // Update `merge_index` and `lhs` if new match is after current.
        auto defaults_it = base::ranges::find(defaults.begin() + merge_index,
                                              defaults.end(), item);
        if (defaults_it != defaults.end()) {
          merge_index = defaults_it - defaults.begin();
          lhs = preload_service_ordinals_[item];
          if ((merge_index + 1) < defaults.size()) {
            rhs = preload_service_ordinals_[defaults[merge_index + 1]];
          } else {
            rhs = syncer::StringOrdinal();
          }
        }
      }
    }
  }
}

}  // namespace app_list
