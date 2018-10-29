// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "ui/base/theme_provider.h"

class BrowserThemePack;
class CustomThemeSupplier;
class ThemeSyncableService;
class Profile;

namespace base {
class FilePath;
}

namespace color_utils {
struct HSL;
}

namespace extensions {
class Extension;
}

namespace gfx {
class Image;
}

namespace theme_service_internal {
class ThemeServiceTest;
}

namespace ui {
class ResourceBundle;
}

class ThemeService : public content::NotificationObserver, public KeyedService {
 public:
  // Public constants used in ThemeService and its subclasses:
  static const char kDefaultThemeID[];

  ThemeService();
  ~ThemeService() override;

  virtual void Init(Profile* profile);

  // KeyedService:
  void Shutdown() override;

  // Overridden from content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Set the current theme to the theme defined in |extension|.
  // |extension| must already be added to this profile's
  // ExtensionService.
  void SetTheme(const extensions::Extension* extension);

  // Similar to SetTheme, but doesn't show an undo infobar.
  void RevertToTheme(const extensions::Extension* extension);

  // Reset the theme to default.
  virtual void UseDefaultTheme();

  // Set the current theme to the system theme. On some platforms, the system
  // theme is the default theme.
  virtual void UseSystemTheme();

  // Returns true if the default theme and system theme are not the same on
  // this platform.
  virtual bool IsSystemThemeDistinctFromDefaultTheme() const;

  // Whether we're using the chrome default theme. Virtual so linux can check
  // if we're using the GTK theme.
  virtual bool UsingDefaultTheme() const;

  // Whether we are using the system theme. On GTK, the system theme is the GTK
  // theme, not the "Classic" theme.
  virtual bool UsingSystemTheme() const;

  // Gets the id of the last installed theme. (The theme may have been further
  // locally customized.)
  virtual std::string GetThemeID() const;

  // This class needs to keep track of the number of theme infobars so that we
  // clean up unused themes.
  void OnInfobarDisplayed();

  // Decrements the number of theme infobars. If the last infobar has been
  // destroyed, uninstalls all themes that aren't the currently selected.
  void OnInfobarDestroyed();

  // Uninstall theme extensions which are no longer in use. |ignore_infobars| is
  // whether unused themes should be removed despite a theme infobar being
  // visible.
  void RemoveUnusedThemes(bool ignore_infobars);

  // Returns the syncable service for syncing theme. The returned service is
  // owned by |this| object.
  virtual ThemeSyncableService* GetThemeSyncableService() const;

  // Gets the ThemeProvider for |profile|. This will be different for an
  // incognito profile and its original profile, even though both profiles use
  // the same ThemeService.
  static const ui::ThemeProvider& GetThemeProviderForProfile(Profile* profile);

  // Gets the ThemeProvider for |profile| that represents the default colour
  // scheme for the OS.
  static const ui::ThemeProvider& GetDefaultThemeProviderForProfile(
      Profile* profile);

 protected:
  // Set a custom default theme instead of the normal default theme.
  virtual void SetCustomDefaultTheme(
      scoped_refptr<CustomThemeSupplier> theme_supplier);

  // Returns true if the ThemeService should use the system theme on startup.
  virtual bool ShouldInitWithSystemTheme() const;

  // Returns the color to use for |id| and |incognito| if the theme service does
  // not provide an override.
  virtual SkColor GetDefaultColor(int id, bool incognito) const;

  // Get the specified tint - |id| is one of the TINT_* enum values.
  color_utils::HSL GetTint(int id, bool incognito) const;

  // Clears all the override fields and saves the dictionary.
  virtual void ClearAllThemeData();

  // Load theme data from preferences.
  virtual void LoadThemePrefs();

  // Let all the browser views know that themes have changed.
  virtual void NotifyThemeChanged();

  // Implementation for ui::ThemeProvider (see block of functions in private
  // section).
  virtual bool ShouldUseNativeFrame() const;
  bool HasCustomImage(int id) const;

  // If there is an inconsistency in preferences, change preferences to a
  // consistent state.
  virtual void FixInconsistentPreferencesIfNeeded();

  Profile* profile() const { return profile_; }

  void set_ready() { ready_ = true; }

  const CustomThemeSupplier* get_theme_supplier() const {
    return theme_supplier_.get();
  }

  // True if the theme service is ready to be used.
  // TODO(pkotwicz): Add DCHECKS to the theme service's getters once
  // ThemeSource no longer uses the ThemeService when it is not ready.
  bool ready_;

 private:
  // This class implements ui::ThemeProvider on behalf of ThemeService and keeps
  // track of the incognito state of the calling code.
  class BrowserThemeProvider : public ui::ThemeProvider {
   public:
    BrowserThemeProvider(const ThemeService& theme_service,
                         bool incognito,
                         bool use_default);
    ~BrowserThemeProvider() override;

    // Overridden from ui::ThemeProvider:
    gfx::ImageSkia* GetImageSkiaNamed(int id) const override;
    SkColor GetColor(int original_id) const override;
    color_utils::HSL GetTint(int original_id) const override;
    int GetDisplayProperty(int id) const override;
    bool ShouldUseNativeFrame() const override;
    bool HasCustomImage(int id) const override;
    bool HasCustomColor(int id) const override;
    base::RefCountedMemory* GetRawData(int id, ui::ScaleFactor scale_factor)
        const override;

   private:
    class DefaultScope;
    friend class DefaultScope;

    const ThemeService& theme_service_;
    bool incognito_;
    bool use_default_;

    DISALLOW_COPY_AND_ASSIGN(BrowserThemeProvider);
  };
  friend class BrowserThemeProvider;
  friend class theme_service_internal::ThemeServiceTest;

  // Key for cache of separator colors; pair is <tab color, frame color>.
  using SeparatorColorKey = std::pair<SkColor, SkColor>;
  using SeparatorColorCache = std::map<SeparatorColorKey, SkColor>;

  // Computes the "toolbar top separator" color.  This color is drawn atop the
  // frame to separate it from tabs, the toolbar, and the new tab button, as
  // well as atop background tabs to separate them from other tabs or the
  // toolbar.  We use semitransparent black or white so as to darken or lighten
  // the frame, with the goal of contrasting with both the frame color and the
  // active tab (i.e. toolbar) color.  (It's too difficult to try to find colors
  // that will contrast with both of these as well as the background tab color,
  // and contrasting with the foreground tab is the most important).
  static SkColor GetSeparatorColor(SkColor tab_color, SkColor frame_color);

  // virtual for testing.
  virtual void DoSetTheme(const extensions::Extension* extension,
                          bool suppress_infobar);

  // These methods provide the implementation for ui::ThemeProvider (exposed
  // via BrowserThemeProvider).
  gfx::ImageSkia* GetImageSkiaNamed(int id, bool incognito) const;
  SkColor GetColor(int id,
                   bool incognito,
                   bool* has_custom_color = nullptr) const;
  int GetDisplayProperty(int id) const;
  base::RefCountedMemory* GetRawData(int id,
                                     ui::ScaleFactor scale_factor) const;

  // Returns a cross platform image for an id.
  //
  // TODO(erg): Make this part of the ui::ThemeProvider and the main way to get
  // theme properties out of the theme provider since it's cross platform.
  gfx::Image GetImageNamed(int id, bool incognito) const;

  // Called when the extension service is ready.
  void OnExtensionServiceReady();

  // Migrate the theme to the new theme pack schema by recreating the data pack
  // from the extension.
  void MigrateTheme();

  // Replaces the current theme supplier with a new one and calls
  // StopUsingTheme() or StartUsingTheme() as appropriate.
  void SwapThemeSupplier(scoped_refptr<CustomThemeSupplier> theme_supplier);

  // Saves the filename of the cached theme pack.
  void SavePackName(const base::FilePath& pack_path);

  // Save the id of the last theme installed.
  void SaveThemeID(const std::string& id);

  // Implementation of SetTheme() (and the fallback from LoadThemePrefs() in
  // case we don't have a theme pack). |new_theme| indicates whether this is a
  // newly installed theme or a migration.
  void BuildFromExtension(const extensions::Extension* extension,
                          bool new_theme);

  // Callback when |pack| has finished or failed building.
  void OnThemeBuiltFromExtension(const extensions::ExtensionId& extension_id,
                                 scoped_refptr<BrowserThemePack> pack,
                                 bool new_theme);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Returns true if the profile belongs to a supervised user.
  bool IsSupervisedUser() const;

  // Sets the current theme to the supervised user theme. Should only be used
  // for supervised user profiles.
  void SetSupervisedUserTheme();
#endif

  ui::ResourceBundle& rb_;
  Profile* profile_;

  scoped_refptr<CustomThemeSupplier> theme_supplier_;

  // The id of the theme extension which has just been installed but has not
  // been loaded yet. The theme extension with |installed_pending_load_id_| may
  // never be loaded if the install is due to updating a disabled theme.
  // |pending_install_id_| should be set to |kDefaultThemeID| if there are no
  // recently installed theme extensions
  std::string installed_pending_load_id_;

  // The number of infobars currently displayed.
  int number_of_infobars_;

  // A cache of already-computed values for COLOR_TOOLBAR_TOP_SEPARATOR, which
  // can be expensive to compute.
  mutable SeparatorColorCache separator_color_cache_;

  content::NotificationRegistrar registrar_;

  std::unique_ptr<ThemeSyncableService> theme_syncable_service_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  class ThemeObserver;
  std::unique_ptr<ThemeObserver> theme_observer_;
#endif

  BrowserThemeProvider original_theme_provider_;
  BrowserThemeProvider incognito_theme_provider_;
  BrowserThemeProvider default_theme_provider_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Allows us to cancel building a theme pack from an extension.
  base::CancelableTaskTracker build_extension_task_tracker_;

  // The ID of the theme that's currently being built on a different thread.
  // We hold onto this just to be sure not to uninstall the extension view
  // RemoveUnusedThemes while it's still being built.
  std::string building_extension_id_;

  base::WeakPtrFactory<ThemeService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThemeService);
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_H_
