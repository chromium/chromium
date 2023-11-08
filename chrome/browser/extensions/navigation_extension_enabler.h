// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_NAVIGATION_EXTENSION_ENABLER_H_
#define CHROME_BROWSER_EXTENSIONS_NAVIGATION_EXTENSION_ENABLER_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {

// The NavigationExtensionEnabler listens to navigation notifications. If the
// user navigates into an extension that has been disabled due to a permission
// increase, it prompts the user to accept the new permissions and re-enables
// the extension.
class NavigationExtensionEnabler
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NavigationExtensionEnabler>,
      public ExtensionRegistryObserver {
 public:
  NavigationExtensionEnabler(const NavigationExtensionEnabler&) = delete;
  NavigationExtensionEnabler& operator=(const NavigationExtensionEnabler&) =
      delete;

  ~NavigationExtensionEnabler() override;

 private:
  explicit NavigationExtensionEnabler(content::WebContents* web_contents);
  friend class content::WebContentsUserData<NavigationExtensionEnabler>;

  // Checks if the WebContents has navigated to an extension's web extent. If it
  // has and the extension is disabled due to a permissions increase, this
  // prompts the user to accept the new permissions and enables the extension.
  void PromptToEnableExtensionIfNecessary(const GURL& url);

  void OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload payload);

  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

  // The UI used to confirm enabling extensions.
  std::unique_ptr<ExtensionInstallPrompt> extension_install_prompt_;

  // The data we keep track of when prompting to enable extensions.
  std::string in_progress_prompt_extension_id_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::WeakPtrFactory<NavigationExtensionEnabler> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_NAVIGATION_EXTENSION_ENABLER_H_
