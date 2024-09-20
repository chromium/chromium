// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host_factory.h"

#include <string>

#include "chrome/browser/extensions/extension_side_panel_view_host.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace extensions {

namespace {

// Creates a new ExtensionHost with its associated view, grouping it in the
// appropriate SiteInstance (and therefore process) based on the URL and
// profile.
std::unique_ptr<ExtensionViewHost> CreateViewHostForExtension(
    const Extension* extension,
    const GURL& url,
    Profile* profile,
    mojom::ViewType view_type,
    Browser* browser,
    content::WebContents* web_contents) {
  DCHECK(profile);
  // A NULL browser may only be given for side panels.
  DCHECK(browser || view_type == mojom::ViewType::kExtensionSidePanel);
  scoped_refptr<content::SiteInstance> site_instance =
      ProcessManager::Get(profile)->GetSiteInstanceForURL(url);
  return view_type == mojom::ViewType::kExtensionSidePanel
             ? std::make_unique<ExtensionSidePanelViewHost>(
                   extension, site_instance.get(), url, browser, web_contents)
             : std::make_unique<ExtensionViewHost>(
                   extension, site_instance.get(), url, view_type, browser);
}

// Creates a view host for an extension in an incognito window. Returns NULL
// if the extension is not allowed to run in incognito.
std::unique_ptr<ExtensionViewHost> CreateViewHostForIncognito(
    const Extension* extension,
    const GURL& url,
    Profile* profile,
    Browser* browser,
    content::WebContents* web_contents,
    mojom::ViewType view_type) {
  DCHECK(extension);
  DCHECK(profile->IsOffTheRecord());

  if (!IncognitoInfo::IsSplitMode(extension)) {
    // If it's not split-mode the host is associated with the original profile.
    Profile* original_profile = profile->GetOriginalProfile();
    return CreateViewHostForExtension(extension, url, original_profile,
                                      view_type, browser, web_contents);
  }

  // Create the host if the extension can run in incognito.
  if (util::IsIncognitoEnabled(extension->id(), profile)) {
    return CreateViewHostForExtension(extension, url, profile, view_type,
                                      browser, web_contents);
  }
  NOTREACHED_IN_MIGRATION()
      << "We shouldn't be trying to create an incognito extension view unless "
         "it has been enabled for incognito.";
  return nullptr;
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

// Creates and initializes an ExtensionViewHost for the extension with |url|.
std::unique_ptr<ExtensionViewHost> CreateViewHost(
    const GURL& url,
    Profile* profile,
    Browser* browser,
    content::WebContents* web_contents,
    extensions::mojom::ViewType view_type) {
  DCHECK(profile);
  // A NULL browser may only be given for side panels.
  DCHECK(browser || view_type == mojom::ViewType::kExtensionSidePanel);

  const Extension* extension = GetExtensionForUrl(profile, url);
  if (!extension)
    return nullptr;
  if (profile->IsOffTheRecord()) {
    return CreateViewHostForIncognito(extension, url, profile, browser,
                                      web_contents, view_type);
  }

  return CreateViewHostForExtension(extension, url, profile, view_type, browser,
                                    web_contents);
}

}  // namespace

// static
std::unique_ptr<ExtensionViewHost> ExtensionViewHostFactory::CreatePopupHost(
    const GURL& url,
    Browser* browser) {
  DCHECK(browser);
  return CreateViewHost(url, browser->profile(), browser,
                        /*web_contents=*/nullptr,
                        mojom::ViewType::kExtensionPopup);
}

// static
std::unique_ptr<ExtensionViewHost>
ExtensionViewHostFactory::CreateSidePanelHost(
    const GURL& url,
    Browser* browser,
    content::WebContents* web_contents) {
  DCHECK(browser == nullptr ^ web_contents == nullptr);

  Profile* profile = browser
                         ? browser->profile()
                         : chrome::FindBrowserWithTab(web_contents)->profile();
  return CreateViewHost(url, profile, browser, web_contents,
                        mojom::ViewType::kExtensionSidePanel);
}

}  // namespace extensions
