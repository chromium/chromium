// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_functions.h"
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
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/new_tab_page/chrome_colors/chrome_colors_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/increased_contrast_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/layout.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry_observer.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/views/linux_ui/linux_ui.h"
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

absl::optional<ui::ColorId> ThemeProviderColorIdToColorId(int color_id) {
  // clang-format off
  static constexpr const auto kMap = base::MakeFixedFlatMap<int, ui::ColorId>({
#if BUILDFLAG(IS_WIN)
      {TP::COLOR_ACCENT_BORDER_ACTIVE, kColorAccentBorderActive},
      {TP::COLOR_ACCENT_BORDER_INACTIVE, kColorAccentBorderInactive},
#endif  // BUILDFLAG(IS_WIN)
      {TP::COLOR_APP_MENU_HIGHLIGHT_SEVERITY_LOW,
       kColorAppMenuHighlightSeverityLow},
      {TP::COLOR_APP_MENU_HIGHLIGHT_SEVERITY_HIGH,
       kColorAppMenuHighlightSeverityHigh},
      {TP::COLOR_APP_MENU_HIGHLIGHT_SEVERITY_MEDIUM,
       kColorAppMenuHighlightSeverityMedium},
      {TP::COLOR_AVATAR_BUTTON_HIGHLIGHT_NORMAL,
       kColorAvatarButtonHighlightNormal},
      {TP::COLOR_AVATAR_BUTTON_HIGHLIGHT_SYNC_ERROR,
       kColorAvatarButtonHighlightSyncError},
      {TP::COLOR_AVATAR_BUTTON_HIGHLIGHT_SYNC_PAUSED,
       kColorAvatarButtonHighlightSyncPaused},
      {TP::COLOR_BOOKMARK_BAR_BACKGROUND, kColorBookmarkBarBackground},
      {TP::COLOR_BOOKMARK_BUTTON_ICON, kColorBookmarkButtonIcon},
      {TP::COLOR_BOOKMARK_FAVICON, kColorBookmarkFavicon},
      {TP::COLOR_BOOKMARK_SEPARATOR, kColorBookmarkBarSeparator},
      {TP::COLOR_BOOKMARK_TEXT, kColorBookmarkBarForeground},
      {TP::COLOR_CONTROL_BUTTON_BACKGROUND, kColorCaptionButtonBackground},
      {TP::COLOR_DOWNLOAD_SHELF, kColorDownloadShelfBackground},
      {TP::COLOR_DOWNLOAD_SHELF_BUTTON_BACKGROUND,
       kColorDownloadShelfButtonBackground},
      {TP::COLOR_DOWNLOAD_SHELF_BUTTON_TEXT, kColorDownloadShelfButtonText},
      {TP::COLOR_DOWNLOAD_SHELF_CONTENT_AREA_SEPARATOR,
       kColorDownloadShelfContentAreaSeparator},
      {TP::COLOR_DOWNLOAD_SHELF_FOREGROUND, kColorDownloadShelfForeground},
      {TP::COLOR_FEATURE_PROMO_BUBBLE_BACKGROUND,
       kColorFeaturePromoBubbleBackground},
      {TP::COLOR_FEATURE_PROMO_BUBBLE_BUTTON_BORDER,
       kColorFeaturePromoBubbleButtonBorder},
      {TP::COLOR_FEATURE_PROMO_BUBBLE_CLOSE_BUTTON_INK_DROP,
       kColorFeaturePromoBubbleCloseButtonInkDrop},
      {TP::COLOR_FEATURE_PROMO_BUBBLE_DEFAULT_BUTTON_BACKGROUND,
       kColorFeaturePromoBubbleDefaultButtonBackground},
      {TP::COLOR_FEATURE_PROMO_BUBBLE_DEFAULT_BUTTON_FOREGROUND,
       kColorFeaturePromoBubbleDefaultButtonForeground},
      {TP::COLOR_FEATURE_PROMO_BUBBLE_FOREGROUND,
       kColorFeaturePromoBubbleForeground},
      {TP::COLOR_FLYING_INDICATOR_BACKGROUND, kColorFlyingIndicatorBackground},
      {TP::COLOR_FLYING_INDICATOR_FOREGROUND, kColorFlyingIndicatorForeground},
      {TP::COLOR_HOVER_CARD_NO_PREVIEW_BACKGROUND,
       kColorTabHoverCardBackground},
      {TP::COLOR_HOVER_CARD_NO_PREVIEW_FOREGROUND,
       kColorTabHoverCardForeground},
      {TP::COLOR_INFOBAR, kColorInfoBarBackground},
      {TP::COLOR_INFOBAR_CONTENT_AREA_SEPARATOR,
       kColorInfoBarContentAreaSeparator},
      {TP::COLOR_INFOBAR_TEXT, kColorInfoBarForeground},
      {TP::COLOR_LOCATION_BAR_BORDER, kColorLocationBarBorder},
      {TP::COLOR_LOCATION_BAR_BORDER_OPAQUE, kColorLocationBarBorderOpaque},
      {TP::COLOR_NTP_BACKGROUND, kColorNewTabPageBackground},
      {TP::COLOR_NTP_HEADER, kColorNewTabPageHeader},
      {TP::COLOR_NTP_LINK, kColorNewTabPageLink},
      {TP::COLOR_NTP_LOGO, kColorNewTabPageLogo},
      {TP::COLOR_NTP_SECTION_BORDER, kColorNewTabPageSectionBorder},
      {TP::COLOR_NTP_SHORTCUT, kColorNewTabPageMostVisitedTileBackground},
      {TP::COLOR_NTP_TEXT, kColorNewTabPageText},
      {TP::COLOR_NTP_TEXT_LIGHT, kColorNewTabPageTextLight},
      {TP::COLOR_OMNIBOX_BACKGROUND, kColorOmniboxBackground},
      {TP::COLOR_OMNIBOX_BACKGROUND_HOVERED, kColorOmniboxBackgroundHovered},
      {TP::COLOR_OMNIBOX_BUBBLE_OUTLINE, kColorOmniboxBubbleOutline},
      {TP::COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE,
       kColorOmniboxBubbleOutlineExperimentalKeywordMode},
      {TP::COLOR_OMNIBOX_SELECTED_KEYWORD, kColorOmniboxKeywordSelected},
      {TP::COLOR_OMNIBOX_RESULTS_BG, kColorOmniboxResultsBackground},
      {TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED,
       kColorOmniboxResultsBackgroundHovered},
      {TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED,
       kColorOmniboxResultsBackgroundSelected},
      {TP::COLOR_OMNIBOX_RESULTS_BUTTON_BORDER,
       kColorOmniboxResultsButtonBorder},
      {TP::COLOR_OMNIBOX_RESULTS_BUTTON_INK_DROP,
       kColorOmniboxResultsButtonInkDrop},
      {TP::COLOR_OMNIBOX_RESULTS_BUTTON_INK_DROP_SELECTED,
       kColorOmniboxResultsButtonInkDropSelected},
      {TP::COLOR_OMNIBOX_RESULTS_ICON, kColorOmniboxResultsIcon},
      {TP::COLOR_OMNIBOX_RESULTS_ICON_SELECTED,
       kColorOmniboxResultsIconSelected},
      {TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED, kColorOmniboxResultsTextDimmed},
      {TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED,
       kColorOmniboxResultsTextDimmedSelected},
      {TP::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE,
       kColorOmniboxResultsTextNegative},
      {TP::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE_SELECTED,
       kColorOmniboxResultsTextNegativeSelected},
      {TP::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE,
       kColorOmniboxResultsTextPositive},
      {TP::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE_SELECTED,
       kColorOmniboxResultsTextPositiveSelected},
      {TP::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY,
       kColorOmniboxResultsTextSecondary},
      {TP::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY_SELECTED,
       kColorOmniboxResultsTextSecondarySelected},
      {TP::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED,
       kColorOmniboxResultsTextSelected},
      {TP::COLOR_OMNIBOX_RESULTS_URL, kColorOmniboxResultsUrl},
      {TP::COLOR_OMNIBOX_RESULTS_URL_SELECTED, kColorOmniboxResultsUrlSelected},
      {TP::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS,
       kColorOmniboxSecurityChipDangerous},
      {TP::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT,
       kColorOmniboxSecurityChipDefault},
      {TP::COLOR_OMNIBOX_SECURITY_CHIP_SECURE, kColorOmniboxSecurityChipSecure},
      {TP::COLOR_OMNIBOX_TEXT, kColorOmniboxText},
      {TP::COLOR_OMNIBOX_TEXT_DIMMED, kColorOmniboxTextDimmed},
      {TP::COLOR_FRAME_CAPTION_ACTIVE, kColorFrameCaptionActive},
      {TP::COLOR_FRAME_CAPTION_INACTIVE, kColorFrameCaptionInactive},
      {TP::COLOR_FRAME_ACTIVE, ui::kColorFrameActive},
      {TP::COLOR_FRAME_INACTIVE, ui::kColorFrameInactive},
      {TP::COLOR_READ_LATER_BUTTON_HIGHLIGHT, kColorReadLaterButtonHighlight},
      {TP::COLOR_SIDE_PANEL_CONTENT_AREA_SEPARATOR,
       kColorSidePanelContentAreaSeparator},
      {TP::COLOR_STATUS_BUBBLE_ACTIVE, kColorStatusBubbleBackgroundFrameActive},
      {TP::COLOR_STATUS_BUBBLE_INACTIVE,
       kColorStatusBubbleBackgroundFrameInactive},
      {TP::COLOR_STATUS_BUBBLE_TEXT_ACTIVE,
       kColorStatusBubbleForegroundFrameActive},
      {TP::COLOR_STATUS_BUBBLE_TEXT_INACTIVE,
       kColorStatusBubbleForegroundFrameInactive},
      {TP::COLOR_STATUS_BUBBLE_SHADOW, kColorStatusBubbleShadow},
      {TP::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE,
       kColorTabBackgroundActiveFrameActive},
      {TP::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE,
       kColorTabBackgroundActiveFrameInactive},
      {TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE,
       kColorTabBackgroundInactiveFrameActive},
      {TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE,
       kColorTabBackgroundInactiveFrameInactive},
      {TP::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE,
       kColorTabForegroundActiveFrameActive},
      {TP::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE,
       kColorTabForegroundActiveFrameInactive},
      {TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE,
       kColorTabForegroundInactiveFrameActive},
      {TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE,
       kColorTabForegroundInactiveFrameInactive},
      // The colors used for tab groups in the tabstrip.
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_BLUE,
       kColorTabGroupTabStripFrameActiveBlue},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_CYAN,
       kColorTabGroupTabStripFrameActiveCyan},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREEN,
       kColorTabGroupTabStripFrameActiveGreen},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREY,
       kColorTabGroupTabStripFrameActiveGrey},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_ORANGE,
       kColorTabGroupTabStripFrameActiveOrange},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PINK,
       kColorTabGroupTabStripFrameActivePink},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PURPLE,
       kColorTabGroupTabStripFrameActivePurple},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_RED,
       kColorTabGroupTabStripFrameActiveRed},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_YELLOW,
       kColorTabGroupTabStripFrameActiveYellow},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_BLUE,
       kColorTabGroupTabStripFrameInactiveBlue},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_CYAN,
       kColorTabGroupTabStripFrameInactiveCyan},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREEN,
       kColorTabGroupTabStripFrameInactiveGreen},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREY,
       kColorTabGroupTabStripFrameInactiveGrey},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_ORANGE,
       kColorTabGroupTabStripFrameInactiveOrange},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PINK,
       kColorTabGroupTabStripFrameInactivePink},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PURPLE,
       kColorTabGroupTabStripFrameInactivePurple},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_RED,
       kColorTabGroupTabStripFrameInactiveRed},
      {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_YELLOW,
       kColorTabGroupTabStripFrameInactiveYellow},
      // The colors used for tab groups in the bubble dialog view.
      {TP::COLOR_TAB_GROUP_DIALOG_BLUE, kColorTabGroupDialogBlue},
      {TP::COLOR_TAB_GROUP_DIALOG_CYAN, kColorTabGroupDialogCyan},
      {TP::COLOR_TAB_GROUP_DIALOG_GREEN, kColorTabGroupDialogGreen},
      {TP::COLOR_TAB_GROUP_DIALOG_GREY, kColorTabGroupDialogGrey},
      {TP::COLOR_TAB_GROUP_DIALOG_ORANGE, kColorTabGroupDialogOrange},
      {TP::COLOR_TAB_GROUP_DIALOG_PINK, kColorTabGroupDialogPink},
      {TP::COLOR_TAB_GROUP_DIALOG_PURPLE, kColorTabGroupDialogPurple},
      {TP::COLOR_TAB_GROUP_DIALOG_RED, kColorTabGroupDialogRed},
      {TP::COLOR_TAB_GROUP_DIALOG_YELLOW, kColorTabGroupDialogYellow},
      // The colors used for tab groups in the context submenu.
      {TP::COLOR_TAB_GROUP_CONTEXT_MENU_BLUE, kColorTabGroupContextMenuBlue},
      {TP::COLOR_TAB_GROUP_CONTEXT_MENU_CYAN, kColorTabGroupContextMenuCyan},
      {TP::COLOR_TAB_GROUP_CONTEXT_MENU_GREEN, kColorTabGroupContextMenuGreen},
      {TP::COLOR_TAB_GROUP_CONTEXT_MENU_GREY, kColorTabGroupContextMenuGrey},
      {TP::COLOR_TAB_GROUP_CONTEXT_MENU_PINK, kColorTabGroupContextMenuPink},
      {TP::COLOR_TAB_GROUP_CONTEXT_MENU_PURPLE,
       kColorTabGroupContextMenuPurple},
      {TP::COLOR_TAB_GROUP_CONTEXT_MENU_ORANGE,
       kColorTabGroupContextMenuOrange},
      {TP::COLOR_TAB_GROUP_CONTEXT_MENU_RED, kColorTabGroupContextMenuRed},
      {TP::COLOR_TAB_GROUP_CONTEXT_MENU_YELLOW,
       kColorTabGroupContextMenuYellow},
      // The colors used for saved tab group chips on the bookmark bar.
      {TP::COLOR_TAB_GROUP_BOOKMARK_BAR_BLUE, kColorTabGroupBookmarkBarBlue},
      {TP::COLOR_TAB_GROUP_BOOKMARK_BAR_CYAN, kColorTabGroupBookmarkBarCyan},
      {TP::COLOR_TAB_GROUP_BOOKMARK_BAR_GREEN, kColorTabGroupBookmarkBarGreen},
      {TP::COLOR_TAB_GROUP_BOOKMARK_BAR_GREY, kColorTabGroupBookmarkBarGrey},
      {TP::COLOR_TAB_GROUP_BOOKMARK_BAR_ORANGE,
       kColorTabGroupBookmarkBarOrange},
      {TP::COLOR_TAB_GROUP_BOOKMARK_BAR_PINK, kColorTabGroupBookmarkBarPink},
      {TP::COLOR_TAB_GROUP_BOOKMARK_BAR_PURPLE,
       kColorTabGroupBookmarkBarPurple},
      {TP::COLOR_TAB_GROUP_BOOKMARK_BAR_RED, kColorTabGroupBookmarkBarRed},
      {TP::COLOR_TAB_GROUP_BOOKMARK_BAR_YELLOW,
       kColorTabGroupBookmarkBarYellow},
      {TP::COLOR_TAB_STROKE_FRAME_ACTIVE, kColorTabStrokeFrameActive},
      {TP::COLOR_TAB_STROKE_FRAME_INACTIVE, kColorTabStrokeFrameInactive},
      // Toolbar and associated colors.
      {TP::COLOR_TOOLBAR, kColorToolbar},
      {TP::COLOR_TOOLBAR_BUTTON_BACKGROUND, kColorToolbarButtonBackground},
      {TP::COLOR_TOOLBAR_BUTTON_BORDER, kColorToolbarButtonBorder},
      {TP::COLOR_TOOLBAR_BUTTON_ICON, kColorToolbarButtonIcon},
      {TP::COLOR_TOOLBAR_BUTTON_ICON_HOVERED, kColorToolbarButtonIconHovered},
      {TP::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE, kColorToolbarButtonIconInactive},
      {TP::COLOR_TOOLBAR_BUTTON_ICON_PRESSED, kColorToolbarButtonIconPressed},
      {TP::COLOR_TOOLBAR_BUTTON_TEXT, kColorToolbarButtonText},
      {TP::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR,
       kColorToolbarContentAreaSeparator},
      {TP::COLOR_TOOLBAR_FEATURE_PROMO_HIGHLIGHT,
       kColorToolbarFeaturePromoHighlight},
      {TP::COLOR_TOOLBAR_INK_DROP, kColorToolbarInkDrop},
      {TP::COLOR_TOOLBAR_TEXT, kColorToolbarText},
      {TP::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_ACTIVE,
        kColorToolbarTopSeparatorFrameActive},
      {TP::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_INACTIVE,
        kColorToolbarTopSeparatorFrameInactive},
      {TP::COLOR_TOOLBAR_VERTICAL_SEPARATOR, kColorToolbarSeparator},
      {TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE,
       kColorWindowControlButtonBackgroundActive},
      {TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE,
       kColorWindowControlButtonBackgroundInactive},
  });
  // clang-format on
  auto* color_it = kMap.find(color_id);
  if (color_it != kMap.cend()) {
    return color_it->second;
  }
  return absl::nullopt;
}

// Writes the theme pack to disk on a separate thread.
void WritePackToDiskCallback(BrowserThemePack* pack,
                             const base::FilePath& directory) {
  if (g_dont_write_theme_pack_for_testing)
    return;

  const bool success =
      pack->WriteToDisk(directory.Append(chrome::kThemePackFilename));
  base::UmaHistogramBoolean("Browser.ThemeService.WritePackToDisk", success);
}

void ReportHistogramBooleanUsesColorProvider(bool uses_color_provider) {
  UMA_HISTOGRAM_BOOLEAN(
      "Browser.ThemeService.BrowserThemeProvider.GetColor.UsesColorProvider",
      uses_color_provider);
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
  return theme_helper_.GetImageSkiaNamed(id, incognito_, GetThemeSupplier());
}

SkColor ThemeService::BrowserThemeProvider::GetColor(int id) const {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Browser.ThemeService.BrowserThemeProvider.GetColor");
  if (auto color = GetColorProviderColor(id)) {
    ReportHistogramBooleanUsesColorProvider(true);
    return color.value();
  }

  ReportHistogramBooleanUsesColorProvider(false);
  return theme_helper_.GetColor(id, incognito_, GetThemeSupplier());
}

color_utils::HSL ThemeService::BrowserThemeProvider::GetTint(int id) const {
  return theme_helper_.GetTint(id, incognito_, GetThemeSupplier());
}

int ThemeService::BrowserThemeProvider::GetDisplayProperty(int id) const {
  return theme_helper_.GetDisplayProperty(id, GetThemeSupplier());
}

bool ThemeService::BrowserThemeProvider::ShouldUseNativeFrame() const {
  return theme_helper_.ShouldUseNativeFrame(GetThemeSupplier());
}

bool ThemeService::BrowserThemeProvider::HasCustomImage(int id) const {
  return theme_helper_.HasCustomImage(id, GetThemeSupplier());
}

base::RefCountedMemory* ThemeService::BrowserThemeProvider::GetRawData(
    int id,
    ui::ResourceScaleFactor scale_factor) const {
  return theme_helper_.GetRawData(id, GetThemeSupplier(), scale_factor);
}

absl::optional<SkColor>
ThemeService::BrowserThemeProvider::GetColorProviderColor(int id) const {
  if (base::FeatureList::IsEnabled(
          features::kColorProviderRedirectionForThemeProvider)) {
    if (auto provider_color_id = ThemeProviderColorIdToColorId(id)) {
      const ui::NativeTheme* native_theme = nullptr;

      if (incognito_) {
        native_theme = ui::NativeTheme::GetInstanceForDarkUI();
      } else {
        native_theme = ui::NativeTheme::GetInstanceForNativeUi();
#if BUILDFLAG(IS_LINUX)
        if (const auto* linux_ui = views::LinuxUI::instance()) {
          native_theme =
              linux_ui->GetNativeTheme(delegate_->ShouldUseSystemTheme());
        }
#endif
      }

      auto color_provider_key = native_theme->GetColorProviderKey(
          GetThemeSupplier(), delegate_->ShouldUseCustomFrame());
      auto* color_provider =
          ui::ColorProviderManager::Get().GetColorProviderFor(
              color_provider_key);
      return color_provider->GetColor(provider_color_id.value());
    }
  }
  return absl::nullopt;
}

CustomThemeSupplier* ThemeService::BrowserThemeProvider::GetThemeSupplier()
    const {
  return incognito_ ? nullptr : delegate_->GetThemeSupplier();
}

// ThemeService ---------------------------------------------------------------

const char ThemeService::kAutogeneratedThemeID[] = "autogenerated_theme_id";

// static
std::unique_ptr<ui::ThemeProvider> ThemeService::CreateBoundThemeProvider(
    Profile* profile,
    BrowserThemeProviderDelegate* delegate) {
  return std::make_unique<BrowserThemeProvider>(
      ThemeServiceFactory::GetForProfile(profile)->theme_helper_, false,
      delegate);
}

ThemeService::ThemeService(Profile* profile, const ThemeHelper& theme_helper)
    : profile_(profile),
      theme_helper_(theme_helper),
      original_theme_provider_(theme_helper_, false, this),
      incognito_theme_provider_(theme_helper_, true, this) {}

ThemeService::~ThemeService() = default;

void ThemeService::Init() {
  theme_helper_.DCheckCalledOnValidSequence();

  // TODO(https://crbug.com/953978): Use GetNativeTheme() for all platforms.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  if (native_theme)
    native_theme_observation_.Observe(native_theme);

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
}

void ThemeService::Shutdown() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  theme_observer_.reset();
#endif
  native_theme_observation_.Reset();
}

void ThemeService::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  // If we're using the default theme, it means that we need to respond to
  // changes in the HC state. Don't use SetCustomDefaultTheme because that
  // kicks off theme changed events which conflict with the NativeThemeChanged
  // events that are already processing.
  if (UsingDefaultTheme()) {
    scoped_refptr<CustomThemeSupplier> supplier;
    if (theme_helper_.ShouldUseIncreasedContrastThemeSupplier(observed_theme)) {
      supplier =
          base::MakeRefCounted<IncreasedContrastThemeSupplier>(observed_theme);
    }
    SwapThemeSupplier(supplier);
  }
}

CustomThemeSupplier* ThemeService::GetThemeSupplier() const {
  return theme_supplier_.get();
}

bool ThemeService::ShouldUseSystemTheme() const {
#if BUILDFLAG(IS_LINUX)
  return profile_->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme);
#else
  return false;
#endif
}

bool ThemeService::ShouldUseCustomFrame() const {
#if BUILDFLAG(IS_LINUX)
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

void ThemeService::UseDefaultTheme() {
  if (UsingPolicyTheme()) {
    DVLOG(1)
        << "Default theme was not applied because a policy theme has been set.";
    return;
  }

  if (ready_)
    base::RecordAction(base::UserMetricsAction("Themes_Reset"));

  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  if (theme_helper_.ShouldUseIncreasedContrastThemeSupplier(native_theme)) {
    SetCustomDefaultTheme(new IncreasedContrastThemeSupplier(native_theme));
    // Early return here because SetCustomDefaultTheme does ClearAllThemeData
    // and NotifyThemeChanged when it needs to. Without this return, the
    // IncreasedContrastThemeSupplier would get immediately removed if this
    // code runs after ready_ is set to true.
    return;
  }
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
  std::unique_ptr<const extensions::ExtensionSet> extensions(
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet());
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  for (extensions::ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const extensions::Extension* extension = it->get();
    if (extension->is_theme() && extension->id() != current_theme) {
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
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(
          GetThemeSupplier()));
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
  absl::optional<std::string> previous_theme_id;
  if (UsingExtensionTheme())
    previous_theme_id = GetThemeID();

  auto pack = base::MakeRefCounted<BrowserThemePack>(
      CustomThemeSupplier::ThemeType::AUTOGENERATED);
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

// static
void ThemeService::DisableThemePackForTesting() {
  g_dont_write_theme_pack_for_testing = true;
}

std::unique_ptr<ThemeService::ThemeReinstaller>
ThemeService::BuildReinstallerForCurrentTheme() {
  base::OnceClosure reinstall_callback;
  if (UsingExtensionTheme()) {
    reinstall_callback =
        base::BindOnce(&ThemeService::RevertToExtensionTheme,
                       weak_ptr_factory_.GetWeakPtr(), GetThemeID());
  } else if (UsingAutogeneratedTheme()) {
    reinstall_callback = base::BindOnce(
        static_cast<void (ThemeService::*)(SkColor)>(
            &ThemeService::BuildAutogeneratedThemeFromColor),
        weak_ptr_factory_.GetWeakPtr(), GetAutogeneratedThemeColor());
  } else if (UsingSystemTheme()) {
    reinstall_callback = base::BindOnce(&ThemeService::UseSystemTheme,
                                        weak_ptr_factory_.GetWeakPtr());
  } else {
    reinstall_callback = base::BindOnce(&ThemeService::UseDefaultTheme,
                                        weak_ptr_factory_.GetWeakPtr());
  }

  return std::make_unique<ThemeReinstaller>(profile_,
                                            std::move(reinstall_callback));
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

  ClearAllThemeData();
  SwapThemeSupplier(std::move(theme_supplier));
  NotifyThemeChanged();
}

bool ThemeService::ShouldInitWithSystemTheme() const {
  return false;
}

void ThemeService::ClearAllThemeData() {
  if (!ready_)
    return;

  absl::optional<std::string> previous_theme_id;
  if (UsingExtensionTheme())
    previous_theme_id = GetThemeID();

  SwapThemeSupplier(nullptr);
  ClearThemePrefs();

  // Disable extension after modifying the prefs so that unloading the extension
  // doesn't trigger |ClearAllThemeData| again.
  if (previous_theme_id.has_value())
    DisableExtension(previous_theme_id.value());
}

void ThemeService::InitFromPrefs() {
  FixInconsistentPreferencesIfNeeded();

  // If theme color policy was set while browser was off, apply it now.
  if (UsingPolicyTheme()) {
    BuildAutogeneratedPolicyTheme();
    set_ready();
    return;
  }

  std::string current_id = GetThemeID();
  if (current_id == ThemeHelper::kDefaultThemeID) {
    if (ShouldInitWithSystemTheme())
      UseSystemTheme();
    else
      UseDefaultTheme();
    set_ready();
    return;
  }

  if (current_id == kAutogeneratedThemeID) {
    SkColor color = GetAutogeneratedThemeColor();
    BuildAutogeneratedThemeFromColor(color);
    set_ready();
    chrome_colors::ChromeColorsService::RecordColorOnLoadHistogram(color);
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
      set_ready();
    }
  }
  // Else: wait for the extension service to be ready so that the theme pack
  // can be recreated from the extension.
}

void ThemeService::NotifyThemeChanged() {
  if (!ready_)
    return;

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

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
      registry ? registry->GetExtensionById(
                     GetThemeID(), extensions::ExtensionRegistry::ENABLED)
               : nullptr;
  if (extension) {
    // Theme migration is done on the UI thread. Blocking the UI from appearing
    // until it's ready is deemed better than showing a blip of the default
    // theme.
    TRACE_EVENT0("browser", "ThemeService::MigrateTheme - BuildFromExtension");
    DLOG(ERROR) << "Migrating theme";

    scoped_refptr<BrowserThemePack> pack(
        new BrowserThemePack(CustomThemeSupplier::ThemeType::EXTENSION));
    BrowserThemePack::BuildFromExtension(extension, pack.get());
    OnThemeBuiltFromExtension(extension->id(), pack.get(), true);
    base::RecordAction(base::UserMetricsAction("Themes.Migrated"));
  } else {
    DLOG(ERROR) << "Theme is mysteriously gone.";
    ClearAllThemeData();
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
  scoped_refptr<BrowserThemePack> pack(
      new BrowserThemePack(CustomThemeSupplier::ThemeType::EXTENSION));
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
  absl::optional<std::string> previous_theme_id;
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
    // FindTabbedBrowser() is called with |match_original_profiles| true because
    // a theme install in either a normal or incognito window for a profile
    // affects all normal and incognito windows for that profile.
    Browser* browser = chrome::FindTabbedBrowser(profile_, true);
    if (browser) {
      content::WebContents* web_contents =
          browser->tab_strip_model()->GetActiveWebContents();
      if (web_contents) {
        ThemeInstalledInfoBarDelegate::Create(
            infobars::ContentInfoBarManager::FromWebContents(web_contents),
            ThemeServiceFactory::GetForProfile(profile_), extension->name(),
            extension->id(), std::move(reinstaller));
      }
    }
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
  profile_->GetPrefs()->SetString(prefs::kCurrentThemeID,
                                  ThemeHelper::kDefaultThemeID);
}

void ThemeService::SetThemePrefsForExtension(
    const extensions::Extension* extension) {
  ClearThemePrefs();

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
