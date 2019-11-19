// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/l10n/l10n_util.h"

using vm_tools::apps::App;

namespace crostini {

namespace {

// Prefix of the ApplicationId set on exo windows for X apps.
constexpr char kCrostiniWindowAppIdPrefix[] = "org.chromium.termina.";
// This comes after kCrostiniWindowAppIdPrefix
constexpr char kWMClassPrefix[] = "wmclass.";

constexpr char kCrostiniIconFolder[] = "crostini.icons";

// Keys for the Dictionary stored in prefs for each app.
constexpr char kAppDesktopFileIdKey[] = "desktop_file_id";
constexpr char kAppVmNameKey[] = "vm_name";
constexpr char kAppContainerNameKey[] = "container_name";
constexpr char kAppCommentKey[] = "comment";
constexpr char kAppMimeTypesKey[] = "mime_types";
constexpr char kAppKeywordsKey[] = "keywords";
constexpr char kAppExecutableFileNameKey[] = "executable_file_name";
constexpr char kAppNameKey[] = "name";
constexpr char kAppNoDisplayKey[] = "no_display";
constexpr char kAppScaledKey[] = "scaled";
constexpr char kAppPackageIdKey[] = "package_id";
constexpr char kAppStartupWMClassKey[] = "startup_wm_class";
constexpr char kAppStartupNotifyKey[] = "startup_notify";
constexpr char kAppInstallTimeKey[] = "install_time";
constexpr char kAppLastLaunchTimeKey[] = "last_launch_time";

constexpr char kCrostiniAppsInstalledHistogram[] =
    "Crostini.AppsInstalledAtLogin";

const std::string* GetAppNameForWMClass(base::StringPiece wmclass) {
  // A hard-coded mapping from WMClass to app names.
  // This is used to deal with the Linux apps that don't specify the correct
  // WMClass in their desktop files so that their aura windows can be identified
  // with their respective app IDs.
  static const base::NoDestructor<std::map<std::string, std::string>>
      kWMClassToNname({{"Octave-gui", "GNU Octave"},
                       {"MuseScore2", "MuseScore 2"},
                       {"XnViewMP", "XnView Multi Platform"}});
  const auto it = kWMClassToNname->find(wmclass.as_string());
  if (it == kWMClassToNname->end())
    return nullptr;
  return &it->second;
}

std::string GenerateAppId(const std::string& desktop_file_id,
                          const std::string& vm_name,
                          const std::string& container_name) {
  // These can collide in theory because the user could choose VM and container
  // names which contain slashes, but this will only result in apps missing from
  // the launcher.
  return crx_file::id_util::GenerateId(kCrostiniAppIdPrefix + vm_name + "/" +
                                       container_name + "/" + desktop_file_id);
}

base::Value ProtoToDictionary(const App::LocaleString& locale_string) {
  base::Value result(base::Value::Type::DICTIONARY);
  for (const App::LocaleString::Entry& entry : locale_string.values()) {
    const std::string& locale = entry.locale();

    std::string locale_with_dashes(locale);
    std::replace(locale_with_dashes.begin(), locale_with_dashes.end(), '_',
                 '-');
    if (!locale.empty() && !l10n_util::IsValidLocaleSyntax(locale_with_dashes))
      continue;

    result.SetKey(locale, base::Value(entry.value()));
  }
  return result;
}

std::set<std::string> ListToStringSet(const base::Value* list) {
  std::set<std::string> result;
  if (!list)
    return result;
  for (const base::Value& value : list->GetList())
    result.insert(value.GetString());
  return result;
}

base::Value ProtoToList(
    const google::protobuf::RepeatedPtrField<std::string>& strings) {
  base::Value result(base::Value::Type::LIST);
  for (const std::string& string : strings)
    result.Append(string);
  return result;
}

base::Value LocaleStringsProtoToDictionary(
    const App::LocaleStrings& repeated_locale_string) {
  base::Value result(base::Value::Type::DICTIONARY);
  for (const auto& strings_with_locale : repeated_locale_string.values()) {
    const std::string& locale = strings_with_locale.locale();

    std::string locale_with_dashes(locale);
    std::replace(locale_with_dashes.begin(), locale_with_dashes.end(), '_',
                 '-');
    if (!locale.empty() && !l10n_util::IsValidLocaleSyntax(locale_with_dashes))
      continue;
    result.SetKey(locale, ProtoToList(strings_with_locale.value()));
  }
  return result;
}

// Construct a registration based on the given App proto.
// |name| should be |app.name()| in Dictionary form.
base::Value AppPrefRegistrationFromApp(
    const vm_tools::apps::App& app,
    base::Value name,
    const vm_tools::apps::ApplicationList& app_list) {
  base::Value pref_registration(base::Value::Type::DICTIONARY);
  pref_registration.SetKey(kAppDesktopFileIdKey,
                           base::Value(app.desktop_file_id()));
  pref_registration.SetKey(kAppVmNameKey, base::Value(app_list.vm_name()));
  pref_registration.SetKey(kAppContainerNameKey,
                           base::Value(app_list.container_name()));
  pref_registration.SetKey(kAppNameKey, std::move(name));
  pref_registration.SetKey(kAppCommentKey, ProtoToDictionary(app.comment()));
  pref_registration.SetKey(kAppExecutableFileNameKey,
                           base::Value(app.executable_file_name()));
  pref_registration.SetKey(kAppMimeTypesKey, ProtoToList(app.mime_types()));
  pref_registration.SetKey(kAppKeywordsKey,
                           LocaleStringsProtoToDictionary(app.keywords()));
  pref_registration.SetKey(kAppNoDisplayKey, base::Value(app.no_display()));
  pref_registration.SetKey(kAppStartupWMClassKey,
                           base::Value(app.startup_wm_class()));
  pref_registration.SetKey(kAppStartupNotifyKey,
                           base::Value(app.startup_notify()));
  pref_registration.SetKey(kAppPackageIdKey, base::Value(app.package_id()));

  return pref_registration;
}

// This is the companion to CrostiniRegistryService::SetCurrentTime().
base::Time GetTime(const base::Value& pref, const char* key) {
  if (!pref.is_dict())
    return base::Time();

  const base::Value* value = pref.FindKeyOfType(key, base::Value::Type::STRING);
  int64_t time;
  if (!value || !base::StringToInt64(value->GetString(), &time))
    return base::Time();
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(time));
}

bool MatchingString(const std::string& search_string,
                    const std::string& value_string,
                    bool ignore_space) {
  std::string search = search_string;
  std::string value = value_string;
  if (ignore_space) {
    base::RemoveChars(search, " ", &search);
    base::RemoveChars(value, " ", &value);
  }
  return base::EqualsCaseInsensitiveASCII(search, value);
}

enum class FindAppIdResult { NoMatch, UniqueMatch, NonUniqueMatch };
// Looks for an app where prefs_key is set to search_value. Returns the apps id
// if there was only one app matching, otherwise returns an empty string.
FindAppIdResult FindAppId(const base::DictionaryValue* prefs,
                          base::StringPiece prefs_key,
                          base::StringPiece search_value,
                          std::string* result,
                          bool require_startup_notify = false,
                          bool need_display = false,
                          bool ignore_space = false) {
  result->clear();
  for (const auto& item : prefs->DictItems()) {
    if (item.first == kCrostiniTerminalId)
      continue;

    if (require_startup_notify &&
        !item.second
             .FindKeyOfType(kAppStartupNotifyKey, base::Value::Type::BOOLEAN)
             ->GetBool())
      continue;

    if (need_display) {
      const base::Value* no_display = item.second.FindKeyOfType(
          kAppNoDisplayKey, base::Value::Type::BOOLEAN);
      if (no_display && no_display->GetBool())
        continue;
    }

    const base::Value* value = item.second.FindKey(prefs_key);
    if (!value)
      continue;
    if (value->type() == base::Value::Type::STRING) {
      if (!MatchingString(search_value.as_string(), value->GetString(),
                          ignore_space)) {
        continue;
      }
    } else if (value->type() == base::Value::Type::DICTIONARY) {
      // Look at the unlocalized name to see if that matches.
      value = value->FindKeyOfType("", base::Value::Type::STRING);
      if (!value || !MatchingString(search_value.as_string(),
                                    value->GetString(), ignore_space)) {
        continue;
      }
    } else {
      continue;
    }

    if (!result->empty())
      return FindAppIdResult::NonUniqueMatch;
    *result = item.first;
  }

  if (!result->empty())
    return FindAppIdResult::UniqueMatch;
  return FindAppIdResult::NoMatch;
}

bool EqualsExcludingTimestamps(const base::Value& left,
                               const base::Value& right) {
  auto left_items = left.DictItems();
  auto right_items = right.DictItems();
  auto left_iter = left_items.begin();
  auto right_iter = right_items.begin();
  while (left_iter != left_items.end() && right_iter != right_items.end()) {
    if (left_iter->first == kAppInstallTimeKey ||
        left_iter->first == kAppLastLaunchTimeKey) {
      ++left_iter;
      continue;
    }
    if (right_iter->first == kAppInstallTimeKey ||
        right_iter->first == kAppLastLaunchTimeKey) {
      ++right_iter;
      continue;
    }
    if (*left_iter != *right_iter)
      return false;
    ++left_iter;
    ++right_iter;
  }
  return left_iter == left_items.end() && right_iter == right_items.end();
}

bool InstallIconFromFileThread(const base::FilePath& icon_path,
                               const std::string& content_png) {
  DCHECK(!content_png.empty());

  base::CreateDirectory(icon_path.DirName());

  int wrote =
      base::WriteFile(icon_path, content_png.c_str(), content_png.size());
  if (wrote != static_cast<int>(content_png.size())) {
    VLOG(2) << "Failed to write Crostini icon file: "
            << icon_path.MaybeAsASCII();
    if (!base::DeleteFile(icon_path, false)) {
      VLOG(2) << "Couldn't delete broken icon file" << icon_path.MaybeAsASCII();
    }
    return false;
  }

  return true;
}

void DeleteIconFolderFromFileThread(const base::FilePath& path) {
  DCHECK(path.DirName().BaseName().MaybeAsASCII() == kCrostiniIconFolder &&
         (!base::PathExists(path) || base::DirectoryExists(path)));
  const bool deleted = base::DeleteFile(path, true);
  DCHECK(deleted);
}

}  // namespace

CrostiniRegistryService::Registration::Registration(const base::Value* pref,
                                                    bool is_terminal_app)
    : is_terminal_app_(is_terminal_app) {
  DCHECK(pref || is_terminal_app);
  if (pref)
    pref_ = pref->Clone();
}

CrostiniRegistryService::Registration::~Registration() = default;

std::string CrostiniRegistryService::Registration::DesktopFileId() const {
  if (is_terminal_app_)
    return std::string();
  return pref_.FindKeyOfType(kAppDesktopFileIdKey, base::Value::Type::STRING)
      ->GetString();
}

std::string CrostiniRegistryService::Registration::VmName() const {
  if (is_terminal_app_)
    return kCrostiniDefaultVmName;
  return pref_.FindKeyOfType(kAppVmNameKey, base::Value::Type::STRING)
      ->GetString();
}

std::string CrostiniRegistryService::Registration::ContainerName() const {
  if (is_terminal_app_)
    return kCrostiniDefaultContainerName;
  return pref_.FindKeyOfType(kAppContainerNameKey, base::Value::Type::STRING)
      ->GetString();
}

std::string CrostiniRegistryService::Registration::Name() const {
  if (is_terminal_app_)
    return l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_NAME);
  return LocalizedString(kAppNameKey);
}

std::string CrostiniRegistryService::Registration::Comment() const {
  return LocalizedString(kAppCommentKey);
}

std::string CrostiniRegistryService::Registration::ExecutableFileName() const {
  if (pref_.is_none())
    return std::string();
  const base::Value* executable_file_name =
      pref_.FindKeyOfType(kAppExecutableFileNameKey, base::Value::Type::STRING);
  if (!executable_file_name)
    return std::string();
  return executable_file_name->GetString();
}

std::set<std::string> CrostiniRegistryService::Registration::MimeTypes() const {
  if (pref_.is_none())
    return {};
  return ListToStringSet(
      pref_.FindKeyOfType(kAppMimeTypesKey, base::Value::Type::LIST));
}

std::set<std::string> CrostiniRegistryService::Registration::Keywords() const {
  if (is_terminal_app_) {
    std::set<std::string> result = {"linux", "terminal", "crostini"};
    result.insert(
        l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_SEARCH_TERMS));
    return result;
  }
  return LocalizedList(kAppKeywordsKey);
}

bool CrostiniRegistryService::Registration::NoDisplay() const {
  if (pref_.is_none())
    return false;
  const base::Value* no_display =
      pref_.FindKeyOfType(kAppNoDisplayKey, base::Value::Type::BOOLEAN);
  if (no_display)
    return no_display->GetBool();
  return false;
}

std::string CrostiniRegistryService::Registration::PackageId() const {
  if (is_terminal_app_)
    return std::string();
  if (pref_.is_none())
    return std::string();
  const base::Value* package_id =
      pref_.FindKeyOfType(kAppPackageIdKey, base::Value::Type::STRING);
  if (!package_id)
    return std::string();
  return package_id->GetString();
}

bool CrostiniRegistryService::Registration::CanUninstall() const {
  if (pref_.is_none())
    return false;
  // We can uninstall if and only if there is a package that owns the
  // application. If no package owns the application, we don't know how to
  // uninstall the app.
  //
  // We don't check other things that might prevent us from uninstalling the
  // app. In particular, we don't check if there are other packages which
  // depend on the owning package. This should be rare for packages that have
  // desktop files, and it's better to show an error message (which the user can
  // then Google to learn more) than to just not have an uninstall option at
  // all.
  const base::Value* package_id =
      pref_.FindKeyOfType(kAppPackageIdKey, base::Value::Type::STRING);
  if (package_id)
    return !package_id->GetString().empty();
  return false;
}

base::Time CrostiniRegistryService::Registration::InstallTime() const {
  return GetTime(pref_, kAppInstallTimeKey);
}

base::Time CrostiniRegistryService::Registration::LastLaunchTime() const {
  return GetTime(pref_, kAppLastLaunchTimeKey);
}

bool CrostiniRegistryService::Registration::IsScaled() const {
  if (pref_.is_none())
    return false;
  const base::Value* scaled =
      pref_.FindKeyOfType(kAppScaledKey, base::Value::Type::BOOLEAN);
  if (!scaled)
    return false;
  return scaled->GetBool();
}

// We store in prefs all the localized values for given fields (formatted with
// undescores, e.g. 'fr' or 'en_US'), but users of the registry don't need to
// deal with this.
std::string CrostiniRegistryService::Registration::LocalizedString(
    base::StringPiece key) const {
  if (pref_.is_none())
    return std::string();
  const base::Value* dict =
      pref_.FindKeyOfType(key, base::Value::Type::DICTIONARY);
  if (!dict)
    return std::string();

  std::string current_locale =
      l10n_util::NormalizeLocale(g_browser_process->GetApplicationLocale());
  std::vector<std::string> locales;
  l10n_util::GetParentLocales(current_locale, &locales);
  // We use an empty locale as fallback.
  locales.push_back(std::string());

  for (const std::string& locale : locales) {
    const base::Value* value =
        dict->FindKeyOfType(locale, base::Value::Type::STRING);
    if (value)
      return value->GetString();
  }
  return std::string();
}

std::set<std::string> CrostiniRegistryService::Registration::LocalizedList(
    base::StringPiece key) const {
  if (pref_.is_none())
    return {};
  const base::Value* dict =
      pref_.FindKeyOfType(key, base::Value::Type::DICTIONARY);
  if (!dict)
    return {};

  std::string current_locale =
      l10n_util::NormalizeLocale(g_browser_process->GetApplicationLocale());
  std::vector<std::string> locales;
  l10n_util::GetParentLocales(current_locale, &locales);
  // We use an empty locale as fallback.
  locales.push_back(std::string());

  for (const std::string& locale : locales) {
    const base::Value* value =
        dict->FindKeyOfType(locale, base::Value::Type::LIST);
    if (value)
      return ListToStringSet(value);
  }
  return {};
}

CrostiniRegistryService::CrostiniRegistryService(Profile* profile)
    : profile_(profile),
      prefs_(profile->GetPrefs()),
      base_icon_path_(profile->GetPath().AppendASCII(kCrostiniIconFolder)),
      clock_(base::DefaultClock::GetInstance()) {
  RecordStartupMetrics();
}

CrostiniRegistryService::~CrostiniRegistryService() = default;

base::WeakPtr<CrostiniRegistryService> CrostiniRegistryService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// The code follows these steps to identify apps and returns the first match:
// 1) If the Startup Id is set, look for a matching desktop file id.
// 2) Ignore windows if the App Id is not set.
// 3) If the App Id is not prefixed by org.chromium.termina., it's an app with
// native Wayland support. Look for a matching desktop file id.
// 4) If the App Id is prefixed by org.chromium.termina.wmclass.:
// 4.1) Look for an app where StartupWMClass is matches the suffix.
// 4.2) Look for an app where the desktop file id matches the suffix.
// 4.3) Look for an app where the unlocalized name matches the suffix. This
//      handles the xterm & uxterm examples.
// 5) If we couldn't find a match, prefix the app id with 'crostini:' so we can
// easily identify shelf entries as Crostini apps.
std::string CrostiniRegistryService::GetCrostiniShelfAppId(
    const std::string* window_app_id,
    const std::string* window_startup_id) {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(prefs::kCrostiniRegistry);
  std::string app_id;

  if (window_startup_id) {
    // TODO(timloh): We should use a value that is unique so we can handle
    // an app installed in multiple containers.
    if (FindAppId(apps, kAppDesktopFileIdKey, *window_startup_id, &app_id,
                  true) == FindAppIdResult::UniqueMatch)
      return app_id;
    LOG(ERROR) << "Startup ID was set to '" << *window_startup_id
               << "' but not matched";
    // Try a lookup with the window app id.
  }

  if (!window_app_id)
    return std::string();

  // Wayland apps won't be prefixed with org.chromium.termina.
  if (!base::StartsWith(*window_app_id, kCrostiniWindowAppIdPrefix,
                        base::CompareCase::SENSITIVE)) {
    if (FindAppId(apps, kAppDesktopFileIdKey, *window_app_id, &app_id) ==
        FindAppIdResult::UniqueMatch) {
      return app_id;
    }
    return kCrostiniAppIdPrefix + *window_app_id;
  }

  base::StringPiece suffix(
      window_app_id->begin() + strlen(kCrostiniWindowAppIdPrefix),
      window_app_id->end());

  // If we don't have an id to match to a desktop file, use the window app id.
  if (!base::StartsWith(suffix, kWMClassPrefix, base::CompareCase::SENSITIVE))
    return kCrostiniAppIdPrefix + *window_app_id;

  // If an app had StartupWMClass set to the given WM class, use that,
  // otherwise look for a desktop file id matching the WM class.
  base::StringPiece key = suffix.substr(strlen(kWMClassPrefix));
  FindAppIdResult result = FindAppId(apps, kAppStartupWMClassKey, key, &app_id,
                                     false /* require_startup_notification */,
                                     true /* need_display */);
  if (result == FindAppIdResult::UniqueMatch)
    return app_id;
  if (result == FindAppIdResult::NonUniqueMatch)
    return kCrostiniAppIdPrefix + *window_app_id;

  if (FindAppId(apps, kAppDesktopFileIdKey, key, &app_id) ==
      FindAppIdResult::UniqueMatch) {
    return app_id;
  }

  if (FindAppId(apps, kAppNameKey, key, &app_id,
                false /* require_startup_notification */,
                true /* need_display */,
                true /* ignore_space */) == FindAppIdResult::UniqueMatch) {
    return app_id;
  }

  const std::string* app_name = GetAppNameForWMClass(key);
  if (app_name &&
      FindAppId(apps, kAppNameKey, *app_name, &app_id,
                false /* require_startup_notification */,
                true /* need_display */) == FindAppIdResult::UniqueMatch) {
    return app_id;
  }

  return kCrostiniAppIdPrefix + *window_app_id;
}

bool CrostiniRegistryService::IsCrostiniShelfAppId(
    const std::string& shelf_app_id) {
  if (base::StartsWith(shelf_app_id, kCrostiniAppIdPrefix,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }
  if (shelf_app_id == kCrostiniTerminalId)
    return true;
  // TODO(timloh): We need to handle desktop files that have been removed.
  // For example, running windows with a no-longer-valid app id will try to
  // use the ExtensionContextMenuModel.
  return prefs_->GetDictionary(prefs::kCrostiniRegistry)
             ->FindKey(shelf_app_id) != nullptr;
}

std::map<std::string, CrostiniRegistryService::Registration>
CrostiniRegistryService::GetRegisteredApps() const {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(prefs::kCrostiniRegistry);
  std::map<std::string, CrostiniRegistryService::Registration> result;
  for (const auto& item : apps->DictItems()) {
    result.emplace(item.first, Registration(&item.second,
                                            item.first == kCrostiniTerminalId));
  }
  if (!apps->FindKey(kCrostiniTerminalId)) {
    result.emplace(kCrostiniTerminalId, Registration(nullptr, true));
  }
  return result;
}

base::Optional<CrostiniRegistryService::Registration>
CrostiniRegistryService::GetRegistration(const std::string& app_id) const {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(prefs::kCrostiniRegistry);
  const base::Value* pref_registration =
      apps->FindKeyOfType(app_id, base::Value::Type::DICTIONARY);

  if (app_id == kCrostiniTerminalId)
    return base::make_optional<Registration>(pref_registration, true);

  if (!pref_registration)
    return base::nullopt;
  return base::make_optional<Registration>(pref_registration, false);
}

void CrostiniRegistryService::RecordStartupMetrics() {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(prefs::kCrostiniRegistry);

  if (!CrostiniFeatures::Get()->IsEnabled(profile_))
    return;

  size_t num_apps = 0;

  for (const auto& item : apps->DictItems()) {
    if (item.first == kCrostiniTerminalId)
      continue;

    const base::Value* no_display =
        item.second.FindKeyOfType(kAppNoDisplayKey, base::Value::Type::BOOLEAN);
    if (no_display && no_display->GetBool())
      continue;

    num_apps++;
  }
  UMA_HISTOGRAM_COUNTS_1000(kCrostiniAppsInstalledHistogram, num_apps);
}

base::FilePath CrostiniRegistryService::GetAppPath(
    const std::string& app_id) const {
  return base_icon_path_.AppendASCII(app_id);
}

base::FilePath CrostiniRegistryService::GetIconPath(
    const std::string& app_id,
    ui::ScaleFactor scale_factor) const {
  const base::FilePath app_path = GetAppPath(app_id);
  switch (scale_factor) {
    case ui::SCALE_FACTOR_100P:
      return app_path.AppendASCII("icon_100p.png");
    case ui::SCALE_FACTOR_125P:
      return app_path.AppendASCII("icon_125p.png");
    case ui::SCALE_FACTOR_133P:
      return app_path.AppendASCII("icon_133p.png");
    case ui::SCALE_FACTOR_140P:
      return app_path.AppendASCII("icon_140p.png");
    case ui::SCALE_FACTOR_150P:
      return app_path.AppendASCII("icon_150p.png");
    case ui::SCALE_FACTOR_180P:
      return app_path.AppendASCII("icon_180p.png");
    case ui::SCALE_FACTOR_200P:
      return app_path.AppendASCII("icon_200p.png");
    case ui::SCALE_FACTOR_250P:
      return app_path.AppendASCII("icon_250p.png");
    case ui::SCALE_FACTOR_300P:
      return app_path.AppendASCII("icon_300p.png");
    default:
      NOTREACHED();
      return base::FilePath();
  }
}

void CrostiniRegistryService::MaybeRequestIcon(const std::string& app_id,
                                               ui::ScaleFactor scale_factor) {
  // First check to see if this request is already in process or not.
  const auto active_iter = active_icon_requests_.find(app_id);
  if (active_iter != active_icon_requests_.end()) {
    if (active_iter->second & (1 << scale_factor)) {
      // Icon request already in progress.
      return;
    }
  }
  const auto retry_iter = retry_icon_requests_.find(app_id);
  if (retry_iter != retry_icon_requests_.end()) {
    if (retry_iter->second & (1 << scale_factor)) {
      // Icon request already setup to be retried when we are active.
      return;
    }
  }
  RequestIcon(app_id, scale_factor);
}

void CrostiniRegistryService::ClearApplicationList(
    const std::string& vm_name,
    const std::string& container_name) {
  std::vector<std::string> removed_apps;
  // The DictionaryPrefUpdate should be destructed before calling the observer.
  {
    DictionaryPrefUpdate update(prefs_, prefs::kCrostiniRegistry);
    base::DictionaryValue* apps = update.Get();

    for (const auto& item : apps->DictItems()) {
      if (item.first == kCrostiniTerminalId)
        continue;
      if (item.second.FindKey(kAppVmNameKey)->GetString() == vm_name &&
          (container_name.empty() ||
           item.second.FindKey(kAppContainerNameKey)->GetString() ==
               container_name))
        removed_apps.push_back(item.first);
    }
    for (const std::string& removed_app : removed_apps) {
      RemoveAppData(removed_app);
      apps->RemoveKey(removed_app);
    }
  }

  if (removed_apps.empty())
    return;
  std::vector<std::string> updated_apps;
  std::vector<std::string> inserted_apps;
  for (Observer& obs : observers_)
    obs.OnRegistryUpdated(this, updated_apps, removed_apps, inserted_apps);
}

void CrostiniRegistryService::UpdateApplicationList(
    const vm_tools::apps::ApplicationList& app_list) {
  if (app_list.vm_name().empty()) {
    LOG(WARNING) << "Received app list with missing VM name";
    return;
  }
  if (app_list.container_name().empty()) {
    LOG(WARNING) << "Received app list with missing container name";
    return;
  }

  // We need to compute the diff between the new list of apps and the old list
  // of apps (with matching vm/container names). We keep a set of the new app
  // ids so that we can compute these and update the Dictionary directly.
  std::set<std::string> new_app_ids;
  std::vector<std::string> updated_apps;
  std::vector<std::string> removed_apps;
  std::vector<std::string> inserted_apps;

  // The DictionaryPrefUpdate should be destructed before calling the observer.
  {
    DictionaryPrefUpdate update(prefs_, prefs::kCrostiniRegistry);
    base::DictionaryValue* apps = update.Get();
    for (const App& app : app_list.apps()) {
      if (app.desktop_file_id().empty()) {
        LOG(WARNING) << "Received app with missing desktop file id";
        continue;
      }

      base::Value name = ProtoToDictionary(app.name());
      if (name.FindKey(base::StringPiece()) == nullptr) {
        LOG(WARNING) << "Received app '" << app.desktop_file_id()
                     << "' with missing unlocalized name";
        continue;
      }

      std::string app_id = GenerateAppId(
          app.desktop_file_id(), app_list.vm_name(), app_list.container_name());
      new_app_ids.insert(app_id);

      base::Value pref_registration =
          AppPrefRegistrationFromApp(app, std::move(name), app_list);

      base::Value* old_app = apps->FindKey(app_id);
      if (old_app && EqualsExcludingTimestamps(pref_registration, *old_app))
        continue;

      base::Value* old_install_time = nullptr;
      base::Value* old_last_launch_time = nullptr;
      if (old_app) {
        updated_apps.push_back(app_id);
        old_install_time = old_app->FindKey(kAppInstallTimeKey);
        old_last_launch_time = old_app->FindKey(kAppLastLaunchTimeKey);
      } else {
        inserted_apps.push_back(app_id);
      }

      if (old_install_time)
        pref_registration.SetKey(kAppInstallTimeKey, old_install_time->Clone());
      else
        SetCurrentTime(&pref_registration, kAppInstallTimeKey);

      if (old_last_launch_time) {
        pref_registration.SetKey(kAppLastLaunchTimeKey,
                                 old_last_launch_time->Clone());
      }

      apps->SetKey(app_id, std::move(pref_registration));
    }

    for (const auto& item : apps->DictItems()) {
      if (item.first == kCrostiniTerminalId)
        continue;
      if (item.second.FindKey(kAppVmNameKey)->GetString() ==
              app_list.vm_name() &&
          item.second.FindKey(kAppContainerNameKey)->GetString() ==
              app_list.container_name() &&
          new_app_ids.find(item.first) == new_app_ids.end()) {
        removed_apps.push_back(item.first);
      }
    }

    for (const std::string& removed_app : removed_apps) {
      RemoveAppData(removed_app);
      apps->RemoveKey(removed_app);
    }
  }

  // When we receive notification of the application list then the container
  // *should* be online and we can retry all of our icon requests that failed
  // due to the container being offline.
  for (auto retry_iter = retry_icon_requests_.begin();
       retry_iter != retry_icon_requests_.end(); ++retry_iter) {
    for (ui::ScaleFactor scale_factor : ui::GetSupportedScaleFactors()) {
      if (retry_iter->second & (1 << scale_factor)) {
        RequestIcon(retry_iter->first, scale_factor);
      }
    }
  }
  retry_icon_requests_.clear();

  if (!updated_apps.empty() || !removed_apps.empty() ||
      !inserted_apps.empty()) {
    for (Observer& obs : observers_)
      obs.OnRegistryUpdated(this, updated_apps, removed_apps, inserted_apps);
  }
}

void CrostiniRegistryService::RemoveAppData(const std::string& app_id) {
  // Remove any pending requests we have for this icon.
  active_icon_requests_.erase(app_id);
  retry_icon_requests_.erase(app_id);

  // Remove local data on filesystem for the icons.
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteIconFolderFromFileThread, GetAppPath(app_id)));
}

void CrostiniRegistryService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CrostiniRegistryService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CrostiniRegistryService::AppLaunched(const std::string& app_id) {
  DictionaryPrefUpdate update(prefs_, prefs::kCrostiniRegistry);
  base::DictionaryValue* apps = update.Get();

  base::Value* app = apps->FindKey(app_id);
  if (!app) {
    DCHECK_EQ(app_id, kCrostiniTerminalId);
    base::Value pref(base::Value::Type::DICTIONARY);
    SetCurrentTime(&pref, kAppLastLaunchTimeKey);
    apps->SetKey(app_id, std::move(pref));
    return;
  }

  SetCurrentTime(app, kAppLastLaunchTimeKey);
}

void CrostiniRegistryService::SetCurrentTime(base::Value* dictionary,
                                             const char* key) const {
  DCHECK(dictionary);
  int64_t time = clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  dictionary->SetKey(key, base::Value(base::NumberToString(time)));
}

void CrostiniRegistryService::SetAppScaled(const std::string& app_id,
                                           bool scaled) {
  DCHECK_NE(app_id, kCrostiniTerminalId);

  DictionaryPrefUpdate update(prefs_, prefs::kCrostiniRegistry);
  base::DictionaryValue* apps = update.Get();

  base::Value* app = apps->FindKey(app_id);
  if (!app) {
    LOG(ERROR)
        << "Tried to set display scaled property on the app with this app_id "
        << app_id << " that doesn't exist in the registry.";
    return;
  }
  app->SetKey(kAppScaledKey, base::Value(scaled));
}

void CrostiniRegistryService::RequestIcon(const std::string& app_id,
                                          ui::ScaleFactor scale_factor) {
  // Ignore requests for app_id that isn't registered.
  base::Optional<CrostiniRegistryService::Registration> registration =
      GetRegistration(app_id);
  if (!registration) {
    VLOG(2) << "Request to load icon for non-registered app: " << app_id;
    return;
  }

  // Mark that we're doing a request for this icon so we don't duplicate
  // requests.
  active_icon_requests_[app_id] |= (1 << scale_factor);

  // Now make the call to request the actual icon.
  std::vector<std::string> desktop_file_ids{registration->DesktopFileId()};
  // We can only send integer scale factors to Crostini, so if we have a
  // non-integral scale factor we need round the scale factor. We do not expect
  // Crostini to give us back exactly what we ask for and we deal with that in
  // the CrostiniAppIcon class and may rescale the result in there to match our
  // needs.
  uint32_t icon_scale = 1;
  switch (scale_factor) {
    case ui::SCALE_FACTOR_180P:  // Close enough to 2, so use 2.
    case ui::SCALE_FACTOR_200P:
    case ui::SCALE_FACTOR_250P:  // Rounding scale factor down is better.
      icon_scale = 2;
      break;
    case ui::SCALE_FACTOR_300P:
      icon_scale = 3;
      break;
    default:
      break;
  }

  crostini::CrostiniManager::GetForProfile(profile_)->GetContainerAppIcons(
      registration->VmName(), registration->ContainerName(), desktop_file_ids,
      ash::AppListConfig::instance().grid_icon_dimension(), icon_scale,
      base::BindOnce(&CrostiniRegistryService::OnContainerAppIcon,
                     weak_ptr_factory_.GetWeakPtr(), app_id, scale_factor));
}

void CrostiniRegistryService::OnContainerAppIcon(
    const std::string& app_id,
    ui::ScaleFactor scale_factor,
    bool success,
    const std::vector<Icon>& icons) {
  if (!success) {
    // Add this to the list of retryable icon requests so we redo this when
    // we get feedback from the container that it's available.
    retry_icon_requests_[app_id] |= (1 << scale_factor);
    return;
  }
  if (icons.empty())
    return;
  // Now install the icon that we received.
  const base::FilePath icon_path = GetIconPath(app_id, scale_factor);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&InstallIconFromFileThread, icon_path, icons[0].content),
      base::BindOnce(&CrostiniRegistryService::OnIconInstalled,
                     weak_ptr_factory_.GetWeakPtr(), app_id, scale_factor));
}

void CrostiniRegistryService::OnIconInstalled(const std::string& app_id,
                                              ui::ScaleFactor scale_factor,
                                              bool success) {
  if (!success)
    return;

  for (Observer& obs : observers_)
    obs.OnAppIconUpdated(app_id, scale_factor);
}

}  // namespace crostini
