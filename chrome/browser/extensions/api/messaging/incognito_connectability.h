// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_INCOGNITO_CONNECTABILITY_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_INCOGNITO_CONNECTABILITY_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "url/gurl.h"

class InfoBarService;

namespace content {
class BrowserContext;
class WebContents;
}

namespace infobars {
class InfoBar;
}

namespace extensions {
class Extension;

// Tracks the web connectability of domains to extensions in incognito mode.
//
// The most important functionality is prompting the user to allow or disallow
// connections from incognito tabs to extensions or apps. Users are not prompted
// for extensions which can be enabled in incognito mode. However for apps, it's
// essential we have this functionality because there is no way for them to be
// enabled in incognito.
class IncognitoConnectability : public BrowserContextKeyedAPI {
 public:
  // While in scope, immediately either accepts or denies the alerts that show
  // up, and counts the number of times it was invoked.
  class ScopedAlertTracker {
   public:
    enum Mode {
      INTERACTIVE,
      ALWAYS_ALLOW,
      ALWAYS_DENY,
    };

    explicit ScopedAlertTracker(Mode mode);

    ~ScopedAlertTracker();

    // Returns the number of times the alert has been shown since
    // GetAndResetAlertCount was last called.
    int GetAndResetAlertCount();

   private:
    int last_checked_invocation_count_;
  };

  // Returns the IncognitoConnectability object for |context|. |context| must
  // be off-the-record.
  static IncognitoConnectability* Get(content::BrowserContext* context);

  // Passes true to the provided callback if |url| is allowed to connect from
  // this profile, false otherwise. If unknown, the user will be prompted before
  // an answer is returned.
  void Query(const Extension* extension,
             content::WebContents* web_contents,
             const GURL& url,
             const base::Callback<void(bool)>& callback);

 private:
  struct TabContext {
    TabContext();
    TabContext(const TabContext& other);
    ~TabContext();

    // The infobar being shown in a given tab. The InfoBarService maintains
    // ownership of this object. This struct must always be destroyed before the
    // infobar it tracks.
    infobars::InfoBar* infobar;
    // Connectability queries outstanding on this infobar.
    std::vector<base::Callback<void(bool)>> callbacks;
  };

  friend class BrowserContextKeyedAPIFactory<IncognitoConnectability>;

  explicit IncognitoConnectability(content::BrowserContext* context);
  ~IncognitoConnectability() override;

  typedef std::map<std::string, std::set<GURL> > ExtensionToOriginsMap;
  typedef std::pair<std::string, GURL> ExtensionOriginPair;
  typedef std::map<InfoBarService*, TabContext> PendingOrigin;
  typedef std::map<ExtensionOriginPair, PendingOrigin> PendingOriginMap;

  // Called with the user's selection from the infobar.
  // |response == INTERACTIVE| indicates that the user closed the infobar
  // without selecting allow or deny.
  void OnInteractiveResponse(const std::string& extension_id,
                             const GURL& origin,
                             InfoBarService* infobar_service,
                             ScopedAlertTracker::Mode response);

  // Returns true if the (|extension|, |origin|) pair appears in the map.
  bool IsInMap(const Extension* extension,
               const GURL& origin,
               const ExtensionToOriginsMap& map);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<IncognitoConnectability>*
      GetFactoryInstance();
  static const char* service_name() {
    return "Messaging.IncognitoConnectability";
  }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsCreatedWithBrowserContext = false;

  // The origins that have been prompted for and either allowed or disallowed.
  // These are deliberately stored in-memory so that they're reset when the
  // profile is destroyed (i.e. when the last incognito window is closed).
  ExtensionToOriginsMap allowed_origins_;
  ExtensionToOriginsMap disallowed_origins_;

  // This maps extension/origin pairs to the tabs with an infobar prompting for
  // incognito connectability on them. This also stores a reference to the
  // infobar and the set of callbacks (passed to Query) that will be called when
  // the query is resolved.
  PendingOriginMap pending_origins_;

  base::WeakPtrFactory<IncognitoConnectability> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_INCOGNITO_CONNECTABILITY_H_
