// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_
#define CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/common/net/net_error_page_support.mojom.h"
#include "chrome/common/network_diagnostics.mojom.h"
#include "chrome/common/network_easter_egg.mojom.h"
#include "components/error_page/common/net_error_info.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace chrome_browser_net {

// A TabHelper that monitors loads for certain types of network errors and
// does interesting things with them.  Currently, starts DNS probes using the
// DnsProbeService whenever a page fails to load with a DNS-related error.
class NetErrorTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NetErrorTabHelper>,
      public chrome::mojom::NetErrorPageSupport,
      public chrome::mojom::NetworkDiagnostics,
      public chrome::mojom::NetworkEasterEgg {
 public:
  enum TestingState {
    TESTING_DEFAULT,
    TESTING_FORCE_DISABLED,
    TESTING_FORCE_ENABLED
  };

  using DnsProbeStatusSnoopCallback =
      base::RepeatingCallback<void(error_page::DnsProbeStatus)>;

  NetErrorTabHelper(const NetErrorTabHelper&) = delete;
  NetErrorTabHelper& operator=(const NetErrorTabHelper&) = delete;

  ~NetErrorTabHelper() override;

  static void BindNetErrorPageSupport(
      mojo::PendingAssociatedReceiver<chrome::mojom::NetErrorPageSupport>
          receiver,
      content::RenderFrameHost* rfh);
  static void BindNetworkDiagnostics(
      mojo::PendingAssociatedReceiver<chrome::mojom::NetworkDiagnostics>
          receiver,
      content::RenderFrameHost* rfh);
  static void BindNetworkEasterEgg(
      mojo::PendingAssociatedReceiver<chrome::mojom::NetworkEasterEgg> receiver,
      content::RenderFrameHost* rfh);

  static void set_state_for_testing(TestingState testing_state);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* prefs);

  // Sets a callback that will be called immediately after the helper sends
  // a NetErrorHelper IPC.  (Used by the DNS probe browser test to know when to
  // check the error page for updates, instead of polling.)
  void set_dns_probe_status_snoop_callback_for_testing(
      const DnsProbeStatusSnoopCallback& dns_probe_status_snoop_callback) {
    dns_probe_status_snoop_callback_ = dns_probe_status_snoop_callback;
  }

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  bool is_showing_download_button_in_error_page() const {
    return is_showing_download_button_in_error_page_;
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // chrome::mojom::NetErrorPageSupport:
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  void DownloadPageLater() override;
  void SetIsShowingDownloadButtonInErrorPage(
      bool showing_download_button) override;
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
#if BUILDFLAG(IS_CHROMEOS)
  void ShowPortalSignin() override;
#endif

 protected:
  // |contents| is the WebContents of the tab this NetErrorTabHelper is
  // attached to.
  explicit NetErrorTabHelper(content::WebContents* contents);
  virtual void StartDnsProbe();
  virtual void SendInfo();
  void OnDnsProbeFinished(error_page::DnsProbeStatus result);

  error_page::DnsProbeStatus dns_probe_status() const {
    return dns_probe_status_;
  }

  content::RenderFrameHostReceiverSet<chrome::mojom::NetworkDiagnostics>&
  network_diagnostics_receivers_for_testing() {
    return network_diagnostics_receivers_;
  }

 private:
  friend class content::WebContentsUserData<NetErrorTabHelper>;

  void OnMainFrameDnsError();

  void InitializePref(content::WebContents* contents);
  bool ProbesAllowed() const;

  // chrome::mojom::NetworkDiagnostics:
  void RunNetworkDiagnostics(const GURL& url) override;

  // chrome::mojom::NetworkEasterEgg:
  void GetHighScore(GetHighScoreCallback callback) override;
  void UpdateHighScore(uint32_t high_score) override;
  void ResetHighScore() override;

  // Shows the diagnostics dialog after its been sanitized, virtual for
  // testing.
  virtual void RunNetworkDiagnosticsHelper(const std::string& sanitized_url);

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // Virtual for testing.
  virtual void DownloadPageLaterHelper(const GURL& url);
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  content::RenderFrameHostReceiverSet<chrome::mojom::NetworkDiagnostics>
      network_diagnostics_receivers_;
  content::RenderFrameHostReceiverSet<chrome::mojom::NetworkEasterEgg>
      network_easter_egg_receivers_;
  content::RenderFrameHostReceiverSet<chrome::mojom::NetErrorPageSupport>
      net_error_page_support_;

  // True if the last provisional load that started was for an error page.
  bool is_error_page_;

  // True if the helper has seen a main frame page load fail with a DNS error,
  // but has not yet seen a new page commit successfully afterwards.
  bool dns_error_active_;

  // True if the helper has seen an error page commit while |dns_error_active_|
  // is true.  (This should never be true if |dns_error_active_| is false.)
  bool dns_error_page_committed_;

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // True if download button is being shown when the error page commits.
  bool is_showing_download_button_in_error_page_;
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  // The status of a DNS probe that may or may not have started or finished.
  // Since the renderer can change out from under the helper (in cross-process
  // navigations), it re-sends the status whenever an error page commits.
  error_page::DnsProbeStatus dns_probe_status_;

  // Optional callback for browser test to snoop on outgoing NetErrorInfo IPCs.
  DnsProbeStatusSnoopCallback dns_probe_status_snoop_callback_;

  // "Use a web service to resolve navigation errors" preference is required
  // to allow probes.
  BooleanPrefMember resolve_errors_with_web_service_;

  // Preference storing the user's current easter egg game high score.
  IntegerPrefMember easter_egg_high_score_;

  base::WeakPtrFactory<NetErrorTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_NET_ERROR_TAB_HELPER_H_
