// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/sparky/sparky_delegate_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/sparky/keyboard_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/sparky/sparky_util.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/manta/sparky/system_info_delegate.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

using SetPrefResult = extensions::settings_private::SetPrefResult;
using SettingsPrivatePrefType = extensions::api::settings_private::PrefType;

// Returns a displayable time for the last modified date.
std::u16string GetFormattedTime(base::Time time) {
  std::u16string date_time_of_day = base::TimeFormatTimeOfDay(time);
  std::u16string relative_date = ui::TimeFormat::RelativeDate(time, nullptr);
  std::u16string formatted_time;
  if (!relative_date.empty()) {
    relative_date = base::ToLowerASCII(relative_date);
    formatted_time = relative_date + u" " + date_time_of_day;
  } else {
    formatted_time = base::TimeFormatShortDate(time) + u", " + date_time_of_day;
  }

  return formatted_time;
}

// Returns a vector of Files within the root file path.
std::vector<manta::FileData> SearchFiles(
    const base::FilePath& my_files_path,
    const std::vector<base::FilePath> trash_paths,
    bool obtain_bytes,
    std::set<std::string> allowed_file_paths) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // Enumerate through  all of the files in the My Files folder.
  std::vector<manta::FileData> files_data;
  base::FileEnumerator file_enumerator(my_files_path,
                                       /*recursive=*/true,
                                       base::FileEnumerator::FileType::FILES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    // Exclude any paths that are parented at an enabled trash location.
    if (base::ranges::any_of(trash_paths,
                             [&file_path](const base::FilePath& trash_path) {
                               return trash_path.IsParent(file_path);
                             })) {
      continue;
    }

    // Get the file's name.
    std::string file_name = file_path.BaseName().AsUTF8Unsafe();

    // If a set of allowed file paths is defined, then only include files within
    // this list.
    if (!allowed_file_paths.empty() &&
        !allowed_file_paths.contains(file_path.AsUTF8Unsafe())) {
      continue;
    }

    // Open the file.
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);

    // Get the file's information.
    base::Time file_date_modified =
        base::Time::FromTimeT(file_enumerator.GetInfo().stat().st_atime);
    std::string file_date_modified_string =
        base::UTF16ToUTF8(GetFormattedTime(file_date_modified));

    auto file_data = manta::FileData(file_path.AsUTF8Unsafe(), file_name,
                                     file_date_modified_string);

    // Obtain the bytes of the file if requested.
    if (obtain_bytes) {
      file_data.bytes = base::ReadFileToBytes(file_path);
    }

    file_data.size_in_bytes = file_enumerator.GetInfo().GetSize();

    // Create a `FilesData` object.
    files_data.emplace_back(std::move(file_data));
  }
  return files_data;
}

// Write a file to disk. This returns the file path as a string so it can be
// easily chained with LaunchFile.
std::string WriteFileBlocking(const base::FilePath& file_path,
                              std::string_view bytes) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ImportantFileWriter::WriteFileAtomically(file_path, bytes, "Sparky");
  return file_path.value();
}

}  // namespace

SparkyDelegateImpl::SparkyDelegateImpl(Profile* profile)
    : profile_(profile),
      prefs_util_(std::make_unique<extensions::PrefsUtil>(profile)),
      screenshot_handler_(std::make_unique<sparky::ScreenshotHandler>()),
      total_disk_space_calculator_(profile),
      free_disk_space_calculator_(profile),
      root_path_(file_manager::util::GetMyFilesFolderForProfile(profile_)) {
  StartObservingCalculators();
}

SparkyDelegateImpl::~SparkyDelegateImpl() {
  StopObservingCalculators();
}

bool SparkyDelegateImpl::SetSettings(
    std::unique_ptr<manta::SettingsData> settings_data) {
  if (!settings_data->val_set) {
    return false;
  }
  if (settings_data->pref_name == prefs::kDarkModeEnabled) {
    profile_->GetPrefs()->SetBoolean(settings_data->pref_name,
                                     settings_data->bool_val);
    return true;
  }

  SetPrefResult result = prefs_util_->SetPref(
      settings_data->pref_name, base::to_address(settings_data->GetValue()));

  return result == SetPrefResult::SUCCESS;
}

void SparkyDelegateImpl::AddPrefToMap(
    const std::string& pref_name,
    SettingsPrivatePrefType settings_pref_type,
    std::optional<base::Value> value) {
  // TODO (b:354608065) Add in UMA logging for these error cases.
  switch (settings_pref_type) {
    case SettingsPrivatePrefType::kBoolean: {
      if (!value->is_bool()) {
        DVLOG(1) << "Cros setting " << pref_name
                 << " has a prefType of bool, but has a value of type: "
                 << value->type();
        break;
      }
      current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
          pref_name, manta::PrefType::kBoolean, std::move(value));
      break;
    }
    case SettingsPrivatePrefType::kNumber: {
      if (value->is_int()) {
        current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
            pref_name, manta::PrefType::kInt, std::move(value));
      } else if (value->is_double()) {
        current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
            pref_name, manta::PrefType::kDouble, std::move(value));
      } else {
        DVLOG(1) << "Cros setting " << pref_name
                 << " has a prefType of number, but has a value of type: "
                 << value->type();
      }
      break;
    }
    case SettingsPrivatePrefType::kList: {
      if (!value->is_list()) {
        DVLOG(1) << "Cros setting " << pref_name
                 << " has a prefType of list, but has a value of type: "
                 << value->type();
        break;
      }
      current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
          pref_name, manta::PrefType::kList, std::move(value));
      break;
    }
    case SettingsPrivatePrefType::kString:
    case SettingsPrivatePrefType::kUrl: {
      if (!value->is_string()) {
        DVLOG(1)
            << "Cros setting " << pref_name
            << " has a prefType of string or url, but has a value of type: "
            << value->type();
        break;
      }
      current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
          pref_name, manta::PrefType::kString, std::move(value));
      break;
    }
    case SettingsPrivatePrefType::kDictionary: {
      if (!value->is_dict()) {
        DVLOG(1) << "Cros setting " << pref_name
                 << " has a prefType of dictionary, but has a value of type: "
                 << value->type();
        break;
      }
      current_prefs_[pref_name] = std::make_unique<manta::SettingsData>(
          pref_name, manta::PrefType::kDictionary, std::move(value));
      break;
    }
    default:
      break;
  }
}

SparkyDelegateImpl::SettingsDataList* SparkyDelegateImpl::GetSettingsList() {
  extensions::PrefsUtil::TypedPrefMap pref_list =
      prefs_util_->GetAllowlistedKeys();

  current_prefs_ = SparkyDelegateImpl::SettingsDataList();

  for (auto const& [pref_name, pref_type] : pref_list) {
    auto pref_object = prefs_util_->GetPref(pref_name);
    if (pref_object.has_value()) {
      AddPrefToMap(pref_name, pref_type, std::move(pref_object->value));
    }
  }

  current_prefs_[prefs::kDarkModeEnabled] =
      std::make_unique<manta::SettingsData>(
          prefs::kDarkModeEnabled, manta::PrefType::kBoolean,
          std::make_optional<base::Value>(
              profile_->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled)));
  return &current_prefs_;
}

std::optional<base::Value> SparkyDelegateImpl::GetSettingValue(
    const std::string& setting_id) {
  if (setting_id == prefs::kDarkModeEnabled) {
    return std::make_optional<base::Value>(
        profile_->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled));
  }
  auto pref_object = prefs_util_->GetPref(setting_id);
  if (pref_object.has_value()) {
    return std::move(pref_object->value);
  } else {
    return std::nullopt;
  }
}

void SparkyDelegateImpl::GetScreenshot(manta::ScreenshotDataCallback callback) {
  screenshot_handler_->TakeScreenshot(std::move(callback));
}

std::vector<manta::AppsData> SparkyDelegateImpl::GetAppsList() {
  std::vector<manta::AppsData> apps;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForEachApp([&apps](const apps::AppUpdate& update) {
        if (!apps_util::IsInstalled(update.Readiness())) {
          return;
        }

        if (!update.ShowInSearch().value_or(false) &&
            !(update.Recommendable().value_or(false) &&
              update.AppType() == apps::AppType::kBuiltIn)) {
          return;
        }

        manta::AppsData& app = apps.emplace_back(update.Name(), update.AppId());

        for (const std::string& term : update.AdditionalSearchTerms()) {
          app.AddSearchableText(term);
        }
      });
  return apps;
}

void SparkyDelegateImpl::LaunchApp(const std::string& app_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->Launch(app_id, ui::EF_IS_SYNTHESIZED, apps::LaunchSource::kFromSparky,
                std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));
}

void SparkyDelegateImpl::ObtainStorageInfo(
    manta::StorageDataCallback storage_callback) {
  storage_callback_ = std::move(storage_callback);
  total_disk_space_calculator_.StartCalculation();
  free_disk_space_calculator_.StartCalculation();
}

void SparkyDelegateImpl::Click(int x, int y) {
  // Get the Window of the primary display.
  const auto& display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto* host = ash::GetWindowTreeHostForDisplay(display.id());
  CHECK(host);
  aura::Window* window = host->window();
  CHECK(window);

  // Create a point in window coordinates, which can be different from screen
  // coordinates if multiple screens are present.
  gfx::Point point(x, y);
  ::wm::ConvertPointFromScreen(window, &point);

  // Create a pair of pressed/released mouse events. These need to be scaled to
  // the screen to account for non-1x scale factors.
  ui::MouseEvent mouse_pressed(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_IS_SYNTHESIZED,
                               ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent mouse_released(ui::EventType::kMouseReleased, point, point,
                                ui::EventTimeForNow(), ui::EF_IS_SYNTHESIZED,
                                ui::EF_LEFT_MOUSE_BUTTON);

  mouse_pressed.UpdateForRootTransform(
      host->GetRootTransform(),
      host->GetRootTransformForLocalEventCoordinates());
  mouse_released.UpdateForRootTransform(
      host->GetRootTransform(),
      host->GetRootTransformForLocalEventCoordinates());

  // Other parts of the system can temporarily disable mouse events. If this is
  // the case, re-enable them for the duration of our calls.
  auto* cursor = aura::client::GetCursorClient(window);
  const bool mouse_disabled = !cursor->IsMouseEventsEnabled();
  if (mouse_disabled) {
    cursor->EnableMouseEvents();
  }

  // No delay is needed between these events.
  //
  // DeliverEventToSink skips event rewriters, unlike SendEventToSink.
  // TODO(b/351099209): understand if this is desirable.
  host->DeliverEventToSink(&mouse_pressed);
  host->DeliverEventToSink(&mouse_released);

  if (mouse_disabled) {
    cursor->DisableMouseEvents();
  }
}

void SparkyDelegateImpl::KeyboardEntry(std::string text) {
  // Get the window tree host for the primary display.
  const auto& display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto* host = ash::GetWindowTreeHostForDisplay(display.id());
  CHECK(host);

  auto key_events = KeyEventsForText(text);
  if (!key_events) {
    // TODO(b/351099209): report an error, `text` contains non-typeable
    // characters.
    return;
  }

  for (auto& key_event : key_events.value()) {
    host->DeliverEventToSink(&key_event);
  }
}

void SparkyDelegateImpl::KeyPress(const std::string& key,
                                  bool control,
                                  bool alt,
                                  bool shift) {
  // Get the window tree host for the primary display.
  const auto& display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto* host = ash::GetWindowTreeHostForDisplay(display.id());
  CHECK(host);

  const auto key_code = KeyboardCodeForDOMString(key);

  if (key_code) {
    auto pressed_released =
        MakeKeyEventPair(key_code.value(), control, alt, shift);
    host->DeliverEventToSink(&pressed_released.first);
    host->DeliverEventToSink(&pressed_released.second);
  } else {
    // TODO(b/351099209): Report an error.
  }
}

void SparkyDelegateImpl::StartObservingCalculators() {
  total_disk_space_calculator_.AddObserver(this);
  free_disk_space_calculator_.AddObserver(this);
}

void SparkyDelegateImpl::StopObservingCalculators() {
  total_disk_space_calculator_.RemoveObserver(this);
  free_disk_space_calculator_.RemoveObserver(this);
}

void SparkyDelegateImpl::OnSizeCalculated(
    const SimpleSizeCalculator::CalculationType& calculation_type,
    int64_t total_bytes) {
  // The total disk space is rounded to the next power of 2.
  if (calculation_type == SimpleSizeCalculator::CalculationType::kTotal) {
    total_bytes = sparky::RoundByteSize(total_bytes);
  }

  // Store calculated item's size.
  const int item_index = static_cast<int>(calculation_type);
  storage_items_total_bytes_[item_index] = total_bytes;

  // Mark item as calculated.
  calculation_state_.set(item_index);
  OnStorageInfoUpdated();
}

void SparkyDelegateImpl::OnStorageInfoUpdated() {
  // If some size calculations are pending, return early and wait for all
  // calculations to complete.
  if (!calculation_state_.all()) {
    return;
  }

  const int total_space_index =
      static_cast<int>(SimpleSizeCalculator::CalculationType::kTotal);
  const int free_disk_space_index =
      static_cast<int>(SimpleSizeCalculator::CalculationType::kAvailable);

  int64_t total_bytes = storage_items_total_bytes_[total_space_index];
  int64_t available_bytes = storage_items_total_bytes_[free_disk_space_index];

  if (total_bytes <= 0 || available_bytes < 0) {
    // We can't get useful information from the storage page if total_bytes <=
    // 0 or available_bytes is less than 0. This is not expected to happen.
    NOTREACHED_IN_MIGRATION()
        << "Unable to retrieve total or available disk space";
    return;
  }
  std::move(storage_callback_)
      .Run(std::make_unique<manta::StorageData>(
          base::UTF16ToUTF8(ui::FormatBytes(available_bytes)),
          base::UTF16ToUTF8(ui::FormatBytes(total_bytes))));
}

void SparkyDelegateImpl::LaunchFile(const std::string& file_path) {
  file_manager::util::OpenItem(profile_, base::FilePath(file_path),
                               platform_util::OpenItemType::OPEN_FILE,
                               base::DoNothing());
}

void SparkyDelegateImpl::WriteFile(const std::string& name,
                                   const std::string& bytes) {
  const auto downloads =
      file_manager::util::GetDownloadsFolderForProfile(profile_);
  const auto file_path = downloads.Append(name);

  // Write a file and then immediately open it, so that it's clear that action
  // was taken.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(WriteFileBlocking, file_path, bytes),
      base::BindOnce(&SparkyDelegateImpl::LaunchFile, base::Unretained(this)));
}

void SparkyDelegateImpl::GetMyFiles(manta::FilesDataCallback callback,
                                    bool obtain_bytes,
                                    std::set<std::string> allowed_file_paths) {
  if (trash_paths_.empty()) {
    if (!file_manager::trash::IsTrashEnabledForProfile(profile_)) {
      trash_paths_ = std::vector<base::FilePath>();
    } else {
      auto enabled_trash_locations =
          file_manager::trash::GenerateEnabledTrashLocationsForProfile(
              profile_, /*base_path=*/base::FilePath());
      for (const auto& it : enabled_trash_locations) {
        trash_paths_.emplace_back(
            it.first.Append(it.second.relative_folder_path));
      }
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(SearchFiles, root_path_, trash_paths_, obtain_bytes,
                     allowed_file_paths),
      std::move(callback));
}

void SparkyDelegateImpl::UpdateFileSummaries(
    const std::vector<manta::FileData>& files_with_summary) {
  // Adds all new entries. Overrides any current entries.
  for (const manta::FileData& file : files_with_summary) {
    // All files added to the index must include a file name, path and summary.
    if (file.path.empty() || file.summary.empty()) {
      continue;
    }
    file_summaries_.insert_or_assign(file.path, file);
  }
}

std::vector<manta::FileData> SparkyDelegateImpl::GetFileSummaries() {
  std::vector<manta::FileData> files_data;
  for (const auto& [path, file] : file_summaries_) {
    files_data.emplace_back(file);
  }
  return files_data;
}

}  // namespace ash
