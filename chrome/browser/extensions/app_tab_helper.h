// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_TAB_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_APP_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class Image;
}

namespace extensions {

// Handles apps for the extensions system.
class AppTabHelper : public content::WebContentsObserver,
                     public ExtensionRegistryObserver,
                     public content::WebContentsUserData<AppTabHelper> {
 public:
  AppTabHelper(const AppTabHelper&) = delete;
  AppTabHelper& operator=(const AppTabHelper&) = delete;

  ~AppTabHelper() override;

  // Sets the extension denoting this as an app. If `extension` is non-null this
  // tab becomes an app-tab. WebContents does not listen for unload events for
  // the extension. It's up to consumers of WebContents to do that.
  //
  // NOTE: this should only be manipulated before the tab is added to a browser.
  // TODO(sky): resolve if this is the right way to identify an app tab. If it
  // is, then this should be passed in the constructor.
  void SetExtensionApp(const Extension* extension);

  // Convenience for setting the app extension by id. This does nothing if
  // `extension_app_id` is empty, or an extension can't be found given the
  // specified id.
  void SetExtensionAppById(const ExtensionId& extension_app_id);

  // Returns true if an app extension has been set.
  bool is_app() const { return extension_app_ != nullptr; }

  // Return ExtensionId for extension app.
  // If an app extension has not been set, returns empty id.
  ExtensionId GetExtensionAppId() const;

  // If an app extension has been explicitly set for this WebContents its icon
  // is returned.
  //
  // NOTE: the returned icon is larger than 16x16 (its size is
  // extension_misc::EXTENSION_ICON_SMALLISH).
  SkBitmap* GetExtensionAppIcon();

 private:
  friend class content::WebContentsUserData<AppTabHelper>;

  explicit AppTabHelper(content::WebContents* web_contents);

  // content::WebContentsObserver overrides.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidCloneToNewWebContents(
      content::WebContents* old_web_contents,
      content::WebContents* new_web_contents) override;

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Resets app_icon_ and if `extension` is non-null uses ImageLoader to load
  // the extension's image asynchronously.
  void UpdateExtensionAppIcon(const Extension* extension);

  const Extension* GetExtension(const ExtensionId& extension_app_id);

  void OnImageLoaded(const gfx::Image& image);

  raw_ptr<Profile> profile_ = nullptr;

  // If non-null this tab is an app tab and this is the extension the tab was
  // created for.
  raw_ptr<const Extension> extension_app_ = nullptr;

  // Icon for extension_app_ (if non-null) or a manually-set icon for
  // non-extension apps.
  SkBitmap extension_app_icon_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};

  // Vend weak pointers that can be invalidated to stop in-progress loads.
  base::WeakPtrFactory<AppTabHelper> image_loader_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_TAB_HELPER_H_
