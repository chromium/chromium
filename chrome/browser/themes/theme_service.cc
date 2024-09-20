// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/new_tab_page/chrome_colors/chrome_colors_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/themes/theme_service_utils.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry_observer.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#include "ui/ozone/public/ozone_platform.h"  // nogncheck
#endif

using TP = ThemeProperties;

// Helpers --------------------------------------------------------------------

namespace {

// Wait this many seconds after startup to garbage collect unused themes.
// Removing unused themes is done after a delay because there is no
// reason to do it at startup.
// ExtensionService::GarbageCollectExtensions() does something similar.
constexpr base::TimeDelta kRemoveUnusedThemesStartupDelay = base::Seconds(30);

bool g_dont_write_theme_pack_for_testing = false;

// Writes the theme pack to disk on a separate thread.
void WritePackToDiskCallback(BrowserThemePack* pack,
                             const base::FilePath& directory) {
  if (g_dont_write_theme_pack_for_testing)
    return;

  pack->WriteToDisk(directory.Append(chrome::kThemePackFilename));
}

}  // namespace


// ThemeService::ThemeObserver ------------------------------------------------

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ThemeService::ThemeObserver
    : public extensions::ExtensionRegistryObserver {
 public:
  explicit ThemeObserver(ThemeService* service) : theme_service_(service) {
    extension_registry_observation_.Observe(
        extensions::ExtensionRegistry::Get(theme_service_->profile_));
  }

  ThemeObserver(const ThemeObserver&) = delete;
  ThemeObserver& operator=(const ThemeObserver&) = delete;

  ~ThemeObserver() override {
  }

 private:
  // extensions::ExtensionRegistryObserver:
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const extensions::Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override {
    if (extension->is_theme()) {
      // Remember ID of the newly installed theme.
      theme_service_->installed_pending_load_id_ = extension->id();
    }
  }

  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override {
    if (!extension->is_theme() || theme_service_->UsingPolicyTheme())
      return;

    bool is_new_version =
        theme_service_->installed_pending_load_id_ !=
            ThemeHelper::kDefaultThemeID &&
        theme_service_->installed_pending_load_id_ == extension->id();
    theme_service_->installed_pending_load_id_ = ThemeHelper::kDefaultThemeID;

    // Do not load already loaded theme.
    if (!is_new_version && extension->id() == theme_service_->GetThemeID())
      return;

    // Set the new theme during extension load:
    // This includes: a) installing a new theme, b) enabling a disabled theme.
    // We shouldn't get here for the update of a disabled theme.
    theme_service_->DoSetTheme(extension, !is_new_version);
  }

  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionReason reason) override {
    if (reason != extensions::UnloadedExtensionReason::UPDATE &&
        reason != extensions::UnloadedExtensionReason::LOCK_ALL &&
        extension->is_theme() &&
        extension->id() == theme_service_->GetThemeID()) {
      theme_service_->UseDefaultTheme();
    }
  }

  raw_ptr<ThemeService> theme_service_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};
};
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// ThemeService::ThemeReinstaller -----------------------------------------

ThemeService::ThemeReinstaller::ThemeReinstaller(Profile* profile,
                                                 base::OnceClosure installer)
    : theme_service_(ThemeServiceFactory::GetForProfile(profile)) {
  theme_service_->number_of_reinstallers_++;
  installer_ = std::move(installer);
}

ThemeService::ThemeReinstaller::~ThemeReinstaller() {
  theme_service_->number_of_reinstallers_--;
  theme_service_->RemoveUnusedThemes();
}

void ThemeService::ThemeReinstaller::Reinstall() {
  if (!installer_.is_null()) {
    std::move(installer_).Run();
  }
}

// ThemeService::BrowserThemeProvider ------------------------------------------

ThemeService::BrowserThemeProvider::BrowserThemeProvider(
    const ThemeHelper& theme_helper,
    bool incognito,
    const BrowserThemeProviderDelegate* delegate)
    : theme_helper_(theme_helper), incognito_(incognito), delegate_(delegate) {
  DCHECK(delegate_);
}

ThemeService::BrowserThemeProvider::~BrowserThemeProvider() = default;

gfx::ImageSkia* ThemeService::BrowserThemeProvider::GetImageSkiaNamed(
    int id) const {
  return theme_helper_->GetImageSkiaNamed(id, incognito_, GetThemeSupplier());
}

color_utils::HSL ThemeService::BrowserThemeProvider::GetTint(int id) const {
  return theme_helper_->GetTint(id, incognito_, GetThemeSupplier());
}

int ThemeService::BrowserThemeProvider::GetDisplayProperty(int id) const {
  return theme_helper_->GetDisplayProperty(id, GetThemeSupplier());
}

bool ThemeService::BrowserThemeProvider::ShouldUseNativeFrame() const {
  return theme_helper_->ShouldUseNativeFrame(GetThemeSupplier());
}

bool ThemeService::BrowserThemeProvider::HasCustomImage(int id) const {
  return theme_helper_->HasCustomImage(id, GetThemeSupplier());
}

base::RefCountedMemory* ThemeService::BrowserThemeProvider::GetRawData(
    int id,
    ui::ResourceScaleFactor scale_factor) const {
  return theme_helper_->GetRawData(id, GetThemeSupplier(), scale_factor);
}

CustomThemeSupplier* ThemeService::BrowserThemeProvider::GetThemeSupplier()
    const {
  return incognito_ ? nullptr : delegate_->GetThemeSupplier();
}

// ThemeService ---------------------------------------------------------------

const char ThemeService::kAutogeneratedThemeID[] = "autogenerated_theme_id";
const char ThemeService::kUserColorThemeID[] = "user_color_theme_id";

// static
std::unique_ptr<ui::ThemeProvider> ThemeService::CreateBoundThemeProvider(
    Profile* profile,
    BrowserThemeProviderDelegate* delegate) {
  return std::make_unique<BrowserThemeProvider>(
      *ThemeServiceFactory::GetForProfile(profile)->theme_helper_, false,
      delegate);
}

ThemeService::ThemeService(Profile* profile, const ThemeHelper& theme_helper)
    : profile_(profile),
      theme_helper_(theme_helper),
      original_theme_provider_(*theme_helper_, false, this),
      incognito_theme_provider_(*theme_helper_, true, this) {}

ThemeService::~ThemeService() = default;

void ThemeService::Init() {
  theme_helper_->DCheckCalledOnValidSequence();

  InitFromPrefs();

  // ThemeObserver should be constructed before calling
  // OnExtensionServiceReady. Otherwise, the ThemeObserver won't be
  // constructed in time to observe the corresponding events.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  theme_observer_ = std::make_unique<ThemeObserver>(this);

  extensions::ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE, base::BindOnce(&ThemeService::OnExtensionServiceReady,
                                weak_ptr_factory_.GetWeakPtr()));
#endif
  theme_syncable_service_ =
      std::make_unique<ThemeSyncableService>(profile_, this);

  // TODO(gayane): Temporary entry point for Chrome Colors. Remove once UI is
  // there.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInstallAutogeneratedTheme)) {
    std::string value =
        command_line->GetSwitchValueASCII(switches::kInstallAutogeneratedTheme);
    std::vector<std::string> rgb = base::SplitString(
        value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (rgb.size() != 3)
      return;
    int r, g, b;
    base::StringToInt(rgb[0], &r);
    base::StringToInt(rgb[1], &g);
    base::StringToInt(rgb[2], &b);
    BuildAutogeneratedThemeFromColor(SkColorSetRGB(r, g, b));
  }

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kPolicyThemeColor,
      base::BindRepeating(&ThemeService::HandlePolicyColorUpdate,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      GetThemePrefNameInMigration(ThemePrefInMigration::kBrowserColorScheme),
      base::BindRepeating(&ThemeService::NotifyThemeChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      GetThemePrefNameInMigration(ThemePrefInMigration::kBrowserColorVariant),
      base::BindRepeating(&ThemeService::NotifyThemeChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      GetThemePrefNameInMigration(ThemePrefInMigration::kGrayscaleThemeEnabled),
      base::BindRepeating(&ThemeService::NotifyThemeChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      GetThemePrefNameInMigration(ThemePrefInMigration::kUserColor),
      base::BindRepeating(&ThemeService::NotifyThemeChanged,
                          base::Unretained(this)));
}

void ThemeService::Shutdown() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  theme_observer_.reset();
#endif
  theme_syncable_service_.reset();
}

CustomThemeSupplier* ThemeService::GetThemeSupplier() const {
  return theme_supplier_.get();
}

bool ThemeService::ShouldUseCustomFrame() const {
#if BUILDFLAG(IS_LINUX)
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformRuntimeProperties()
           .supports_server_side_window_decorations) {
    return true;
  }

  return profile_->GetPrefs()->GetBoolean(prefs::kUseCustomChromeFrame);
#else
  return true;
#endif
}

void ThemeService::SetTheme(const extensions::Extension* extension) {
  DoSetTheme(extension, true);
}

void ThemeService::RevertToExtensionTheme(const std::string& extension_id) {
  const auto* extension = extensions::ExtensionRegistry::Get(profile_)
                              ->disabled_extensions()
                              .GetByID(extension_id);
  if (extension && extension->is_theme()) {
    extensions::ExtensionService* service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    DCHECK(!service->IsExtensionEnabled(extension->id()));
    // |extension| is disabled when reverting to the previous theme via an
    // infobar.
    service->EnableExtension(extension->id());
    // Enabling the extension will call back to SetTheme().
  }
}

void ThemeService::UseTheme(ui::SystemTheme system_theme) {
  UseDefaultTheme();
}

void ThemeService::UseDefaultTheme() {
  if (UsingPolicyTheme()) {
    DVLOG(1)
        << "Default theme was not applied because a policy theme has been set.";
    return;
  }

  if (ready_)
    base::RecordAction(base::UserMetricsAction("Themes_Reset"));

  ClearThemeData(/*clear_ntp_background=*/true);
  NotifyThemeChanged();
}

void ThemeService::UseSystemTheme() {
  UseDefaultTheme();
}

void ThemeService::UseDeviceTheme(bool follow) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  // This toggle is currently supported on ChromeOS and Windows and we only want
  // platforms to set the value if they have a visible toggle.
  profile_->GetPrefs()->SetBoolean(prefs::kBrowserFollowsSystemThemeColors,
                                   follow);
  NotifyThemeChanged();
#endif
}

bool ThemeService::UsingDeviceTheme() const {
#if BUILDFLAG(IS_CHROMEOS)
  const PrefService::Preference* pref = profile_->GetPrefs()->FindPreference(
      prefs::kBrowserFollowsSystemThemeColors);
  // Ensure we respect previous theme settings for an unset follow theme
  // value.
  if (pref->IsDefaultValue()) {
    return GetIsBaseline() && !UsingExtensionTheme();
  }
  return pref->GetValue()->GetBool();
#elif BUILDFLAG(IS_WIN)
  // Always respect the profile preference on Windows. In the default case the
  // preference starts disabled.
  return profile_->GetPrefs()
      ->FindPreference(prefs::kBrowserFollowsSystemThemeColors)
      ->GetValue()
      ->GetBool();
#else
  // Only ChromeOS and Windows have this toggle.
  return false;
#endif
}

bool ThemeService::IsSystemThemeDistinctFromDefaultTheme() const {
  return false;
}

bool ThemeService::UsingDefaultTheme() const {
  return ThemeHelper::IsDefaultTheme(GetThemeSupplier());
}

bool ThemeService::UsingSystemTheme() const {
  return UsingDefaultTheme();
}

bool ThemeService::UsingExtensionTheme() const {
  return ThemeHelper::IsExtensionTheme(GetThemeSupplier());
}

bool ThemeService::UsingAutogeneratedTheme() const {
  return ThemeHelper::IsAutogeneratedTheme(GetThemeSupplier());
}

std::string ThemeService::GetThemeID() const {
  return profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);
}

bool ThemeService::UsingPolicyTheme() const {
  return profile_->GetPrefs()->IsManagedPreference(prefs::kPolicyThemeColor);
}

void ThemeService::RemoveUnusedThemes() {
  // We do not want to garbage collect themes on startup (|ready_| is false).
  // Themes will get garbage collected after |kRemoveUnusedThemesStartupDelay|.
  if (!profile_ || !ready_)
    return;
  if (number_of_reinstallers_ != 0 || !building_extension_id_.empty()) {
    return;
  }

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!service)
    return;

  std::string current_theme = GetThemeID();
  std::vector<std::string> remove_list;
  const extensions::ExtensionSet extensions =
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet();
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  for (const auto& extension : extensions) {
    if (extension->is_theme() && extension->id() != current_theme) {
      // Only uninstall themes which are not disabled or are disabled with
      // reason DISABLE_USER_ACTION. We cannot blanket uninstall all disabled
      // themes because externally installed themes are initially disabled.
      int disable_reason = prefs->GetDisableReasons(extension->id());
      if (!prefs->IsExtensionDisabled(extension->id()) ||
          disable_reason == extensions::disable_reason::DISABLE_USER_ACTION) {
        remove_list.push_back(extension->id());
      }
    }
  }
  // TODO: Garbage collect all unused themes. This method misses themes which
  // are installed but not loaded because they are blocked by a management
  // policy provider.

  for (size_t i = 0; i < remove_list.size(); ++i) {
    service->UninstallExtension(
        remove_list[i], extensions::UNINSTALL_REASON_ORPHANED_THEME, nullptr);
  }
}

ThemeSyncableService* ThemeService::GetThemeSyncableService() const {
  return theme_syncable_service_.get();
}

// static
const ui::ThemeProvider& ThemeService::GetThemeProviderForProfile(
    Profile* profile) {
  ThemeService* service = ThemeServiceFactory::GetForProfile(profile);
  return profile->IsIncognitoProfile() ? service->incognito_theme_provider_
                                       : service->original_theme_provider_;
}

// static
CustomThemeSupplier* ThemeService::GetThemeSupplierForProfile(
    Profile* profile) {
  return ThemeServiceFactory::GetForProfile(profile)->GetThemeSupplier();
}

ui::ColorProvider* ThemeService::GetColorProvider() {
  // Device theme overrides custom themes.
  auto* theme_supplier = UsingDeviceTheme() ? nullptr : GetThemeSupplier();
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(
          theme_supplier));
}

void ThemeService::BuildAutogeneratedThemeFromColor(SkColor color) {
  if (UsingPolicyTheme()) {
    DVLOG(1) << "Autogenerated theme was not applied because a policy theme"
                " has been set.";
    return;
  }
  BuildAutogeneratedThemeFromColor(color, /*store_user_prefs*/ true);
}

void ThemeService::BuildAutogeneratedThemeFromColor(SkColor color,
                                                    bool store_in_prefs) {
  std::optional<std::string> previous_theme_id;
  if (UsingExtensionTheme())
    previous_theme_id = GetThemeID();

  auto pack = base::MakeRefCounted<BrowserThemePack>(
      ui::ColorProviderKey::ThemeInitializerSupplier::ThemeType::
          kAutogenerated);
  BrowserThemePack::BuildFromColor(color, pack.get());
  SwapThemeSupplier(std::move(pack));
  if (theme_supplier_) {
    if (store_in_prefs) {
      SetThemePrefsForColor(color);
      // Only disable previous extension theme if new theme is saved to prefs,
      // otherwise there may be issues (ex. when unsetting managed theme).
      if (previous_theme_id.has_value())
        DisableExtension(previous_theme_id.value());
    }
    NotifyThemeChanged();
  }
}

SkColor ThemeService::GetAutogeneratedThemeColor() const {
  return profile_->GetPrefs()->GetInteger(prefs::kAutogeneratedThemeColor);
}

void ThemeService::BuildAutogeneratedPolicyTheme() {
  BuildAutogeneratedThemeFromColor(GetPolicyThemeColor(),
                                   /*store_user_prefs*/ false);
}

SkColor ThemeService::GetPolicyThemeColor() const {
  DCHECK(UsingPolicyTheme());
  return profile_->GetPrefs()->GetInteger(prefs::kPolicyThemeColor);
}

void ThemeService::SetBrowserColorScheme(
    ThemeService::BrowserColorScheme color_scheme) {
  {
    base::AutoReset<bool> resetter(&should_suppress_theme_updates_, true);
    profile_->GetPrefs()->SetInteger(
        GetThemePrefNameInMigration(ThemePrefInMigration::kBrowserColorScheme),
        static_cast<int>(color_scheme));
  }
  NotifyThemeChanged();
}

ThemeService::BrowserColorScheme ThemeService::GetBrowserColorScheme() const {
  return static_cast<BrowserColorScheme>(profile_->GetPrefs()->GetInteger(
      GetThemePrefNameInMigration(ThemePrefInMigration::kBrowserColorScheme)));
}

void ThemeService::SetUserColor(std::optional<SkColor> user_color) {
  {
    base::AutoReset<bool> resetter(&should_suppress_theme_updates_, true);
    ClearThemeData(/*clear_ntp_background=*/false);
    profile_->GetPrefs()->SetInteger(
        GetThemePrefNameInMigration(ThemePrefInMigration::kUserColor),
        user_color.value_or(SK_ColorTRANSPARENT));
    profile_->GetPrefs()->SetString(prefs::kCurrentThemeID, kUserColorThemeID);
  }
  NotifyThemeChanged();
}

std::optional<SkColor> ThemeService::GetUserColor() const {
  return CurrentThemeUserColor(profile_->GetPrefs());
}

void ThemeService::SetBrowserColorVariant(
    ui::mojom::BrowserColorVariant color_variant) {
  {
    base::AutoReset<bool> resetter(&should_suppress_theme_updates_, true);
    profile_->GetPrefs()->SetInteger(
        GetThemePrefNameInMigration(ThemePrefInMigration::kBrowserColorVariant),
        static_cast<int>(color_variant));
  }
  NotifyThemeChanged();
}

ui::mojom::BrowserColorVariant ThemeService::GetBrowserColorVariant() const {
  return static_cast<ui::mojom::BrowserColorVariant>(
      profile_->GetPrefs()->GetInteger(GetThemePrefNameInMigration(
          ThemePrefInMigration::kBrowserColorVariant)));
}

void ThemeService::SetUserColorAndBrowserColorVariant(
    SkColor user_color,
    ui::mojom::BrowserColorVariant color_variant) {
  {
    base::AutoReset<bool> resetter(&should_suppress_theme_updates_, true);
    ClearThemeData(/*clear_ntp_background=*/false);
    profile_->GetPrefs()->SetInteger(
        GetThemePrefNameInMigration(ThemePrefInMigration::kUserColor),
        user_color);
    profile_->GetPrefs()->SetString(prefs::kCurrentThemeID, kUserColorThemeID);
    profile_->GetPrefs()->SetInteger(
        GetThemePrefNameInMigration(ThemePrefInMigration::kBrowserColorVariant),
        static_cast<int>(color_variant));
  }
  NotifyThemeChanged();
}

void ThemeService::SetIsGrayscale(bool is_grayscale) {
  {
    base::AutoReset<bool> resetter(&should_suppress_theme_updates_, true);
    ClearThemeData(/*clear_ntp_background=*/false);
    profile_->GetPrefs()->SetBoolean(
        GetThemePrefNameInMigration(
            ThemePrefInMigration::kGrayscaleThemeEnabled),
        is_grayscale);
  }
  NotifyThemeChanged();
}

bool ThemeService::GetIsGrayscale() const {
  return CurrentThemeIsGrayscale(profile_->GetPrefs());
}

bool ThemeService::GetIsBaseline() const {
  // Baseline is defined by the absence of a user color set by the corresponding
  // profile pref or the autogenerated theme.
  return !GetUserColor().has_value() && !UsingAutogeneratedTheme();
}

// static
void ThemeService::DisableThemePackForTesting() {
  g_dont_write_theme_pack_for_testing = true;
}

std::unique_ptr<ThemeService::ThemeReinstaller>
ThemeService::BuildReinstallerForCurrentTheme() {
  std::string current_id = GetThemeID();
  if (UsingExtensionTheme()) {
    return std::make_unique<ThemeReinstaller>(
        profile_, base::BindOnce(&ThemeService::RevertToExtensionTheme,
                                 weak_ptr_factory_.GetWeakPtr(), current_id));
  }
  // TODO(crbug.com/352471699): Also restore the NTP background image.
  if (UsingAutogeneratedTheme()) {
    return std::make_unique<ThemeReinstaller>(
        profile_,
        base::BindOnce(static_cast<void (ThemeService::*)(SkColor)>(
                           &ThemeService::BuildAutogeneratedThemeFromColor),
                       weak_ptr_factory_.GetWeakPtr(),
                       GetAutogeneratedThemeColor()));
  }
  if (current_id == kUserColorThemeID) {
    if (const std::optional<SkColor> user_color = GetUserColor()) {
      return std::make_unique<ThemeReinstaller>(
          profile_,
          base::BindOnce(&ThemeService::SetUserColorAndBrowserColorVariant,
                         weak_ptr_factory_.GetWeakPtr(), user_color.value(),
                         GetBrowserColorVariant()));
    }
  }

  auto system_theme = ui::SystemTheme::kDefault;
  if (auto* theme_supplier = GetThemeSupplier()) {
    if (auto* native_theme = theme_supplier->GetNativeTheme()) {
      system_theme = native_theme->system_theme();
    }
  }
  return std::make_unique<ThemeReinstaller>(
      profile_, base::BindOnce(&ThemeService::UseTheme,
                               weak_ptr_factory_.GetWeakPtr(), system_theme));
}

void ThemeService::AddObserver(ThemeServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void ThemeService::RemoveObserver(ThemeServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ThemeService::SetCustomDefaultTheme(
    scoped_refptr<CustomThemeSupplier> theme_supplier) {
  if (UsingPolicyTheme()) {
    DVLOG(1) << "Custom default theme was not applied because a policy "
                "theme has been set.";
    return;
  }

  ClearThemeData(/*clear_ntp_background=*/true);
  SwapThemeSupplier(std::move(theme_supplier));
  NotifyThemeChanged();
}

ui::SystemTheme ThemeService::GetDefaultSystemTheme() const {
  return ui::SystemTheme::kDefault;
}

void ThemeService::ClearThemeData(bool clear_ntp_background) {
  if (!ready_)
    return;

  std::optional<std::string> previous_theme_id;
  if (UsingExtensionTheme())
    previous_theme_id = GetThemeID();

  SwapThemeSupplier(nullptr);
  ClearThemePrefs();
  if (clear_ntp_background) {
    // Redraw and notify sync that theme has changed.
    for (auto& observer : observers_) {
      observer.OnCustomNtpBackgroundObsolete();
    }
  }

  // Disable extension after modifying the prefs so that unloading the extension
  // doesn't trigger |ClearThemeData| again.
  if (previous_theme_id.has_value())
    DisableExtension(previous_theme_id.value());
}

void ThemeService::InitFromPrefs() {
  FixInconsistentPreferencesIfNeeded();
  absl::Cleanup set_ready_cleanup = [this] { this->set_ready(); };

  // If theme color policy was set while browser was off, apply it now.
  if (UsingPolicyTheme()) {
    BuildAutogeneratedPolicyTheme();
    return;
  }

  std::string current_id = GetThemeID();
  if (current_id == ThemeHelper::kDefaultThemeID) {
    if (GetIsGrayscale()) {
      chrome_colors::RecordDynamicColorOnLoadHistogramForGrayscale();
    }
    UseTheme(GetDefaultSystemTheme());
    return;
  }

  if (current_id == kAutogeneratedThemeID) {
    SkColor color = GetAutogeneratedThemeColor();
    BuildAutogeneratedThemeFromColor(color);
    chrome_colors::RecordColorOnLoadHistogram(color);
    return;
  }

  if (current_id == kUserColorThemeID) {
    const auto user_color = GetUserColor();
    if (user_color.has_value()) {
      chrome_colors::RecordDynamicColorOnLoadHistogram(
          *user_color, GetBrowserColorVariant());
    }
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  base::FilePath path = prefs->GetFilePath(prefs::kCurrentThemePackFilename);
  // If we don't have a file pack, we're updating from an old version.
  if (!path.empty()) {
    path = path.Append(chrome::kThemePackFilename);
    SwapThemeSupplier(BrowserThemePack::BuildFromDataPack(path, current_id));
    if (theme_supplier_) {
      base::RecordAction(base::UserMetricsAction("Themes.Loaded"));
      return;
    }
  }
  // Else: wait for the extension service to be ready so that the theme pack
  // can be recreated from the extension.
  std::move(set_ready_cleanup).Cancel();
}

void ThemeService::NotifyThemeChanged() {
  if (!ready_ || should_suppress_theme_updates_) {
    return;
  }

  // Redraw and notify sync that theme has changed.
  for (auto& observer : observers_)
    observer.OnThemeChanged();
}

void ThemeService::FixInconsistentPreferencesIfNeeded() {}

void ThemeService::DoSetTheme(const extensions::Extension* extension,
                              bool suppress_infobar) {
  DCHECK(extension->is_theme());
  DCHECK(!UsingPolicyTheme());
  DCHECK(extensions::ExtensionSystem::Get(profile_)
             ->extension_service()
             ->IsExtensionEnabled(extension->id()));
  BuildFromExtension(extension, suppress_infobar);
}

void ThemeService::OnExtensionServiceReady() {
  if (!ready_) {
    // If the ThemeService is not ready yet, the custom theme data pack needs to
    // be recreated from the extension.
    MigrateTheme();
    set_ready();
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThemeService::RemoveUnusedThemes,
                     weak_ptr_factory_.GetWeakPtr()),
      kRemoveUnusedThemesStartupDelay);
}

void ThemeService::MigrateTheme() {
  TRACE_EVENT0("browser", "ThemeService::MigrateTheme");

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  const extensions::Extension* extension =
      registry ? registry->enabled_extensions().GetByID(GetThemeID()) : nullptr;
  if (extension) {
    // Theme migration is done on the UI thread. Blocking the UI from appearing
    // until it's ready is deemed better than showing a blip of the default
    // theme.
    TRACE_EVENT0("browser", "ThemeService::MigrateTheme - BuildFromExtension");
    DLOG(ERROR) << "Migrating theme";

    scoped_refptr<BrowserThemePack> pack(new BrowserThemePack(
        ui::ColorProviderKey::ThemeInitializerSupplier::ThemeType::kExtension));
    BrowserThemePack::BuildFromExtension(extension, pack.get());
    OnThemeBuiltFromExtension(extension->id(), pack.get(), true);
    base::RecordAction(base::UserMetricsAction("Themes.Migrated"));
  } else {
    DLOG(ERROR) << "Theme is mysteriously gone.";
    ClearThemeData(/*clear_ntp_background=*/true);
    base::RecordAction(base::UserMetricsAction("Themes.Gone"));
  }
}

void ThemeService::SwapThemeSupplier(
    scoped_refptr<CustomThemeSupplier> theme_supplier) {
  if (theme_supplier_)
    theme_supplier_->StopUsingTheme();
  theme_supplier_ = theme_supplier;
  if (theme_supplier_)
    theme_supplier_->StartUsingTheme();
}

void ThemeService::BuildFromExtension(const extensions::Extension* extension,
                                      bool suppress_infobar) {
  build_extension_task_tracker_.TryCancelAll();
  building_extension_id_ = extension->id();
  scoped_refptr<BrowserThemePack> pack(new BrowserThemePack(
      ui::ColorProviderKey::ThemeInitializerSupplier::ThemeType::kExtension));
  auto task_runner = base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  build_extension_task_tracker_.PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&BrowserThemePack::BuildFromExtension,
                     base::RetainedRef(extension),
                     base::RetainedRef(pack.get())),
      base::BindOnce(&ThemeService::OnThemeBuiltFromExtension,
                     weak_ptr_factory_.GetWeakPtr(), extension->id(), pack,
                     suppress_infobar));
}

void ThemeService::OnThemeBuiltFromExtension(
    const extensions::ExtensionId& extension_id,
    scoped_refptr<BrowserThemePack> pack,
    bool suppress_infobar) {
  if (UsingPolicyTheme()) {
    DVLOG(1) << "Extension theme was not applied because a policy theme has "
                "been set.";
    return;
  }

  if (!pack->is_valid()) {
    // TODO(erg): We've failed to install the theme; perhaps we should tell the
    // user? http://crbug.com/34780
    LOG(ERROR) << "Could not load theme.";
    return;
  }

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!service)
    return;
  const auto* extension = extensions::ExtensionRegistry::Get(profile_)
                              ->enabled_extensions()
                              .GetByID(extension_id);
  if (!extension)
    return;

  // Schedule the writing of the packed file to disk.
  extensions::GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&WritePackToDiskCallback,
                                base::RetainedRef(pack), extension->path()));
  std::unique_ptr<ThemeService::ThemeReinstaller> reinstaller =
      BuildReinstallerForCurrentTheme();
  std::optional<std::string> previous_theme_id;
  if (UsingExtensionTheme())
    previous_theme_id = GetThemeID();

  SwapThemeSupplier(std::move(pack));
  SetThemePrefsForExtension(extension);
  NotifyThemeChanged();
  building_extension_id_.clear();

  // Same old theme, but the theme has changed (migrated) or auto-updated.
  if (previous_theme_id.has_value() &&
      previous_theme_id.value() == extension->id()) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("Themes_Installed"));

  bool can_revert_theme = true;
  if (previous_theme_id.has_value())
    can_revert_theme = DisableExtension(previous_theme_id.value());

  // Offer to revert to the old theme.
  if (can_revert_theme && !suppress_infobar && extension->is_theme()) {
    ThemeInstalledInfoBarDelegate::CreateForLastActiveTab(
        profile_, extension->name(), extension->id(), std::move(reinstaller));
  }
}

void ThemeService::HandlePolicyColorUpdate() {
  if (UsingPolicyTheme()) {
    BuildAutogeneratedPolicyTheme();
  } else {
    // If a policy theme is unset, load the previous theme from prefs.
    InitFromPrefs();

    // NotifyThemeChanged() isn't triggered in InitFromPrefs() for extension
    // themes, so it's called here to make sure the browser's theme is updated.
    if (UsingExtensionTheme())
      NotifyThemeChanged();
  }
}

void ThemeService::ClearThemePrefs() {
  profile_->GetPrefs()->ClearPref(prefs::kCurrentThemePackFilename);
  profile_->GetPrefs()->ClearPref(prefs::kAutogeneratedThemeColor);
  profile_->GetPrefs()->ClearPref(
      GetThemePrefNameInMigration(ThemePrefInMigration::kUserColor));
  profile_->GetPrefs()->ClearPref(GetThemePrefNameInMigration(
      ThemePrefInMigration::kGrayscaleThemeEnabled));
  profile_->GetPrefs()->ClearPref(
      GetThemePrefNameInMigration(ThemePrefInMigration::kBrowserColorVariant));
  profile_->GetPrefs()->SetString(prefs::kCurrentThemeID,
                                  ThemeHelper::kDefaultThemeID);
}

void ThemeService::SetThemePrefsForExtension(
    const extensions::Extension* extension) {
  ClearThemePrefs();
  // Redraw and notify sync that theme has changed.
  for (auto& observer : observers_) {
    observer.OnCustomNtpBackgroundObsolete();
  }
  // Extensions are incompatible with device themes so turn them off.
  // TODO(crbug.com/40280173): Remove this if we can otherwise separate
  // extension and device themes from attempting to apply at the same time.
  profile_->GetPrefs()->SetBoolean(prefs::kBrowserFollowsSystemThemeColors,
                                   false);

  profile_->GetPrefs()->SetString(prefs::kCurrentThemeID, extension->id());

  // Save only the extension path. The packed file will be loaded via
  // InitFromPrefs().
  profile_->GetPrefs()->SetFilePath(prefs::kCurrentThemePackFilename,
                                    extension->path());
}

void ThemeService::SetThemePrefsForColor(SkColor color) {
  ClearThemePrefs();
  profile_->GetPrefs()->SetInteger(prefs::kAutogeneratedThemeColor, color);
  profile_->GetPrefs()->SetString(prefs::kCurrentThemeID,
                                  kAutogeneratedThemeID);
}

bool ThemeService::DisableExtension(const std::string& extension_id) {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!service)
    return false;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);

  if (registry->GetInstalledExtension(extension_id)) {
    // Do not disable the previous theme if it is already uninstalled. Sending
    // |ThemeServiceObserver::OnThemeChanged()| causes the previous theme to be
    // uninstalled when the notification causes the remaining infobar to close
    // and does not open any new infobars. See crbug.com/468280.
    service->DisableExtension(extension_id,
                              extensions::disable_reason::DISABLE_USER_ACTION);
    return true;
  }
  return false;
}
