// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEIC_GEIC_PWC_MANAGER_H_
#define CHROME_BROWSER_GEIC_GEIC_PWC_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "chrome/browser/geic/geic.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"

class Profile;

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace geic {

class GeicBrowserHostImpl;

// Command line switch to configure the GEiC guest URL.
inline constexpr char kGeicGuestURLSwitch[] = "geic-guest-url";

// Manages PrivilegedWebContents containers and browser host implementations
// for the Gemini Enterprise in Chrome (GEiC) side panel prototype on a per-tab
// basis.
class GeicPwcManager : public base::SupportsUserData::Data,
                       public ProfileObserver {
 public:
  // Returns the guest URL configured via --geic-guest-url switch,
  // the kGeicGuestURL feature parameter, or enterprise policy.
  static GURL GetConfiguredGuestURL(Profile* profile);

  static GeicPwcManager* GetOrCreateForProfile(Profile* profile,
                                               GURL dev_url = GURL());

  explicit GeicPwcManager(Profile* profile, GURL dev_url = GURL());
  GeicPwcManager(const GeicPwcManager&) = delete;
  GeicPwcManager& operator=(const GeicPwcManager&) = delete;
  ~GeicPwcManager() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Returns the WebContents for `tab`, creating and navigating a new
  // PrivilegedWebContents container and GeicBrowserHostImpl if one does not
  // already exist. Returns nullptr if `tab` is null or guest URL is
  // unconfigured.
  content::WebContents* GetOrCreateWebContentsForTab(tabs::TabInterface* tab);

  // Returns the PrivilegedWebContents associated with `tab`, or nullptr.
  pwc::PrivilegedWebContents* GetPwcForTab(tabs::TabInterface* tab);

  // Returns the GeicBrowserHostImpl associated with `privileged`, or nullptr.
  GeicBrowserHostImpl* GetBrowserHostForPwc(
      pwc::PrivilegedWebContents* privileged);

  // Erases the PWC and browser host associated with the tab.
  void RemoveTab(tabs::TabHandle tab_handle);

  const GURL& guest_url() const { return dev_url_; }
  size_t entry_count_for_testing() const { return entries_.size(); }

 private:
  class TabEntry : public content::WebContentsObserver {
   public:
    TabEntry(GeicPwcManager* manager,
             tabs::TabInterface* tab,
             std::unique_ptr<pwc::PrivilegedWebContents> pwc,
             std::unique_ptr<GeicBrowserHostImpl> browser_host);
    TabEntry(const TabEntry&) = delete;
    TabEntry& operator=(const TabEntry&) = delete;
    ~TabEntry() override;

    // content::WebContentsObserver:
    void RenderFrameCreated(
        content::RenderFrameHost* render_frame_host) override;
    void ReadyToCommitNavigation(
        content::NavigationHandle* navigation_handle) override;

    content::WebContents* web_contents() {
      return pwc_ ? pwc_->web_contents() : nullptr;
    }
    pwc::PrivilegedWebContents* pwc() { return pwc_.get(); }
    GeicBrowserHostImpl* browser_host() { return browser_host_.get(); }

   private:
    void OnTabWillDetach(tabs::TabInterface* tab,
                         tabs::TabInterface::DetachReason reason);
    void OnTabWillDiscard(tabs::TabInterface* tab,
                          content::WebContents* old_contents,
                          content::WebContents* new_contents);

    const raw_ptr<GeicPwcManager> manager_;
    tabs::TabHandle tab_handle_;
    std::unique_ptr<pwc::PrivilegedWebContents> pwc_;
    std::unique_ptr<GeicBrowserHostImpl> browser_host_;
    base::CallbackListSubscription will_detach_subscription_;
    base::CallbackListSubscription will_discard_subscription_;
  };

  const raw_ptr<Profile> profile_;
  // The URL loaded into the GEiC PrivilegedWebContents (the GE panel origin).
  // During development, this is set via the --geic-guest-url command-line
  // switch or the kGeicGuestURL feature parameter; for production, this will be
  // a compile-time constant or Finch-driven. The origin is deliberately never
  // derived from enterprise policy to prevent an administrator from pointing a
  // capability-holding panel at an arbitrary site.
  const GURL dev_url_;
  absl::flat_hash_map<tabs::TabHandle, std::unique_ptr<TabEntry>> entries_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  base::WeakPtrFactory<GeicPwcManager> weak_factory_{this};
};

void BindGeicBrowserHost(
    content::RenderFrameHost* rfh,
    mojo::PendingReceiver<mojom::GeicBrowserHost> receiver);

}  // namespace geic

#endif  // CHROME_BROWSER_GEIC_GEIC_PWC_MANAGER_H_
