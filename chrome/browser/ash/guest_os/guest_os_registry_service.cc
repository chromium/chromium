// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"

#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/icon_transcoder/svg_icon_transcoder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chromeos_app_icon_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia_operations.h"

using vm_tools::apps::App;

namespace guest_os {

namespace {

void Launch(vm_tools::apps::VmType vm_type,
            std::string app_id,
            Profile* profile,
            const GURL& url) {
  switch (vm_type) {
    case VmType::TERMINA:
      crostini::LaunchCrostiniApp(profile, app_id, display::kInvalidDisplayId,
                                  {url.spec()}, base::DoNothing());
      break;

    case VmType::PLUGIN_VM:
      plugin_vm::LaunchPluginVmApp(profile, app_id, {url.spec()},
                                   base::DoNothing());
      break;

    case VmType::BOREALIS:
      borealis::BorealisServiceFactory::GetForProfile(profile)
          ->AppLauncher()
          .Launch(app_id, {url.spec()},
                  borealis::BorealisLaunchSource::kAppUrlHandler,
                  base::DoNothing());
      break;

    default:
      // Usual best practice is to exhaustively handle all enum cases, in order
      // to trigger a compiler warning if a newly-added enum case isn't handled.
      // However, this enum is generated, and the source proto lives in the CrOS
      // platform2 repo. If we attempted to exhaustively handle all cases,
      // adding a new enum entry would unavoidably break Chromium's build (since
      // warnings are treated as errors). So instead we have this default case,
      // and log unexpected values.
      LOG(ERROR) << "Unsupported VmType: " << static_cast<int>(VmType());
  }
}

bool AppHandlesProtocol(const GuestOsRegistryService::Registration& app,
                        const GURL& url) {
  if (app.VmType() == VmType::BOREALIS &&
      !borealis::IsExternalURLAllowed(url)) {
    return false;
  }
  return base::Contains(app.MimeTypes(), "x-scheme-handler/" + url.scheme());
}

// This prefix is used when generating the crostini app list id.
constexpr char kCrostiniAppIdPrefix[] = "crostini:";

constexpr char kCrostiniIconFolder[] = "crostini.icons";

base::Value::Dict ProtoToDictionary(const App::LocaleString& locale_string) {
  base::Value::Dict result;
  for (const App::LocaleString::Entry& entry : locale_string.values()) {
    const std::string& locale = entry.locale();

    std::string locale_with_dashes(locale);
    std::replace(locale_with_dashes.begin(), locale_with_dashes.end(), '_',
                 '-');
    if (!locale.empty() &&
        !l10n_util::IsValidLocaleSyntax(locale_with_dashes)) {
      continue;
    }

    result.Set(locale, base::Value(entry.value()));
  }
  return result;
}

std::set<std::string> ListToStringSet(const base::Value::List* list,
                                      bool to_lower_ascii = false) {
  std::set<std::string> result;
  if (!list) {
    return result;
  }
  for (const base::Value& value : *list) {
    result.insert(to_lower_ascii ? base::ToLowerASCII(value.GetString())
                                 : value.GetString());
  }
  return result;
}

base::Value::List ProtoToList(
    const google::protobuf::RepeatedPtrField<std::string>& strings) {
  base::Value::List result;
  for (const std::string& string : strings) {
    result.Append(string);
  }
  return result;
}

base::Value::Dict LocaleStringsProtoToDictionary(
    const App::LocaleStrings& repeated_locale_string) {
  base::Value::Dict result;
  for (const auto& strings_with_locale : repeated_locale_string.values()) {
    const std::string& locale = strings_with_locale.locale();

    std::string locale_with_dashes(locale);
    std::replace(locale_with_dashes.begin(), locale_with_dashes.end(), '_',
                 '-');
    if (!locale.empty() &&
        !l10n_util::IsValidLocaleSyntax(locale_with_dashes)) {
      continue;
    }
    result.Set(locale, ProtoToList(strings_with_locale.value()));
  }
  return result;
}

// Populate |pref_registration| based on the given App proto.
// |name| should be |app.name()| in Dictionary form.
void PopulatePrefRegistrationFromApp(base::Value::Dict& pref_registration,
                                     VmType vm_type,
                                     const std::string& vm_name,
                                     const std::string& container_name,
                                     const vm_tools::apps::App& app,
                                     base::Value::Dict name) {
  pref_registration.Set(guest_os::prefs::kAppDesktopFileIdKey,
                        base::Value(app.desktop_file_id()));
  pref_registration.Set(guest_os::prefs::kVmTypeKey, static_cast<int>(vm_type));
  pref_registration.Set(guest_os::prefs::kVmNameKey, base::Value(vm_name));
  pref_registration.Set(guest_os::prefs::kContainerNameKey,
                        base::Value(container_name));
  pref_registration.Set(guest_os::prefs::kAppNameKey, std::move(name));
  pref_registration.Set(guest_os::prefs::kAppExecKey, base::Value(app.exec()));
  pref_registration.Set(guest_os::prefs::kAppExecutableFileNameKey,
                        base::Value(app.executable_file_name()));
  pref_registration.Set(guest_os::prefs::kAppExtensionsKey,
                        ProtoToList(app.extensions()));
  pref_registration.Set(guest_os::prefs::kAppMimeTypesKey,
                        ProtoToList(app.mime_types()));
  pref_registration.Set(guest_os::prefs::kAppKeywordsKey,
                        LocaleStringsProtoToDictionary(app.keywords()));
  pref_registration.Set(guest_os::prefs::kAppNoDisplayKey,
                        base::Value(app.no_display()));
  pref_registration.Set(guest_os::prefs::kAppTerminalKey,
                        base::Value(app.terminal()));
  pref_registration.Set(guest_os::prefs::kAppStartupWMClassKey,
                        base::Value(app.startup_wm_class()));
  pref_registration.Set(guest_os::prefs::kAppStartupNotifyKey,
                        base::Value(app.startup_notify()));
  pref_registration.Set(guest_os::prefs::kAppPackageIdKey,
                        base::Value(app.package_id()));
}

bool EqualsExcludingTimestamps(const base::Value::Dict& left,
                               const base::Value::Dict& right) {
  auto left_iter = left.begin();
  auto right_iter = right.begin();
  while (left_iter != left.end() && right_iter != right.end()) {
    if (left_iter->first == guest_os::prefs::kAppInstallTimeKey ||
        left_iter->first == guest_os::prefs::kAppLastLaunchTimeKey) {
      ++left_iter;
      continue;
    }
    if (right_iter->first == guest_os::prefs::kAppInstallTimeKey ||
        right_iter->first == guest_os::prefs::kAppLastLaunchTimeKey) {
      ++right_iter;
      continue;
    }
    if (*left_iter != *right_iter)
      return false;
    ++left_iter;
    ++right_iter;
  }
  return left_iter == left.end() && right_iter == right.end();
}

void InstallIconFromFileThread(const base::FilePath& icon_path,
                               const std::string& content) {
  DCHECK(!content.empty());

  base::CreateDirectory(icon_path.DirName());

  if (!base::WriteFile(icon_path, content)) {
    VLOG(2) << "Failed to write Crostini icon file: "
            << icon_path.MaybeAsASCII();
    if (!base::DeleteFile(icon_path)) {
      VLOG(2) << "Couldn't delete broken icon file" << icon_path.MaybeAsASCII();
    }
  }
}

void DeleteIconFolderFromFileThread(const base::FilePath& path) {
  DCHECK(path.DirName().BaseName().MaybeAsASCII() == kCrostiniIconFolder &&
         (!base::PathExists(path) || base::DirectoryExists(path)));
  const bool deleted = base::DeletePathRecursively(path);
  DCHECK(deleted);
}

template <typename List>
static std::string Join(const List& list);

static std::string ToString(bool b) {
  return b ? "true" : "false";
}

static std::string ToString(int i) {
  return base::NumberToString(i);
}

static std::string ToString(const std::string& string) {
  return '"' + string + '"';
}

static std::string ToString(
    const google::protobuf::RepeatedPtrField<std::string>& list) {
  return Join(list);
}

static std::string ToString(
    const vm_tools::apps::App_LocaleString_Entry& entry) {
  return "{locale: " + ToString(entry.locale()) +
         ", value: " + ToString(entry.value()) + "}";
}

static std::string ToString(
    const vm_tools::apps::App_LocaleStrings_StringsWithLocale&
        strings_with_locale) {
  return "{locale: " + ToString(strings_with_locale.locale()) +
         ", value: " + ToString(strings_with_locale.value()) + "}";
}

static std::string ToString(const vm_tools::apps::App_LocaleString& string) {
  return Join(string.values());
}

static std::string ToString(const vm_tools::apps::App_LocaleStrings& strings) {
  return Join(strings.values());
}

static std::string ToString(const vm_tools::apps::App& app) {
  return "{desktop_file_id: " + ToString(app.desktop_file_id()) +
         ", name: " + ToString(app.name()) +
         ", comment: " + ToString(app.comment()) +
         ", mime_types: " + ToString(app.mime_types()) +
         ", no_display: " + ToString(app.no_display()) +
         ", terminal: " + ToString(app.terminal()) +
         ", startup_wm_class: " + ToString(app.startup_wm_class()) +
         ", startup_notify: " + ToString(app.startup_notify()) +
         ", keywords: " + ToString(app.keywords()) +
         ", exec: " + ToString(app.exec()) +
         ", executable_file_name: " + ToString(app.executable_file_name()) +
         ", package_id: " + ToString(app.package_id()) +
         ", extensions: " + ToString(app.extensions()) + "}";
}

static std::string ToString(const vm_tools::apps::ApplicationList& list) {
  return "{apps: " + Join(list.apps()) +
         ", vm_type: " + ToString(list.vm_type()) +
         ", vm_name: " + ToString(list.vm_name()) +
         ", container_name: " + ToString(list.container_name()) +
         ", owner_id: " + ToString(list.owner_id()) + "}";
}

template <typename List>
static std::string Join(const List& list) {
  std::string joined = "[";
  const char* seperator = "";
  for (const auto& list_item : list) {
    joined += seperator + ToString(list_item);
    seperator = ", ";
  }
  joined += "]";
  return joined;
}

std::string GetStringKey(const base::Value& dict, std::string_view key) {
  if (!dict.is_dict()) {
    return std::string();
  }
  const std::string* value = dict.GetDict().FindString(key);
  if (!value) {
    return std::string();
  }
  return *value;
}

}  // namespace

GuestOsRegistryService::Registration::Registration(std::string app_id,
                                                   base::Value pref)
    : app_id_(std::move(app_id)), pref_(std::move(pref)) {}

GuestOsRegistryService::Registration::~Registration() = default;

std::string GuestOsRegistryService::Registration::DesktopFileId() const {
  return GetString(guest_os::prefs::kAppDesktopFileIdKey);
}

VmType GuestOsRegistryService::Registration::VmType() const {
  return VmTypeFromPref(pref_);
}

std::string GuestOsRegistryService::Registration::VmName() const {
  return GetString(guest_os::prefs::kVmNameKey);
}

std::string GuestOsRegistryService::Registration::ContainerName() const {
  return GetString(guest_os::prefs::kContainerNameKey);
}

std::string GuestOsRegistryService::Registration::Name() const {
  if (VmType() == VmType::PLUGIN_VM) {
    return l10n_util::GetStringFUTF8(
        IDS_PLUGIN_VM_APP_NAME_WINDOWS_SUFFIX,
        base::UTF8ToUTF16(GetLocalizedString(guest_os::prefs::kAppNameKey)));
  }
  return GetLocalizedString(guest_os::prefs::kAppNameKey);
}

std::string GuestOsRegistryService::Registration::Exec() const {
  return GetString(guest_os::prefs::kAppExecKey);
}

std::string GuestOsRegistryService::Registration::ExecutableFileName() const {
  return GetString(guest_os::prefs::kAppExecutableFileNameKey);
}

std::set<std::string> GuestOsRegistryService::Registration::Extensions() const {
  if (!pref_.is_dict()) {
    return {};
  }
  // Convert to lowercase ASCII to allow case-insensitive match.
  return ListToStringSet(
      pref_.GetDict().FindList(guest_os::prefs::kAppExtensionsKey),
      /*to_lower_ascii=*/true);
}

std::set<std::string> GuestOsRegistryService::Registration::MimeTypes() const {
  if (!pref_.is_dict()) {
    return {};
  }
  // Convert to lowercase ASCII to allow case-insensitive match.
  return ListToStringSet(
      pref_.GetDict().FindList(guest_os::prefs::kAppMimeTypesKey),
      /*to_lower_ascii=*/true);
}

std::set<std::string> GuestOsRegistryService::Registration::Keywords() const {
  return GetLocalizedList(guest_os::prefs::kAppKeywordsKey);
}

bool GuestOsRegistryService::Registration::NoDisplay() const {
  return GetBool(guest_os::prefs::kAppNoDisplayKey);
}

bool GuestOsRegistryService::Registration::Terminal() const {
  return GetBool(guest_os::prefs::kAppTerminalKey);
}
std::string GuestOsRegistryService::Registration::PackageId() const {
  return GetString(guest_os::prefs::kAppPackageIdKey);
}

bool GuestOsRegistryService::Registration::CanUninstall() const {
  if (!pref_.is_dict()) {
    return false;
  }
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
  const std::string* package_id =
      pref_.GetDict().FindString(guest_os::prefs::kAppPackageIdKey);
  if (package_id) {
    return !package_id->empty();
  }
  return false;
}

guest_os::GuestId GuestOsRegistryService::Registration::ToGuestId() const {
  return guest_os::GuestId(VmType(), VmName(), ContainerName());
}

base::Time GuestOsRegistryService::Registration::InstallTime() const {
  return GetTime(guest_os::prefs::kAppInstallTimeKey);
}

base::Time GuestOsRegistryService::Registration::LastLaunchTime() const {
  return GetTime(guest_os::prefs::kAppLastLaunchTimeKey);
}

bool GuestOsRegistryService::Registration::IsScaled() const {
  return GetBool(guest_os::prefs::kAppScaledKey);
}

std::string GuestOsRegistryService::Registration::StartupWmClass() const {
  return GetString(guest_os::prefs::kAppStartupWMClassKey);
}

bool GuestOsRegistryService::Registration::StartupNotify() const {
  return GetBool(guest_os::prefs::kAppStartupNotifyKey);
}

std::string GuestOsRegistryService::Registration::GetString(
    std::string_view key) const {
  return GetStringKey(pref_, key);
}

bool GuestOsRegistryService::Registration::GetBool(std::string_view key) const {
  if (!pref_.is_dict()) {
    return false;
  }
  const std::optional<bool> value = pref_.GetDict().FindBool(key);
  return value.value_or(false);
}

// This is the companion to GuestOsRegistryService::SetCurrentTime().
base::Time GuestOsRegistryService::Registration::GetTime(
    std::string_view key) const {
  if (!pref_.is_dict()) {
    return base::Time();
  }
  const std::string* value = pref_.GetDict().FindString(key);
  int64_t time;
  if (!value || !base::StringToInt64(*value, &time)) {
    return base::Time();
  }
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(time));
}

// We store in prefs all the localized values for given fields (formatted with
// undescores, e.g. 'fr' or 'en_US'), but users of the registry don't need to
// deal with this.
std::string GuestOsRegistryService::Registration::GetLocalizedString(
    std::string_view key) const {
  if (!pref_.is_dict()) {
    return std::string();
  }
  const base::Value::Dict* dict = pref_.GetDict().FindDict(key);
  if (!dict) {
    return std::string();
  }

  std::string current_locale =
      l10n_util::NormalizeLocale(g_browser_process->GetApplicationLocale());
  std::vector<std::string> locales;
  l10n_util::GetParentLocales(current_locale, &locales);
  // We use an empty locale as fallback.
  locales.push_back(std::string());

  for (const std::string& locale : locales) {
    const std::string* value = dict->FindString(locale);
    if (value) {
      return *value;
    }
  }
  return std::string();
}

std::set<std::string> GuestOsRegistryService::Registration::GetLocalizedList(
    std::string_view key) const {
  if (!pref_.is_dict()) {
    return {};
  }
  const base::Value::Dict* dict = pref_.GetDict().FindDict(key);
  if (!dict) {
    return {};
  }

  std::string current_locale =
      l10n_util::NormalizeLocale(g_browser_process->GetApplicationLocale());
  std::vector<std::string> locales;
  l10n_util::GetParentLocales(current_locale, &locales);
  // We use an empty locale as fallback.
  locales.push_back(std::string());

  for (const std::string& locale : locales) {
    const base::Value::List* list = dict->FindList(locale);
    if (list) {
      return ListToStringSet(list);
    }
  }
  return {};
}

GuestOsRegistryService::GuestOsRegistryService(Profile* profile)
    : profile_(profile),
      prefs_(profile->GetPrefs()),
      base_icon_path_(profile->GetPath().AppendASCII(kCrostiniIconFolder)),
      clock_(base::DefaultClock::GetInstance()),
      svg_icon_transcoder_(std::make_unique<apps::SvgIconTranscoder>(profile)) {
}

GuestOsRegistryService::~GuestOsRegistryService() = default;

base::WeakPtr<GuestOsRegistryService> GuestOsRegistryService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::map<std::string, GuestOsRegistryService::Registration>
GuestOsRegistryService::GetAllRegisteredApps() const {
  const base::Value::Dict& apps =
      prefs_->GetDict(guest_os::prefs::kGuestOsRegistry);
  std::map<std::string, GuestOsRegistryService::Registration> result;
  for (const auto item : apps) {
    result.emplace(item.first, Registration(item.first, item.second.Clone()));
  }
  return result;
}

std::map<std::string, GuestOsRegistryService::Registration>
GuestOsRegistryService::GetEnabledApps() const {
  bool crostini_enabled =
      crostini::CrostiniFeatures::Get()->IsEnabled(profile_);
  bool plugin_vm_enabled =
      plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile_);
  bool borealis_enabled =
      borealis::BorealisServiceFactory::GetForProfile(profile_)
          ->Features()
          .IsEnabled();
  if (!crostini_enabled && !plugin_vm_enabled && !borealis_enabled) {
    return {};
  }

  auto apps = GetAllRegisteredApps();
  for (auto it = apps.cbegin(); it != apps.cend();) {
    bool enabled = false;
    switch (it->second.VmType()) {
      case VmType::TERMINA:
        enabled = crostini_enabled;
        break;
      case VmType::PLUGIN_VM:
        enabled = plugin_vm_enabled;
        break;
      case VmType::BOREALIS:
        enabled = borealis_enabled;
        break;
      default:
        LOG(ERROR) << "Unsupported VmType: "
                   << static_cast<int>(it->second.VmType());
    }
    if (enabled) {
      ++it;
    } else {
      it = apps.erase(it);
    }
  }
  return apps;
}

std::map<std::string, GuestOsRegistryService::Registration>
GuestOsRegistryService::GetRegisteredApps(VmType vm_type) const {
  auto apps = GetAllRegisteredApps();
  for (auto it = apps.cbegin(); it != apps.cend();) {
    if (it->second.VmType() == vm_type) {
      ++it;
    } else {
      it = apps.erase(it);
    }
  }
  return apps;
}

std::optional<GuestOsRegistryService::Registration>
GuestOsRegistryService::GetRegistration(const std::string& app_id) const {
  const base::Value::Dict& apps =
      prefs_->GetDict(guest_os::prefs::kGuestOsRegistry);

  const base::Value::Dict* pref_registration = apps.FindDict(app_id);
  if (!pref_registration) {
    return std::nullopt;
  }
  return std::make_optional<Registration>(
      app_id, base::Value(pref_registration->Clone()));
}

void GuestOsRegistryService::RegisterTransientUrlHandler(
    GuestOsUrlHandler handler,
    CanHandleUrlCallback canHandleCallback) {
  url_handlers_.emplace_back(handler, canHandleCallback);
}

std::optional<GuestOsUrlHandler> GuestOsRegistryService::GetHandler(
    const GURL& url) const {
  // Transient URL handlers are system-installed, so always take priority.
  for (const auto& handler : url_handlers_) {
    if (handler.second.Run(url)) {
      return handler.first;
    }
  }

  std::map<std::string, Registration> apps = GetEnabledApps();
  const Registration* result = nullptr;
  for (auto& [unused, registration] : apps) {
    if (AppHandlesProtocol(registration, url) &&
        (!result || registration.LastLaunchTime() > result->LastLaunchTime())) {
      result = &registration;
    }
  }
  if (!result) {
    return std::nullopt;
  }
  return std::make_optional<GuestOsUrlHandler>(
      result->Name(),
      base::BindRepeating(Launch, result->VmType(), result->app_id()));
}

base::FilePath GuestOsRegistryService::GetAppPath(
    const std::string& app_id) const {
  return base_icon_path_.AppendASCII(app_id);
}

base::FilePath GuestOsRegistryService::GetIconPath(
    const std::string& app_id,
    ui::ResourceScaleFactor scale_factor) const {
  const base::FilePath app_path = GetAppPath(app_id);
  switch (scale_factor) {
    case ui::k100Percent:
      return app_path.AppendASCII("icon_100p.png");
    case ui::k200Percent:
      return app_path.AppendASCII("icon_200p.png");
    case ui::k300Percent:
      return app_path.AppendASCII("icon_300p.png");
    case ui::kScaleFactorNone:
      return app_path.AppendASCII("icon.svg");
    default:
      NOTREACHED_IN_MIGRATION();
      return base::FilePath();
  }
}

void GuestOsRegistryService::LoadIcon(const std::string& app_id,
                                      const apps::IconKey& icon_key,
                                      apps::IconType icon_type,
                                      int32_t size_hint_in_dip,
                                      bool allow_placeholder_icon,
                                      int fallback_icon_resource_id,
                                      apps::LoadIconCallback callback) {
  // Add container-badging to all crostini apps except the terminal, which is
  // shared between containers. This is part of the multi-container UI, so is
  // guarded by a flag.
  if (crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(profile_)) {
    auto reg = GetRegistration(app_id);
    if (reg && reg->VmType() == VmType::TERMINA) {
      callback = base::BindOnce(
          &GuestOsRegistryService::ApplyContainerBadgeWithCallback,
          weak_ptr_factory_.GetWeakPtr(),
          crostini::GetContainerBadgeColor(
              profile_, guest_os::GuestId(reg->VmType(), reg->VmName(),
                                          reg->ContainerName())),
          std::move(callback));
    }
  }

  if (icon_key.resource_id != apps::IconKey::kInvalidResourceId) {
    // The icon is a resource built into the Chrome OS binary.
    constexpr bool is_placeholder_icon = false;
    apps::LoadIconFromResource(
        profile_, app_id, icon_type, size_hint_in_dip, icon_key.resource_id,
        is_placeholder_icon,
        static_cast<apps::IconEffects>(icon_key.icon_effects),
        std::move(callback));
    return;
  }

  // There are paths where nothing higher up the call stack will resize so
  // we need to ensure that returned icons are always resized to be
  // size_hint_in_dip big. crbug/1170455 is an example.
  apps::IconEffects icon_effects = static_cast<apps::IconEffects>(
      icon_key.icon_effects | apps::IconEffects::kMdIconStyle);
  auto scale_factor = apps_util::GetPrimaryDisplayUIScaleFactor();

  auto load_icon_from_vm_fallback = base::BindOnce(
      &GuestOsRegistryService::LoadIconFromVM, weak_ptr_factory_.GetWeakPtr(),
      app_id, icon_type, size_hint_in_dip, scale_factor, icon_effects,
      fallback_icon_resource_id);

  auto transcode_svg_fallback = base::BindOnce(
      &GuestOsRegistryService::TranscodeIconFromSvg,
      weak_ptr_factory_.GetWeakPtr(), GetIconPath(app_id, ui::kScaleFactorNone),
      GetIconPath(app_id, scale_factor), icon_type, size_hint_in_dip,
      icon_effects, std::move(load_icon_from_vm_fallback));

  // Try loading the icon from an on-disk cache. If that fails, try to transcode
  // the app's svg icon, and if that fails, fall back
  // to LoadIconFromVM.
  apps::LoadIconFromFileWithFallback(
      icon_type, size_hint_in_dip, GetIconPath(app_id, scale_factor),
      icon_effects, std::move(callback), std::move(transcode_svg_fallback));
}

void GuestOsRegistryService::ApplyContainerBadge(
    const std::optional<std::string>& app_id,
    gfx::ImageSkia* image_skia) {
  if (crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(profile_)) {
    auto reg = GetRegistration(*app_id);
    if (reg && reg->VmType() == guest_os::VmType::TERMINA) {
      ApplyContainerBadgeForImageSkiaIcon(
          crostini::GetContainerBadgeColor(
              profile_, guest_os::GuestId(reg->VmType(), reg->VmName(),
                                          reg->ContainerName())),
          image_skia);
    }
  }
}

void GuestOsRegistryService::ApplyContainerBadgeForImageSkiaIcon(
    SkColor badge_color,
    gfx::ImageSkia* icon_out) {
  gfx::ImageSkia badge_mask =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_ICON_BADGE_MASK);

  if (badge_mask.size() != icon_out->size()) {
    badge_mask = gfx::ImageSkiaOperations::CreateResizedImage(
        badge_mask, skia::ImageOperations::RESIZE_BEST, icon_out->size());
  }
  badge_mask =
      gfx::ImageSkiaOperations::CreateColorMask(badge_mask, badge_color);
  *icon_out =
      gfx::ImageSkiaOperations::CreateSuperimposedImage(*icon_out, badge_mask);
}

void GuestOsRegistryService::ApplyContainerBadgeWithCallback(
    SkColor badge_color,
    apps::LoadIconCallback callback,
    apps::IconValuePtr icon) {
  gfx::ImageSkia badge_mask =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_ICON_BADGE_MASK);

  if (badge_mask.size() != icon->uncompressed.size()) {
    badge_mask = gfx::ImageSkiaOperations::CreateResizedImage(
        badge_mask, skia::ImageOperations::RESIZE_BEST,
        icon->uncompressed.size());
  }
  badge_mask =
      gfx::ImageSkiaOperations::CreateColorMask(badge_mask, badge_color);
  icon->uncompressed = gfx::ImageSkiaOperations::CreateSuperimposedImage(
      icon->uncompressed, badge_mask);

  std::move(callback).Run(std::move(icon));
}

void GuestOsRegistryService::TranscodeIconFromSvg(
    base::FilePath svg_path,
    base::FilePath png_path,
    apps::IconType icon_type,
    int32_t size_hint_in_dip,
    apps::IconEffects icon_effects,
    base::OnceCallback<void(apps::LoadIconCallback)> fallback,
    apps::LoadIconCallback callback) {
  svg_icon_transcoder_->Transcode(
      std::move(svg_path), std::move(png_path), gfx::Size(128, 128),
      base::BindOnce(
          [](apps::IconType icon_type, int32_t size_hint_in_dip,
             apps::IconEffects icon_effects, apps::LoadIconCallback callback,
             base::OnceCallback<void(apps::LoadIconCallback)> fallback,
             std::string icon_content) {
            if (!icon_content.empty()) {
              apps::LoadIconFromCompressedData(
                  icon_type, size_hint_in_dip, icon_effects,
                  std::move(icon_content), std::move(callback));
              return;
            }
            if (fallback) {
              std::move(fallback).Run(std::move(callback));
            }
          },
          icon_type, size_hint_in_dip, icon_effects, std::move(callback),
          std::move(fallback)));
}

void GuestOsRegistryService::LoadIconFromVM(
    const std::string& app_id,
    apps::IconType icon_type,
    int32_t size_hint_in_dip,
    ui::ResourceScaleFactor scale_factor,
    apps::IconEffects icon_effects,
    int fallback_icon_resource_id,
    apps::LoadIconCallback callback) {
  RequestIcon(app_id, scale_factor,
              base::BindOnce(&GuestOsRegistryService::OnLoadIconFromVM,
                             weak_ptr_factory_.GetWeakPtr(), app_id, icon_type,
                             size_hint_in_dip, icon_effects,
                             fallback_icon_resource_id, std::move(callback)));
}

void GuestOsRegistryService::OnLoadIconFromVM(
    const std::string& app_id,
    apps::IconType icon_type,
    int32_t size_hint_in_dip,
    apps::IconEffects icon_effects,
    int fallback_icon_resource_id,
    apps::LoadIconCallback callback,
    std::string compressed_icon_data) {
  if (compressed_icon_data.empty()) {
    if (fallback_icon_resource_id != apps::IconKey::kInvalidResourceId) {
      // We load the fallback icon, but we tell AppsService that this is not
      // a placeholder to avoid endless repeat calls since we don't expect to
      // find a better icon than this any time soon.
      apps::LoadIconFromResource(profile_, app_id, icon_type, size_hint_in_dip,
                                 fallback_icon_resource_id,
                                 /*is_placeholder_icon=*/false, icon_effects,
                                 std::move(callback));
    } else {
      std::move(callback).Run(std::make_unique<apps::IconValue>());
    }
  } else {
    apps::LoadIconFromCompressedData(icon_type, size_hint_in_dip, icon_effects,
                                     compressed_icon_data, std::move(callback));
  }
}

void GuestOsRegistryService::RequestIcon(
    const std::string& app_id,
    ui::ResourceScaleFactor scale_factor,
    base::OnceCallback<void(std::string)> callback) {
  if (!GetRegistration(app_id)) {
    // App isn't registered (e.g. a GUI app launched from within Crostini
    // that doesn't have a .desktop file). Can't get an icon for that case so
    // return an empty icon.
    std::move(callback).Run({});
    return;
  }

  // Coalesce calls to the container.
  auto& callbacks = active_icon_requests_[{app_id, scale_factor}];
  callbacks.emplace_back(std::move(callback));
  if (callbacks.size() > 1) {
    return;
  }
  RequestContainerAppIcon(app_id, scale_factor);
}

void GuestOsRegistryService::ClearApplicationList(
    VmType vm_type,
    const std::string& vm_name,
    const std::string& container_name) {
  std::vector<std::string> removed_apps;
  // The ScopedDictPrefUpdate should be destructed before calling the observer.
  {
    ScopedDictPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
    base::Value::Dict& apps = update.Get();

    for (const auto item : apps) {
      Registration registration(item.first, item.second.Clone());
      if (vm_type != registration.VmType()) {
        continue;
      }
      if (vm_name != registration.VmName()) {
        continue;
      }
      if (!container_name.empty() &&
          container_name != registration.ContainerName()) {
        continue;
      }
      removed_apps.push_back(item.first);
    }
    for (const std::string& removed_app : removed_apps) {
      RemoveAppData(removed_app);
      apps.Remove(removed_app);
    }
  }

  if (removed_apps.empty()) {
    return;
  }

  std::vector<std::string> updated_apps;
  std::vector<std::string> inserted_apps;
  for (Observer& obs : observers_) {
    obs.OnRegistryUpdated(this, vm_type, updated_apps, removed_apps,
                          inserted_apps);
  }
}

void GuestOsRegistryService::UpdateApplicationList(
    const vm_tools::apps::ApplicationList& app_list) {
  VLOG(3) << "Received ApplicationList : " << ToString(app_list);
  // TODO(b/294316866): Special-case Bruschetta VMs until cicerone is updated to
  // use the correct vm_type.
  vm_tools::apps::VmType vm_type = app_list.vm_type();
  if (app_list.vm_name() == bruschetta::kBruschettaVmName) {
    vm_type = vm_tools::apps::VmType::BRUSCHETTA;
  }

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

  // The ScopedDictPrefUpdate should be destructed before calling the observer.
  {
    ScopedDictPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
    base::Value::Dict& apps = update.Get();
    for (const App& app : app_list.apps()) {
      if (app.desktop_file_id().empty()) {
        LOG(WARNING) << "Received app with missing desktop file id";
        continue;
      }

      base::Value::Dict name = ProtoToDictionary(app.name());
      if (name.Find(std::string_view()) == nullptr) {
        LOG(WARNING) << "Received app '" << app.desktop_file_id()
                     << "' with missing unlocalized name";
        continue;
      }

      std::string app_id = GenerateAppId(
          app.desktop_file_id(), app_list.vm_name(), app_list.container_name());
      new_app_ids.insert(app_id);

      base::Value::Dict pref_registration;
      PopulatePrefRegistrationFromApp(
          pref_registration, vm_type, app_list.vm_name(),
          app_list.container_name(), app, std::move(name));

      base::Value::Dict* old_app = apps.FindDict(app_id);
      if (old_app && EqualsExcludingTimestamps(pref_registration, *old_app)) {
        continue;
      }

      base::Value* old_install_time = nullptr;
      base::Value* old_last_launch_time = nullptr;
      if (old_app) {
        updated_apps.push_back(app_id);
        old_install_time = old_app->Find(guest_os::prefs::kAppInstallTimeKey);
        old_last_launch_time =
            old_app->Find(guest_os::prefs::kAppLastLaunchTimeKey);
      } else {
        inserted_apps.push_back(app_id);
      }

      if (old_install_time) {
        pref_registration.Set(guest_os::prefs::kAppInstallTimeKey,
                              old_install_time->Clone());
      } else {
        SetCurrentTime(pref_registration, guest_os::prefs::kAppInstallTimeKey);
      }

      if (old_last_launch_time) {
        pref_registration.Set(guest_os::prefs::kAppLastLaunchTimeKey,
                              old_last_launch_time->Clone());
      }

      apps.Set(app_id, std::move(pref_registration));
    }

    for (const auto item : apps) {
      std::string vm_name =
          GetStringKey(item.second, guest_os::prefs::kVmNameKey);
      std::string container_name =
          GetStringKey(item.second, guest_os::prefs::kContainerNameKey);
      if (vm_name.empty() || container_name.empty()) {
        LOG(WARNING) << "Detected app with empty vm or container name";
        removed_apps.push_back(item.first);
      } else if (vm_name == app_list.vm_name() &&
                 container_name == app_list.container_name() &&
                 new_app_ids.find(item.first) == new_app_ids.end()) {
        removed_apps.push_back(item.first);
      }
    }

    for (const std::string& removed_app : removed_apps) {
      RemoveAppData(removed_app);
      apps.Remove(removed_app);
    }
  }

  // When we receive notification of the application list then the container
  // *should* be online and we can retry all of our icon requests that failed
  // due to the container being offline.
  for (auto retry_iter = retry_icon_requests_.begin();
       retry_iter != retry_icon_requests_.end(); ++retry_iter) {
    for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
      if (retry_iter->second & (1 << scale_factor)) {
        RequestContainerAppIcon(retry_iter->first, scale_factor);
      }
    }
  }
  retry_icon_requests_.clear();

  if (updated_apps.empty() && removed_apps.empty() && inserted_apps.empty()) {
    return;
  }

  for (Observer& obs : observers_) {
    obs.OnRegistryUpdated(this, vm_type, updated_apps, removed_apps,
                          inserted_apps);
  }
}

void GuestOsRegistryService::ContainerBadgeColorChanged(
    const guest_os::GuestId& container_id) {
  std::vector<std::string> updated_apps;

  for (const auto& it : GetAllRegisteredApps()) {
    if (it.second.VmName() == container_id.vm_name &&
        it.second.ContainerName() == container_id.container_name) {
      updated_apps.push_back(it.first);
    }
  }

  std::vector<std::string> removed_apps;
  std::vector<std::string> inserted_apps;
  for (Observer& obs : observers_) {
    obs.OnRegistryUpdated(this, VmType::TERMINA, updated_apps, removed_apps,
                          inserted_apps);
  }
}

void GuestOsRegistryService::RemoveAppData(const std::string& app_id) {
  // Remove any pending requests we have for this icon.
  retry_icon_requests_.erase(app_id);

  // Remove local data on filesystem for the icons.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteIconFolderFromFileThread, GetAppPath(app_id)));
}

void GuestOsRegistryService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GuestOsRegistryService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GuestOsRegistryService::AppLaunched(const std::string& app_id) {
  ScopedDictPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
  base::Value::Dict& app = update->Find(app_id)->GetDict();
  SetCurrentTime(app, guest_os::prefs::kAppLastLaunchTimeKey);

  auto vm_type = app.FindInt(guest_os::prefs::kVmTypeKey);
  if (!vm_type.has_value()) {
    LOG(ERROR) << "Failed to find " << guest_os::prefs::kVmTypeKey
               << " for app " << app_id;
    return;
  }

  for (Observer& obs : observers_) {
    obs.OnAppLastLaunchTimeUpdated(static_cast<VmType>(vm_type.value()), app_id,
                                   clock_->Now());
  }
}

void GuestOsRegistryService::SetCurrentTime(base::Value::Dict& dictionary,
                                            const char* key) const {
  int64_t time = clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  dictionary.Set(key, base::Value(base::NumberToString(time)));
}

void GuestOsRegistryService::SetAppScaled(const std::string& app_id,
                                          bool scaled) {
  ScopedDictPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
  base::Value::Dict& apps = update.Get();

  base::Value::Dict* app = apps.FindDict(app_id);
  if (!app) {
    LOG(ERROR)
        << "Tried to set display scaled property on the app with this app_id "
        << app_id << " that doesn't exist in the registry.";
    return;
  }
  app->Set(guest_os::prefs::kAppScaledKey, scaled);
}

// static
std::string GuestOsRegistryService::GenerateAppId(
    const std::string& desktop_file_id,
    const std::string& vm_name,
    const std::string& container_name) {
  // These can collide in theory because the user could choose VM and container
  // names which contain slashes, but this will only result in apps missing from
  // the launcher.
  return crx_file::id_util::GenerateId(kCrostiniAppIdPrefix + vm_name + "/" +
                                       container_name + "/" + desktop_file_id);
}

void GuestOsRegistryService::RequestContainerAppIcon(
    const std::string& app_id,
    ui::ResourceScaleFactor scale_factor) {
  // Ignore requests for app_id that isn't registered.
  std::optional<GuestOsRegistryService::Registration> registration =
      GetRegistration(app_id);
  DCHECK(registration);
  if (!registration) {
    LOG(ERROR) << "Request to load icon for non-registered app: " << app_id;
    return;
  }
  VLOG(1) << "Request to load icon for app: " << app_id;

  // Now make the call to request the actual icon.
  std::vector<std::string> desktop_file_ids{registration->DesktopFileId()};
  // We can only send integer scale factors to Crostini, so if we have a
  // non-integral scale factor we need round the scale factor. We do not expect
  // Crostini to give us back exactly what we ask for and we deal with that in
  // the CrostiniAppIcon class and may rescale the result in there to match our
  // needs.
  uint32_t icon_scale = 1;
  switch (scale_factor) {
    case ui::k200Percent:
      icon_scale = 2;
      break;
    case ui::k300Percent:
      icon_scale = 3;
      break;
    default:
      break;
  }

  crostini::CrostiniManager::GetForProfile(profile_)->GetContainerAppIcons(
      guest_os::GuestId(registration->VmType(), registration->VmName(),
                        registration->ContainerName()),
      desktop_file_ids,
      ash::SharedAppListConfig::instance().default_grid_icon_dimension(),
      icon_scale,
      base::BindOnce(&GuestOsRegistryService::OnContainerAppIcon,
                     weak_ptr_factory_.GetWeakPtr(), app_id, scale_factor));
}

void GuestOsRegistryService::InvokeActiveIconCallbacks(
    std::string app_id,
    ui::ResourceScaleFactor scale_factor,
    std::string icon_content) {
  // Invoke all active icon request callbacks with the icon.
  auto key =
      std::pair<std::string, ui::ResourceScaleFactor>(app_id, scale_factor);
  auto& callbacks = active_icon_requests_[key];
  VLOG(1) << "Invoking icon callbacks for app: " << app_id
          << ", num callbacks: " << callbacks.size();
  for (auto& callback : callbacks) {
    std::move(callback).Run(icon_content);
  }
  active_icon_requests_.erase(key);
}

void GuestOsRegistryService::OnSvgIconTranscoded(
    std::string app_id,
    ui::ResourceScaleFactor scale_factor,
    std::string svg_icon_content,
    std::string png_icon_content) {
  if (png_icon_content.empty()) {
    VLOG(1) << "Failed to transcode svg icon for " << app_id;
  }
  // Write svg to disk, then invoke active callbacks with png content.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&InstallIconFromFileThread,
                     GetIconPath(app_id, ui::kScaleFactorNone),
                     std::move(svg_icon_content)),
      base::BindOnce(&GuestOsRegistryService::InvokeActiveIconCallbacks,
                     weak_ptr_factory_.GetWeakPtr(), app_id, scale_factor,
                     std::move(png_icon_content)));
}

void GuestOsRegistryService::OnContainerAppIcon(
    const std::string& app_id,
    ui::ResourceScaleFactor scale_factor,
    bool success,
    const std::vector<crostini::Icon>& icons) {
  std::string icon_content;
  if (!success) {
    VLOG(1) << "Failed to load icon for app: " << app_id;
    // Add this to the list of retryable icon requests so we redo this when
    // we get feedback from the container that it's available.
    retry_icon_requests_[app_id] |= (1 << scale_factor);
    InvokeActiveIconCallbacks(app_id, scale_factor, std::string());
    return;
  }

  if (icons.empty()) {
    VLOG(1) << "No icon in container for app: " << app_id;
    InvokeActiveIconCallbacks(app_id, scale_factor, std::string());
    return;
  }

  // Install the icon that we received, and invoke active callbacks.
  const base::FilePath icon_path = GetIconPath(app_id, scale_factor);
  bool is_svg = icons[0].format == vm_tools::cicerone::DesktopIcon::SVG;
  VLOG(1) << "Found icon in container for app: " << app_id
          << " path: " << icon_path << " format: " << (is_svg ? "svg" : "png")
          << " bytes: " << icons[0].content.size();
  if (is_svg) {
    svg_icon_transcoder_->Transcode(
        icons[0].content, std::move(icon_path), gfx::Size(128, 128),
        base::BindOnce(&GuestOsRegistryService::OnSvgIconTranscoded,
                       weak_ptr_factory_.GetWeakPtr(), app_id, scale_factor,
                       icons[0].content));
    return;
  }

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&InstallIconFromFileThread, std::move(icon_path),
                     icons[0].content),
      base::BindOnce(&GuestOsRegistryService::InvokeActiveIconCallbacks,
                     weak_ptr_factory_.GetWeakPtr(), app_id, scale_factor,
                     icons[0].content));
}

}  // namespace guest_os
