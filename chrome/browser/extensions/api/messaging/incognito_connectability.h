// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_INCOGNITO_CONNECTABILITY_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_INCOGNITO_CONNECTABILITY_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace infobars {
class ContentInfoBarManager;
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
             base::OnceCallback<void(bool)> callback);

  static void EnsureFactoryBuilt();

 private:
  struct TabContext {
    TabContext();
    ~TabContext();

    // TabContext can't be copied since the callbacks are OnceCallback (and
    // hence, move-only).
    TabContext(const TabContext& other) = delete;
    TabContext& operator=(const TabContext&) = delete;

    // The infobar being shown in a given tab. The
    // infobars::ContentInfoBarManager maintains ownership of this object. This
    // struct must always be destroyed before the infobar it tracks.
    raw_ptr<infobars::InfoBar> infobar;
    // Connectability queries outstanding on this infobar.
    std::vector<base::OnceCallback<void(bool)>> callbacks;
  };

  friend class BrowserContextKeyedAPIFactory<IncognitoConnectability>;

  explicit IncognitoConnectability(content::BrowserContext* context);
  ~IncognitoConnectability() override;

  using ExtensionToOriginsMap = std::map<ExtensionId, std::set<GURL>>;
  using ExtensionOriginPair = std::pair<ExtensionId, GURL>;
  using PendingOrigin = std::map<infobars::ContentInfoBarManager*, TabContext>;
  using PendingOriginMap = std::map<ExtensionOriginPair, PendingOrigin>;

  // Called with the user's selection from the infobar.
  // |response == INTERACTIVE| indicates that the user closed the infobar
  // without selecting allow or deny.
  void OnInteractiveResponse(const ExtensionId& extension_id,
                             const GURL& origin,
                             infobars::ContentInfoBarManager* infobar_manager,
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
