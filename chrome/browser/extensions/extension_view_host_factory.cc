// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host_factory.h"

#include <string>

#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension_features.h"
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
    Browser* browser,
    mojom::ViewType view_type) {
  DCHECK(profile);
  // A NULL browser may only be given for dialogs.
  DCHECK(browser || view_type == mojom::ViewType::kExtensionDialog);
  scoped_refptr<content::SiteInstance> site_instance =
      ProcessManager::Get(profile)->GetSiteInstanceForURL(url);
  return std::make_unique<ExtensionViewHost>(extension, site_instance.get(),
                                             url, view_type, browser);
}

// Creates a view host for an extension in an incognito window. Returns NULL
// if the extension is not allowed to run in incognito.
std::unique_ptr<ExtensionViewHost> CreateViewHostForIncognito(
    const Extension* extension,
    const GURL& url,
    Profile* profile,
    Browser* browser,
    mojom::ViewType view_type) {
  DCHECK(extension);
  DCHECK(profile->IsOffTheRecord());

  if (!IncognitoInfo::IsSplitMode(extension)) {
    // If it's not split-mode the host is associated with the original profile.
    Profile* original_profile = profile->GetOriginalProfile();
    return CreateViewHostForExtension(
        extension, url, original_profile, browser, view_type);
  }

  // Create the host if the extension can run in incognito.
  if (util::IsIncognitoEnabled(extension->id(), profile)) {
    return CreateViewHostForExtension(
        extension, url, profile, browser, view_type);
  }
  NOTREACHED() <<
      "We shouldn't be trying to create an incognito extension view unless "
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
    extensions::mojom::ViewType view_type) {
  DCHECK(profile);
  // A NULL browser may only be given for dialogs.
  DCHECK(browser || view_type == mojom::ViewType::kExtensionDialog);

  const Extension* extension = GetExtensionForUrl(profile, url);
  if (!extension)
    return nullptr;
  if (profile->IsOffTheRecord()) {
    return CreateViewHostForIncognito(
        extension, url, profile, browser, view_type);
  }
  return CreateViewHostForExtension(
      extension, url, profile, browser, view_type);
}

}  // namespace

// static
std::unique_ptr<ExtensionViewHost> ExtensionViewHostFactory::CreatePopupHost(
    const GURL& url,
    Browser* browser) {
  DCHECK(browser);
  return CreateViewHost(url, browser->profile(), browser,
                        mojom::ViewType::kExtensionPopup);
}

// static
std::unique_ptr<ExtensionViewHost> ExtensionViewHostFactory::CreateDialogHost(
    const GURL& url,
    Profile* profile) {
  DCHECK(profile);
  return CreateViewHost(url, profile, nullptr,
                        mojom::ViewType::kExtensionDialog);
}

// static
std::unique_ptr<ExtensionViewHost>
ExtensionViewHostFactory::CreateSidePanelHost(const GURL& url,
                                              Browser* browser) {
  DCHECK(browser);
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionSidePanelIntegration));
  return CreateViewHost(url, browser->profile(), browser,
                        mojom::ViewType::kExtensionSidePanel);
}

}  // namespace extensions
