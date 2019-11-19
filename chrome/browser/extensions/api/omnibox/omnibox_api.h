// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_OMNIBOX_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_OMNIBOX_API_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/common/extensions/api/omnibox.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/base/window_open_disposition.h"

class Profile;
class TemplateURL;
class TemplateURLService;

namespace content {
class BrowserContext;
class WebContents;
}

namespace gfx {
class Image;
}

namespace extensions {

// Event router class for events related to the omnibox API.
class ExtensionOmniboxEventRouter {
 public:
  // The user has just typed the omnibox keyword. This is sent exactly once in
  // a given input session, before any OnInputChanged events.
  static void OnInputStarted(
      Profile* profile, const std::string& extension_id);

  // The user has changed what is typed into the omnibox while in an extension
  // keyword session. Returns true if someone is listening to this event, and
  // thus we have some degree of confidence we'll get a response.
  static bool OnInputChanged(
      Profile* profile,
      const std::string& extension_id,
      const std::string& input, int suggest_id);

  // The user has accepted the omnibox input.
  static void OnInputEntered(
      content::WebContents* web_contents,
      const std::string& extension_id,
      const std::string& input,
      WindowOpenDisposition disposition);

  // The user has cleared the keyword, or closed the omnibox popup. This is
  // sent at most once in a give input session, after any OnInputChanged events.
  static void OnInputCancelled(
      Profile* profile, const std::string& extension_id);

  // The user has deleted an extension omnibox suggestion result.
  static void OnDeleteSuggestion(Profile* profile,
                                 const std::string& extension_id,
                                 const std::string& suggestion_text);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionOmniboxEventRouter);
};

class OmniboxSendSuggestionsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("omnibox.sendSuggestions", OMNIBOX_SENDSUGGESTIONS)

 protected:
  ~OmniboxSendSuggestionsFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class OmniboxAPI : public BrowserContextKeyedAPI,
                   public ExtensionRegistryObserver {
 public:
  explicit OmniboxAPI(content::BrowserContext* context);
  ~OmniboxAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<OmniboxAPI>* GetFactoryInstance();

  // Convenience method to get the OmniboxAPI for a profile.
  static OmniboxAPI* Get(content::BrowserContext* context);

  // KeyedService implementation.
  void Shutdown() override;

  // Returns the icon to display in the location bar or omnibox popup for the
  // given extension.
  gfx::Image GetOmniboxIcon(const std::string& extension_id);

 private:
  friend class BrowserContextKeyedAPIFactory<OmniboxAPI>;

  typedef std::set<const Extension*> PendingExtensions;

  void OnTemplateURLsLoaded();

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "OmniboxAPI";
  }
  static const bool kServiceRedirectedInIncognito = true;

  Profile* profile_;

  TemplateURLService* url_service_;

  // List of extensions waiting for the TemplateURLService to Load to
  // have keywords registered.
  PendingExtensions pending_extensions_;

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  // Keeps track of favicon-sized omnibox icons for extensions.
  ExtensionIconManager omnibox_icon_manager_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_sub_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxAPI);
};

template <>
void BrowserContextKeyedAPIFactory<OmniboxAPI>::DeclareFactoryDependencies();

class OmniboxSetDefaultSuggestionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("omnibox.setDefaultSuggestion",
                             OMNIBOX_SETDEFAULTSUGGESTION)

 protected:
  ~OmniboxSetDefaultSuggestionFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// If the extension has set a custom default suggestion via
// omnibox.setDefaultSuggestion, apply that to |match|. Otherwise, do nothing.
void ApplyDefaultSuggestionForExtensionKeyword(
    Profile* profile,
    const TemplateURL* keyword,
    const base::string16& remaining_input,
    AutocompleteMatch* match);

// This function converts style information populated by the JSON schema
// // compiler into an ACMatchClassifications object.
ACMatchClassifications StyleTypesToACMatchClassifications(
    const api::omnibox::SuggestResult &suggestion);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_OMNIBOX_API_H_
