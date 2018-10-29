// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/user_metrics.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/increased_contrast_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/scoped_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_theme.h"
#endif

using base::UserMetricsAction;
using content::BrowserThread;
using extensions::Extension;
using ui::ResourceBundle;


// Helpers --------------------------------------------------------------------

namespace {

// The default theme if we've gone to the theme gallery and installed the
// "Default" theme. We have to detect this case specifically. (By the time we
// realize we've installed the default theme, we already have an extension
// unpacked on the filesystem.)
const char kDefaultThemeGalleryID[] = "hkacjpbfdknhflllbcmjibkdeoafencn";

// Wait this many seconds after startup to garbage collect unused themes.
// Removing unused themes is done after a delay because there is no
// reason to do it at startup.
// ExtensionService::GarbageCollectExtensions() does something similar.
const int kRemoveUnusedThemesStartupDelay = 30;

SkColor IncreaseLightness(SkColor color, double percent) {
  color_utils::HSL result;
  color_utils::SkColorToHSL(color, &result);
  result.l += (1 - result.l) * percent;
  return color_utils::HSLToSkColor(result, SkColorGetA(color));
}

// Writes the theme pack to disk on a separate thread.
void WritePackToDiskCallback(BrowserThemePack* pack,
                             const base::FilePath& directory) {
  pack->WriteToDisk(directory.Append(chrome::kThemePackFilename));
}

// For legacy reasons, the theme supplier requires the incognito variants of
// color IDs.  This converts from normal to incognito IDs where they exist.
int GetIncognitoId(int id) {
  switch (id) {
    case ThemeProperties::COLOR_FRAME:
      return ThemeProperties::COLOR_FRAME_INCOGNITO;
    case ThemeProperties::COLOR_FRAME_INACTIVE:
      return ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE;
    case ThemeProperties::COLOR_BACKGROUND_TAB:
      return ThemeProperties::COLOR_BACKGROUND_TAB_INCOGNITO;
    case ThemeProperties::COLOR_BACKGROUND_TAB_INACTIVE:
      return ThemeProperties::COLOR_BACKGROUND_TAB_INCOGNITO_INACTIVE;
    case ThemeProperties::COLOR_BACKGROUND_TAB_TEXT:
      return ThemeProperties::COLOR_BACKGROUND_TAB_TEXT_INCOGNITO;
    case ThemeProperties::COLOR_BACKGROUND_TAB_TEXT_INACTIVE:
      return ThemeProperties::COLOR_BACKGROUND_TAB_TEXT_INCOGNITO_INACTIVE;
    default:
      return id;
  }
}

// Heuristic to determine if color is grayscale. This is used to decide whether
// to use the colorful or white logo, if a theme fails to specify which.
bool IsColorGrayscale(SkColor color) {
  const int kChannelTolerance = 9;
  int r = SkColorGetR(color);
  int g = SkColorGetG(color);
  int b = SkColorGetB(color);
  int range = std::max(r, std::max(g, b)) - std::min(r, std::min(g, b));
  return range < kChannelTolerance;
}

}  // namespace


// ThemeService::BrowserThemeProvider -----------------------------------------

// Creates a temporary scope where all |theme_service_| property getters return
// uncustomized default values if |theme_provider_.use_default_| is enabled.
class ThemeService::BrowserThemeProvider::DefaultScope {
 public:
  explicit DefaultScope(const BrowserThemeProvider& theme_provider)
      : theme_provider_(theme_provider) {
    if (theme_provider_.use_default_) {
      // Mutations to |theme_provider_| are undone in the destructor making it
      // effectively const over the entire duration of this object's scope.
      theme_supplier_ =
          std::move(const_cast<ThemeService&>(theme_provider_.theme_service_)
                        .theme_supplier_);
      DCHECK(!theme_provider_.theme_service_.theme_supplier_);
    }
  }

  ~DefaultScope() {
    if (theme_provider_.use_default_) {
      const_cast<ThemeService&>(theme_provider_.theme_service_)
          .theme_supplier_ = std::move(theme_supplier_);
    }
    DCHECK(!theme_supplier_);
  }

 private:
  const BrowserThemeProvider& theme_provider_;
  scoped_refptr<CustomThemeSupplier> theme_supplier_;

  DISALLOW_COPY_AND_ASSIGN(DefaultScope);
};

ThemeService::BrowserThemeProvider::BrowserThemeProvider(
    const ThemeService& theme_service,
    bool incognito,
    bool use_default)
    : theme_service_(theme_service),
      incognito_(incognito),
      use_default_(use_default) {}

ThemeService::BrowserThemeProvider::~BrowserThemeProvider() {}

gfx::ImageSkia* ThemeService::BrowserThemeProvider::GetImageSkiaNamed(
    int id) const {
  DefaultScope scope(*this);
  return theme_service_.GetImageSkiaNamed(id, incognito_);
}

SkColor ThemeService::BrowserThemeProvider::GetColor(int id) const {
  DefaultScope scope(*this);
  return theme_service_.GetColor(id, incognito_);
}

color_utils::HSL ThemeService::BrowserThemeProvider::GetTint(int id) const {
  DefaultScope scope(*this);
  return theme_service_.GetTint(id, incognito_);
}

int ThemeService::BrowserThemeProvider::GetDisplayProperty(int id) const {
  DefaultScope scope(*this);
  return theme_service_.GetDisplayProperty(id);
}

bool ThemeService::BrowserThemeProvider::ShouldUseNativeFrame() const {
  DefaultScope scope(*this);
  return theme_service_.ShouldUseNativeFrame();
}

bool ThemeService::BrowserThemeProvider::HasCustomImage(int id) const {
  DefaultScope scope(*this);
  return theme_service_.HasCustomImage(id);
}

bool ThemeService::BrowserThemeProvider::HasCustomColor(int id) const {
  DefaultScope scope(*this);
  bool has_custom_color = false;
  theme_service_.GetColor(id, incognito_, &has_custom_color);
  return has_custom_color;
}

base::RefCountedMemory* ThemeService::BrowserThemeProvider::GetRawData(
    int id,
    ui::ScaleFactor scale_factor) const {
  DefaultScope scope(*this);
  return theme_service_.GetRawData(id, scale_factor);
}


// ThemeService::ThemeObserver ------------------------------------------------

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ThemeService::ThemeObserver
    : public extensions::ExtensionRegistryObserver {
 public:
  explicit ThemeObserver(ThemeService* service)
      : theme_service_(service), extension_registry_observer_(this) {
    extension_registry_observer_.Add(
        extensions::ExtensionRegistry::Get(theme_service_->profile_));
  }

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
    if (!extension->is_theme())
      return;

    bool is_new_version =
        theme_service_->installed_pending_load_id_ != kDefaultThemeID &&
        theme_service_->installed_pending_load_id_ == extension->id();
    theme_service_->installed_pending_load_id_ = kDefaultThemeID;

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

  ThemeService* theme_service_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ThemeObserver);
};
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)


// ThemeService ---------------------------------------------------------------

// The default theme if we haven't installed a theme yet or if we've clicked
// the "Use Classic" button.
const char ThemeService::kDefaultThemeID[] = "";

ThemeService::ThemeService()
    : ready_(false),
      rb_(ui::ResourceBundle::GetSharedInstance()),
      profile_(nullptr),
      installed_pending_load_id_(kDefaultThemeID),
      number_of_infobars_(0),
      original_theme_provider_(*this, false, false),
      incognito_theme_provider_(*this, true, false),
      default_theme_provider_(*this, false, true),
      weak_ptr_factory_(this) {}

ThemeService::~ThemeService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ThemeService::Init(Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  profile_ = profile;

  LoadThemePrefs();

  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
                 content::Source<Profile>(profile_));

  theme_syncable_service_.reset(new ThemeSyncableService(profile_, this));
}

void ThemeService::Shutdown() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  theme_observer_.reset();
#endif
}

void ThemeService::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  using content::Details;
  switch (type) {
    case extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED:
      registrar_.Remove(this,
                        extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
                        content::Source<Profile>(profile_));
      OnExtensionServiceReady();
      break;
    default:
      NOTREACHED();
  }
}

void ThemeService::SetTheme(const Extension* extension) {
  DoSetTheme(extension, true);
}

void ThemeService::RevertToTheme(const Extension* extension) {
  DCHECK(extension->is_theme());
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(!service->IsExtensionEnabled(extension->id()));
  // |extension| is disabled when reverting to the previous theme via an
  // infobar.
  service->EnableExtension(extension->id());
  // Enabling the extension will call back to SetTheme().
}

void ThemeService::UseDefaultTheme() {
  if (ready_)
    base::RecordAction(UserMetricsAction("Themes_Reset"));
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (IsSupervisedUser()) {
    SetSupervisedUserTheme();
    return;
  }
#endif
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  if (native_theme && native_theme->UsesHighContrastColors())
    SetCustomDefaultTheme(new IncreasedContrastThemeSupplier);
  ClearAllThemeData();
  NotifyThemeChanged();
}

void ThemeService::UseSystemTheme() {
  UseDefaultTheme();
}

bool ThemeService::IsSystemThemeDistinctFromDefaultTheme() const {
  return false;
}

bool ThemeService::UsingDefaultTheme() const {
  std::string id = GetThemeID();
  return id == ThemeService::kDefaultThemeID ||
      id == kDefaultThemeGalleryID;
}

bool ThemeService::UsingSystemTheme() const {
  return UsingDefaultTheme();
}

std::string ThemeService::GetThemeID() const {
  return profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);
}

void ThemeService::OnInfobarDisplayed() {
  number_of_infobars_++;
}

void ThemeService::OnInfobarDestroyed() {
  number_of_infobars_--;

  if (number_of_infobars_ == 0 &&
      !build_extension_task_tracker_.HasTrackedTasks()) {
    RemoveUnusedThemes(false);
  }
}

void ThemeService::RemoveUnusedThemes(bool ignore_infobars) {
  // We do not want to garbage collect themes on startup (|ready_| is false).
  // Themes will get garbage collected after |kRemoveUnusedThemesStartupDelay|.
  if (!profile_ || !ready_)
    return;
  if (!ignore_infobars && number_of_infobars_ != 0)
    return;

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!service)
    return;

  std::string current_theme = GetThemeID();
  std::vector<std::string> remove_list;
  std::unique_ptr<const extensions::ExtensionSet> extensions(
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet());
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  for (extensions::ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const extensions::Extension* extension = it->get();
    if (extension->is_theme() && extension->id() != current_theme &&
        extension->id() != building_extension_id_) {
      // Only uninstall themes which are not disabled or are disabled with
      // reason DISABLE_USER_ACTION. We cannot blanket uninstall all disabled
      // themes because externally installed themes are initially disabled.
      int disable_reason = prefs->GetDisableReasons(extension->id());
      if (!prefs->IsExtensionDisabled(extension->id()) ||
          disable_reason == extensions::disable_reason::DISABLE_USER_ACTION) {
        remove_list.push_back((*it)->id());
      }
    }
  }
  // TODO: Garbage collect all unused themes. This method misses themes which
  // are installed but not loaded because they are blacklisted by a management
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
  bool incognito = profile->GetProfileType() == Profile::INCOGNITO_PROFILE;
  return incognito ? service->incognito_theme_provider_
                   : service->original_theme_provider_;
}

// static
const ui::ThemeProvider& ThemeService::GetDefaultThemeProviderForProfile(
    Profile* profile) {
  DCHECK_NE(profile->GetProfileType(), Profile::INCOGNITO_PROFILE)
      << "Incognito default theme access not implemented, add if needed.";
  return ThemeServiceFactory::GetForProfile(profile)->default_theme_provider_;
}

void ThemeService::SetCustomDefaultTheme(
    scoped_refptr<CustomThemeSupplier> theme_supplier) {
  ClearAllThemeData();
  SwapThemeSupplier(theme_supplier);
  NotifyThemeChanged();
}

bool ThemeService::ShouldInitWithSystemTheme() const {
  return false;
}

SkColor ThemeService::GetDefaultColor(int id, bool incognito) const {
  // For backward compat with older themes, some newer colors are generated from
  // older ones if they are missing.
  const int kNtpText = ThemeProperties::COLOR_NTP_TEXT;
  const int kLabelBackground =
      ThemeProperties::COLOR_SUPERVISED_USER_LABEL_BACKGROUND;
  switch (id) {
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON:
      return color_utils::HSLShift(
          gfx::kChromeIconGrey,
          GetTint(ThemeProperties::TINT_BUTTONS, incognito));
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE:
      // The active color is overridden in GtkUi.
      return SkColorSetA(
          GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON, incognito),
          0x6E);
    case ThemeProperties::COLOR_LOCATION_BAR_BORDER:
      return SkColorSetA(SK_ColorBLACK, 0x4D);
    case ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR:
    case ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_INACTIVE: {
      const SkColor tab_color =
          GetColor(ThemeProperties::COLOR_TOOLBAR, incognito);
      const int frame_id = (id == ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR)
                               ? ThemeProperties::COLOR_FRAME
                               : ThemeProperties::COLOR_FRAME_INACTIVE;
      const SkColor frame_color = GetColor(frame_id, incognito);
      const SeparatorColorKey key(tab_color, frame_color);
      auto i = separator_color_cache_.find(key);
      if (i != separator_color_cache_.end())
        return i->second;
      const SkColor separator_color = GetSeparatorColor(tab_color, frame_color);
      separator_color_cache_[key] = separator_color;
      return separator_color;
    }
    case ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR: {
      return SkColorSetA(
          GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON, incognito),
          0x4D);
    }
    case ThemeProperties::COLOR_BOOKMARK_BAR_INSTRUCTIONS_TEXT:
      if (UsingDefaultTheme())
        break;
      return GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT, incognito);
    case ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR:
      if (UsingDefaultTheme())
        break;
      return GetColor(ThemeProperties::COLOR_LOCATION_BAR_BORDER, incognito);
    case ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_BACKGROUND:
      if (UsingDefaultTheme())
        break;
      return GetColor(ThemeProperties::COLOR_TOOLBAR, incognito);
    case ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_SEPARATOR:
      // Use a faint version of the text color as the separator color.
      return SkColorSetA(
          GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT, incognito), 0x26);
    case ThemeProperties::COLOR_NTP_TEXT_LIGHT:
      return IncreaseLightness(GetColor(kNtpText, incognito), 0.40);
    case ThemeProperties::COLOR_TAB_THROBBER_SPINNING:
    case ThemeProperties::COLOR_TAB_THROBBER_WAITING: {
      SkColor base_color =
          ui::GetAuraColor(id == ThemeProperties::COLOR_TAB_THROBBER_SPINNING
                               ? ui::NativeTheme::kColorId_ThrobberSpinningColor
                               : ui::NativeTheme::kColorId_ThrobberWaitingColor,
                           ui::NativeTheme::GetInstanceForNativeUi());
      color_utils::HSL hsl = GetTint(ThemeProperties::TINT_BUTTONS, incognito);
      return color_utils::HSLShift(base_color, hsl);
    }
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    case ThemeProperties::COLOR_SUPERVISED_USER_LABEL:
      return color_utils::GetReadableColor(
          SK_ColorWHITE, GetColor(kLabelBackground, incognito));
    case ThemeProperties::COLOR_SUPERVISED_USER_LABEL_BACKGROUND:
      return color_utils::BlendTowardOppositeLuma(
          GetColor(ThemeProperties::COLOR_FRAME, incognito), 0x80);
    case ThemeProperties::COLOR_SUPERVISED_USER_LABEL_BORDER:
      return color_utils::AlphaBlend(GetColor(kLabelBackground, incognito),
                                     SK_ColorBLACK, 230);
#endif
  }

  // Always fall back to the non-incognito color when there's a custom theme
  // because the default (classic) incognito color may be dramatically different
  // (optimized for a light-on-dark color).
  return ThemeProperties::GetDefaultColor(id, incognito && !theme_supplier_);
}

color_utils::HSL ThemeService::GetTint(int id, bool incognito) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  color_utils::HSL hsl;
  if (theme_supplier_ && theme_supplier_->GetTint(id, &hsl))
    return hsl;

  // Always fall back to the non-incognito tint when there's a custom theme.
  // See comment in GetDefaultColor().
  return ThemeProperties::GetDefaultTint(id, incognito && !theme_supplier_);
}

void ThemeService::ClearAllThemeData() {
  if (!ready_)
    return;

  SwapThemeSupplier(nullptr);

  profile_->GetPrefs()->ClearPref(prefs::kCurrentThemePackFilename);
  SaveThemeID(kDefaultThemeID);

  // There should be no more infobars. This may not be the case because of
  // http://crbug.com/62154
  // RemoveUnusedThemes is called on a task because ClearAllThemeData() may
  // be called as a result of OnExtensionUnloaded().
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ThemeService::RemoveUnusedThemes,
                            weak_ptr_factory_.GetWeakPtr(), true));
}

void ThemeService::FixInconsistentPreferencesIfNeeded() {}

void ThemeService::LoadThemePrefs() {
  FixInconsistentPreferencesIfNeeded();
  PrefService* prefs = profile_->GetPrefs();

  std::string current_id = GetThemeID();
  if (current_id == kDefaultThemeID) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    // Supervised users have a different default theme.
    if (IsSupervisedUser()) {
      SetSupervisedUserTheme();
      set_ready();
      return;
    }
#endif
    if (ShouldInitWithSystemTheme())
      UseSystemTheme();
    else
      UseDefaultTheme();
    set_ready();
    return;
  }

  bool loaded_pack = false;

  // If we don't have a file pack, we're updating from an old version.
  base::FilePath path = prefs->GetFilePath(prefs::kCurrentThemePackFilename);
  if (!path.empty()) {
    path = path.Append(chrome::kThemePackFilename);
    SwapThemeSupplier(BrowserThemePack::BuildFromDataPack(path, current_id));
    if (theme_supplier_)
      loaded_pack = true;
  }

  if (loaded_pack) {
    base::RecordAction(UserMetricsAction("Themes.Loaded"));
    set_ready();
  }
  // Else: wait for the extension service to be ready so that the theme pack
  // can be recreated from the extension.
}

void ThemeService::NotifyThemeChanged() {
  if (!ready_)
    return;

  DVLOG(1) << "Sending BROWSER_THEME_CHANGED";
  // Redraw!
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                  content::Source<ThemeService>(this),
                  content::NotificationService::NoDetails());
  // Notify sync that theme has changed.
  if (theme_syncable_service_.get()) {
    theme_syncable_service_->OnThemeChange();
  }
}

bool ThemeService::ShouldUseNativeFrame() const {
  return false;
}

bool ThemeService::HasCustomImage(int id) const {
  return BrowserThemePack::IsPersistentImageID(id) && theme_supplier_ &&
         theme_supplier_->HasCustomImage(id);
}

// static
SkColor ThemeService::GetSeparatorColor(SkColor tab_color,
                                        SkColor frame_color) {
  const float kContrastRatio = 2.f;

  // In most cases, if the tab is lighter than the frame, we darken the
  // frame; if the tab is darker than the frame, we lighten the frame.
  // However, if the frame is already very dark or very light, respectively,
  // this won't contrast sufficiently with the frame color, so we'll need to
  // reverse when we're lightening and darkening.
  const bool lighten = color_utils::GetRelativeLuminance(tab_color) <
                       color_utils::GetRelativeLuminance(frame_color);
  SkColor separator_color =
      lighten ? SK_ColorWHITE : color_utils::GetDarkestColor();

  SkAlpha alpha = color_utils::FindBlendValueForContrastRatio(
      frame_color, separator_color, frame_color, kContrastRatio, 0);
  if (color_utils::GetContrastRatio(
          color_utils::AlphaBlend(separator_color, frame_color, alpha),
          frame_color) >= kContrastRatio) {
    return SkColorSetA(separator_color, alpha);
  }

  separator_color =
      color_utils::BlendTowardOppositeLuma(separator_color, SK_AlphaOPAQUE);

  // If the above call failed to create sufficient contrast, the frame color is
  // already very dark or very light.  Since separators are only used when the
  // tab has low contrast against the frame, the tab color is similarly very
  // dark or very light, just not quite as much so as the frame color.  Blend
  // towards the opposite separator color, and compute the contrast against the
  // tab instead of the frame to ensure both contrasts hit the desired minimum.
  alpha = color_utils::FindBlendValueForContrastRatio(
      frame_color, separator_color, tab_color, kContrastRatio, 0);
  return SkColorSetA(separator_color, alpha);
}

void ThemeService::DoSetTheme(const Extension* extension,
                              bool suppress_infobar) {
  DCHECK(extension->is_theme());
  DCHECK(extensions::ExtensionSystem::Get(profile_)
             ->extension_service()
             ->IsExtensionEnabled(extension->id()));
  BuildFromExtension(extension, suppress_infobar);
}

gfx::ImageSkia* ThemeService::GetImageSkiaNamed(int id, bool incognito) const {
  gfx::Image image = GetImageNamed(id, incognito);
  if (image.IsEmpty())
    return nullptr;
  // TODO(pkotwicz): Remove this const cast.  The gfx::Image interface returns
  // its images const. GetImageSkiaNamed() also should but has many callsites.
  return const_cast<gfx::ImageSkia*>(image.ToImageSkia());
}

SkColor ThemeService::GetColor(int id,
                               bool incognito,
                               bool* has_custom_color) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_custom_color)
    *has_custom_color = false;

  // The incognito NTP always uses the default background color, unless there is
  // a custom NTP background image. See also https://crbug.com/21798#c114.
  if (id == ThemeProperties::COLOR_NTP_BACKGROUND && incognito &&
      !HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    return ThemeProperties::GetDefaultColor(id, incognito);
  }

  SkColor color;
  const int theme_supplier_id = incognito ? GetIncognitoId(id) : id;
  if (theme_supplier_ && theme_supplier_->GetColor(theme_supplier_id, &color)) {
    if (has_custom_color)
      *has_custom_color = true;
    return color;
  }

  return GetDefaultColor(id, incognito);
}

int ThemeService::GetDisplayProperty(int id) const {
  int result = 0;
  if (theme_supplier_ && theme_supplier_->GetDisplayProperty(id, &result)) {
    return result;
  }

  switch (id) {
    case ThemeProperties::NTP_BACKGROUND_ALIGNMENT:
      return ThemeProperties::ALIGN_CENTER;

    case ThemeProperties::NTP_BACKGROUND_TILING:
      return ThemeProperties::NO_REPEAT;

    case ThemeProperties::NTP_LOGO_ALTERNATE: {
      if (UsingDefaultTheme() || UsingSystemTheme())
        return 0;
      if (HasCustomImage(IDR_THEME_NTP_BACKGROUND))
        return 1;
      return IsColorGrayscale(
          GetColor(ThemeProperties::COLOR_NTP_BACKGROUND, false)) ? 0 : 1;
    }

    case ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR:
      return 1;

    default:
      return -1;
  }
}

base::RefCountedMemory* ThemeService::GetRawData(
    int id,
    ui::ScaleFactor scale_factor) const {
  // Check to see whether we should substitute some images.
  int ntp_alternate = GetDisplayProperty(ThemeProperties::NTP_LOGO_ALTERNATE);
  if (id == IDR_PRODUCT_LOGO && ntp_alternate != 0)
    id = IDR_PRODUCT_LOGO_WHITE;

  base::RefCountedMemory* data = nullptr;
  if (theme_supplier_)
    data = theme_supplier_->GetRawData(id, scale_factor);
  if (!data)
    data = rb_.LoadDataResourceBytesForScale(id, ui::SCALE_FACTOR_100P);

  return data;
}

gfx::Image ThemeService::GetImageNamed(int id, bool incognito) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int adjusted_id = id;
  if (incognito) {
    if (id == IDR_THEME_FRAME)
      adjusted_id = IDR_THEME_FRAME_INCOGNITO;
    else if (id == IDR_THEME_FRAME_INACTIVE)
      adjusted_id = IDR_THEME_FRAME_INCOGNITO_INACTIVE;
  }

  gfx::Image image;
  if (theme_supplier_)
    image = theme_supplier_->GetImageNamed(adjusted_id);

  if (image.IsEmpty())
    image = rb_.GetNativeImageNamed(adjusted_id);

  return image;
}

void ThemeService::OnExtensionServiceReady() {
  if (!ready_) {
    // If the ThemeService is not ready yet, the custom theme data pack needs to
    // be recreated from the extension.
    MigrateTheme();
    set_ready();
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  theme_observer_ = std::make_unique<ThemeObserver>(this);
#endif

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ThemeService::RemoveUnusedThemes,
                            weak_ptr_factory_.GetWeakPtr(), false),
      base::TimeDelta::FromSeconds(kRemoveUnusedThemesStartupDelay));
}

void ThemeService::MigrateTheme() {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const Extension* extension =
      service ? service->GetExtensionById(GetThemeID(), false) : nullptr;
  if (extension) {
    DLOG(ERROR) << "Migrating theme";
    // Theme migration is done on the UI thread. Blocking the UI from appearing
    // until it's ready is deemed better than showing a blip of the default
    // theme.
    scoped_refptr<BrowserThemePack> pack(new BrowserThemePack);
    BrowserThemePack::BuildFromExtension(extension, pack);
    OnThemeBuiltFromExtension(extension->id(), pack, true);
    base::RecordAction(UserMetricsAction("Themes.Migrated"));
  } else {
    DLOG(ERROR) << "Theme is mysteriously gone.";
    ClearAllThemeData();
    base::RecordAction(UserMetricsAction("Themes.Gone"));
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

void ThemeService::SavePackName(const base::FilePath& pack_path) {
  profile_->GetPrefs()->SetFilePath(
      prefs::kCurrentThemePackFilename, pack_path);
}

void ThemeService::SaveThemeID(const std::string& id) {
  profile_->GetPrefs()->SetString(prefs::kCurrentThemeID, id);
}

void ThemeService::BuildFromExtension(const Extension* extension,
                                      bool suppress_infobar) {
  build_extension_task_tracker_.TryCancelAll();
  building_extension_id_ = extension->id();
  scoped_refptr<BrowserThemePack> pack(new BrowserThemePack);
  auto task_runner = base::CreateTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  build_extension_task_tracker_.PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::Bind(&BrowserThemePack::BuildFromExtension,
                 base::RetainedRef(extension), pack),
      base::Bind(&ThemeService::OnThemeBuiltFromExtension,
                 weak_ptr_factory_.GetWeakPtr(), extension->id(), pack,
                 suppress_infobar));
}

void ThemeService::OnThemeBuiltFromExtension(
    const extensions::ExtensionId& extension_id,
    scoped_refptr<BrowserThemePack> pack,
    bool suppress_infobar) {
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
  const Extension* extension = extensions::ExtensionRegistry::Get(profile_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension)
    return;

  // Write the packed file to disk.
  extensions::GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&WritePackToDiskCallback, base::RetainedRef(pack),
                            extension->path()));

  const std::string previous_theme_id = GetThemeID();
  const bool previous_using_system_theme = UsingSystemTheme();

  // Save only the extension path. The packed file will be loaded via
  // LoadThemePrefs().
  SavePackName(extension->path());
  SwapThemeSupplier(pack);

  // Clear our image cache.
  SaveThemeID(extension->id());
  NotifyThemeChanged();

  // Same old theme, but the theme has changed (migrated) or auto-updated.
  if (previous_theme_id == extension->id())
    return;

  base::RecordAction(UserMetricsAction("Themes_Installed"));

  bool can_revert_theme = previous_theme_id == kDefaultThemeID;
  if (previous_theme_id != kDefaultThemeID &&
      service->GetInstalledExtension(previous_theme_id)) {
    // Do not disable the previous theme if it is already uninstalled. Sending
    // NOTIFICATION_BROWSER_THEME_CHANGED causes the previous theme to be
    // uninstalled when the notification causes the remaining infobar to close
    // and does not open any new infobars. See crbug.com/468280.

    // Disable the old theme.
    service->DisableExtension(previous_theme_id,
                              extensions::disable_reason::DISABLE_USER_ACTION);

    can_revert_theme = true;
  }

  // Offer to revert to the old theme.
  if (can_revert_theme && !suppress_infobar && extension->is_theme()) {
    // FindTabbedBrowser() is called with |match_original_profiles| true because
    // a theme install in either a normal or incognito window for a profile
    // affects all normal and incognito windows for that profile.
    Browser* browser = chrome::FindTabbedBrowser(profile_, true);
    if (browser) {
      content::WebContents* web_contents =
          browser->tab_strip_model()->GetActiveWebContents();
      if (web_contents) {
        ThemeInstalledInfoBarDelegate::Create(
            InfoBarService::FromWebContents(web_contents),
            extensions::ExtensionSystem::Get(profile_)->extension_service(),
            ThemeServiceFactory::GetForProfile(profile_), extension->name(),
            extension->id(), previous_theme_id, previous_using_system_theme);
      }
    }
  }
  building_extension_id_.clear();
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
bool ThemeService::IsSupervisedUser() const {
  // Do not treat child users as supervised users, so they get the same theme as the parent account
  // instead of getting the default theme.
  return profile_->IsLegacySupervised();
}

void ThemeService::SetSupervisedUserTheme() {
  SetCustomDefaultTheme(new SupervisedUserTheme);
}
#endif
