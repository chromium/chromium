// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host_factory.h"

#include <string>

#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace extensions {

namespace {

// Delegate for ExtensionViewHost attached to a specific browser window.
class ExtensionViewHostBrowserDelegate : public ExtensionViewHost::Delegate {
 public:
  explicit ExtensionViewHostBrowserDelegate(Browser* browser)
      : browser_(browser) {
    DCHECK(browser_);
  }
  ExtensionViewHostBrowserDelegate(const ExtensionViewHostBrowserDelegate&) =
      delete;
  ExtensionViewHostBrowserDelegate& operator=(
      const ExtensionViewHostBrowserDelegate&) = delete;
  ~ExtensionViewHostBrowserDelegate() override = default;

  content::WebContents* OpenURL(
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    return browser_->OpenURL(params, std::move(navigation_handle_callback));
  }

  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    return browser_->PreHandleKeyboardEvent(source, event);
  }

  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override {
    return browser_->OpenEyeDropper(frame, listener);
  }

  WindowController* GetExtensionWindowController() const override {
    return browser_->extension_window_controller();
  }

 private:
  raw_ptr<Browser> browser_;
};

// Delegate for ExtensionViewHost attached to a specific tab.
class ExtensionViewHostTabDelegate : public ExtensionViewHost::Delegate {
 public:
  explicit ExtensionViewHostTabDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {
    DCHECK(web_contents_);
  }
  ExtensionViewHostTabDelegate(const ExtensionViewHostTabDelegate&) = delete;
  ExtensionViewHostTabDelegate& operator=(const ExtensionViewHostTabDelegate&) =
      delete;
  ~ExtensionViewHostTabDelegate() override = default;

  content::WebContents* OpenURL(
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    Browser* browser = FindBrowser();
    if (browser == nullptr) {
      return nullptr;
    }
    return browser->OpenURL(params, std::move(navigation_handle_callback));
  }

  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    Browser* browser = FindBrowser();
    if (browser == nullptr) {
      return content::KeyboardEventProcessingResult::NOT_HANDLED;
    }
    return browser->PreHandleKeyboardEvent(source, event);
  }

  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override {
    Browser* browser = FindBrowser();
    if (browser == nullptr) {
      return nullptr;
    }
    return browser->OpenEyeDropper(frame, listener);
  }

  WindowController* GetExtensionWindowController() const override {
    Browser* browser = FindBrowser();
    if (browser == nullptr) {
      return nullptr;
    }
    return browser->extension_window_controller();
  }

 private:
  Browser* FindBrowser() const {
    return chrome::FindBrowserWithTab(web_contents_);
  }

  raw_ptr<content::WebContents> web_contents_;
};

// Creates a new ExtensionHost with its associated view, grouping it in the
// appropriate SiteInstance (and therefore process) based on the URL and
// profile.
std::unique_ptr<ExtensionViewHost> CreateViewHostForExtension(
    const Extension* extension,
    const GURL& url,
    Profile* profile,
    mojom::ViewType view_type,
    std::unique_ptr<ExtensionViewHost::Delegate> delegate) {
  DCHECK(profile);
  scoped_refptr<content::SiteInstance> site_instance =
      ProcessManager::Get(profile)->GetSiteInstanceForURL(url);
  return std::make_unique<ExtensionViewHost>(extension, site_instance.get(),
                                             profile, url, view_type,
                                             std::move(delegate));
}

// Creates a view host for an extension in an incognito window. Returns NULL
// if the extension is not allowed to run in incognito.
std::unique_ptr<ExtensionViewHost> CreateViewHostForIncognito(
    const Extension* extension,
    const GURL& url,
    Profile* profile,
    mojom::ViewType view_type,
    std::unique_ptr<ExtensionViewHost::Delegate> delegate) {
  DCHECK(extension);
  DCHECK(profile->IsOffTheRecord());

  if (!IncognitoInfo::IsSplitMode(extension)) {
    // If it's not split-mode the host is associated with the original profile.
    Profile* original_profile = profile->GetOriginalProfile();
    return CreateViewHostForExtension(extension, url, original_profile,
                                      view_type, std::move(delegate));
  }

  // Create the host if the extension can run in incognito.
  if (util::IsIncognitoEnabled(extension->id(), profile)) {
    return CreateViewHostForExtension(extension, url, profile, view_type,
                                      std::move(delegate));
  }
  NOTREACHED() << "We shouldn't be trying to create an incognito extension "
                  "view unless it has been enabled for incognito.";
}

// Returns the extension associated with |url| in |profile|. Returns NULL if
// the extension does not exist.
const Extension* GetExtensionForUrl(Profile* profile, const GURL& url) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  if (!registry)
    return nullptr;
  std::string extension_id = url.host();
  return registry->enabled_extensions().GetByID(extension_id);
}

std::unique_ptr<ExtensionViewHost> CreateExtensionViewHost(
    const Extension& extension,
    const GURL& url,
    Profile* profile,
    extensions::mojom::ViewType view_type,
    std::unique_ptr<ExtensionViewHost::Delegate> delegate) {
  CHECK(profile);

  if (profile->IsOffTheRecord()) {
    return CreateViewHostForIncognito(&extension, url, profile, view_type,
                                      std::move(delegate));
  }

  return CreateViewHostForExtension(&extension, url, profile, view_type,
                                    std::move(delegate));
}

// Creates and initializes an ExtensionViewHost for the extension with |url|.
std::unique_ptr<ExtensionViewHost> CreateViewHost(
    const GURL& url,
    Profile* profile,
    extensions::mojom::ViewType view_type,
    std::unique_ptr<ExtensionViewHost::Delegate> delegate) {
  CHECK(profile);

  const Extension* extension = GetExtensionForUrl(profile, url);
  if (!extension) {
    return nullptr;
  }

  return CreateExtensionViewHost(*extension, url, profile, view_type,
                                 std::move(delegate));
}

}  // namespace

// static
std::unique_ptr<ExtensionViewHost> ExtensionViewHostFactory::CreatePopupHost(
    const GURL& url,
    Browser* browser) {
  DCHECK(browser);
  return CreateViewHost(
      url, browser->profile(), mojom::ViewType::kExtensionPopup,
      std::make_unique<ExtensionViewHostBrowserDelegate>(browser));
}

// static
std::unique_ptr<ExtensionViewHost>
ExtensionViewHostFactory::CreateSidePanelHost(
    const Extension& extension,
    const GURL& url,
    BrowserWindowInterface* browser,
    tabs::TabInterface* tab_interface) {
  DCHECK(browser == nullptr ^ tab_interface == nullptr);

  Profile* profile =
      browser ? browser->GetProfile()
              : tab_interface->GetBrowserWindowInterface()->GetProfile();

  std::unique_ptr<ExtensionViewHost::Delegate> delegate =
      browser ? static_cast<std::unique_ptr<ExtensionViewHost::Delegate>>(
                    std::make_unique<ExtensionViewHostBrowserDelegate>(
                        browser->GetBrowserForMigrationOnly()))
              : std::make_unique<ExtensionViewHostTabDelegate>(
                    tab_interface->GetContents());

  return CreateExtensionViewHost(extension, url, profile,
                                 mojom::ViewType::kExtensionSidePanel,
                                 std::move(delegate));
}

}  // namespace extensions
