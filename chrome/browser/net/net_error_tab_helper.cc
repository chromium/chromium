// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/dns_probe_service_factory.h"
#include "chrome/browser/net/net_error_diagnostics_dialog.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/embedder_support/pref_names.h"
#include "components/error_page/common/net_error_info.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::WebContents;
using content::WebContentsObserver;
using error_page::DnsProbeStatus;
using error_page::DnsProbeStatusToString;
using ui::PageTransition;

namespace chrome_browser_net {

namespace {

static NetErrorTabHelper::TestingState testing_state_ =
    NetErrorTabHelper::TESTING_DEFAULT;

}  // namespace

NetErrorTabHelper::~NetErrorTabHelper() {
}

// static
void NetErrorTabHelper::BindNetErrorPageSupport(
    mojo::PendingAssociatedReceiver<chrome::mojom::NetErrorPageSupport>
        receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* tab_helper = NetErrorTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;
  tab_helper->net_error_page_support_.Bind(rfh, std::move(receiver));
}

// static
void NetErrorTabHelper::BindNetworkDiagnostics(
    mojo::PendingAssociatedReceiver<chrome::mojom::NetworkDiagnostics> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* tab_helper = NetErrorTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;
  tab_helper->network_diagnostics_receivers_.Bind(rfh, std::move(receiver));
}

// static
void NetErrorTabHelper::BindNetworkEasterEgg(
    mojo::PendingAssociatedReceiver<chrome::mojom::NetworkEasterEgg> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* tab_helper = NetErrorTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;
  tab_helper->network_easter_egg_receivers_.Bind(rfh, std::move(receiver));
}

// static
void NetErrorTabHelper::set_state_for_testing(TestingState state) {
  testing_state_ = state;
}

void NetErrorTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // Ignore subframe and fencedframe creation - only primary frame error pages
  // can link to the platform's network diagnostics dialog.
  if (render_frame_host->GetParentOrOuterDocument())
    return;

  mojo::AssociatedRemote<chrome::mojom::NetworkDiagnosticsClient> client;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  client->SetCanShowNetworkDiagnosticsDialog(
      CanShowNetworkDiagnosticsDialog(web_contents()));
}

void NetErrorTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  if (net::IsHostnameResolutionError(navigation_handle->GetNetErrorCode())) {
    dns_error_active_ = true;
    OnMainFrameDnsError();
  }

  // Resend status every time an error page commits; this is somewhat spammy,
  // but ensures that the status will make it to the real error page, even if
  // the link doctor loads a blank intermediate page or the tab switches
  // renderer processes.
  if (navigation_handle->IsErrorPage() && dns_error_active_) {
    dns_error_page_committed_ = true;
    DVLOG(1) << "Committed error page; resending status.";
    SendInfo();
  } else if (navigation_handle->HasCommitted() &&
             !navigation_handle->IsErrorPage()) {
    dns_error_active_ = false;
    dns_error_page_committed_ = false;
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
    is_showing_download_button_in_error_page_ = false;
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
  }
}

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
void NetErrorTabHelper::DownloadPageLater() {
  // Makes sure that this is coming from an error page.
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry || entry->GetPageType() != content::PAGE_TYPE_ERROR)
    return;

  // Only download the page for HTTP/HTTPS URLs.
  GURL url(entry->GetVirtualURL());
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  DownloadPageLaterHelper(url);
}

void NetErrorTabHelper::SetIsShowingDownloadButtonInErrorPage(
    bool showing_download_button) {
  is_showing_download_button_in_error_page_ = showing_download_button;
}
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

#if BUILDFLAG(IS_CHROMEOS)
void NetErrorTabHelper::ShowPortalSignin() {
  // TODO(b/247618374): Lacros implementation.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NetworkPortalSigninController::Get()->ShowSignin(
      ash::NetworkPortalSigninController::SigninSource::kErrorPage);
#endif
}
#endif

NetErrorTabHelper::NetErrorTabHelper(WebContents* contents)
    : WebContentsObserver(contents),
      content::WebContentsUserData<NetErrorTabHelper>(*contents),
      network_diagnostics_receivers_(contents, this),
      network_easter_egg_receivers_(contents, this),
      net_error_page_support_(contents, this),
      is_error_page_(false),
      dns_error_active_(false),
      dns_error_page_committed_(false),
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
      is_showing_download_button_in_error_page_(false),
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
      dns_probe_status_(error_page::DNS_PROBE_POSSIBLE) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If this helper is under test, it won't have a WebContents.
  if (contents)
    InitializePref(contents);
}

void NetErrorTabHelper::OnMainFrameDnsError() {
  if (ProbesAllowed()) {
    // Don't start more than one probe at a time.
    if (dns_probe_status_ != error_page::DNS_PROBE_STARTED) {
      StartDnsProbe();
      dns_probe_status_ = error_page::DNS_PROBE_STARTED;
    }
  } else {
    dns_probe_status_ = error_page::DNS_PROBE_NOT_RUN;
  }
}

void NetErrorTabHelper::StartDnsProbe() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(dns_error_active_);
  DCHECK_NE(error_page::DNS_PROBE_STARTED, dns_probe_status_);

  DVLOG(1) << "Starting DNS probe.";

  DnsProbeService* probe_service = DnsProbeServiceFactory::GetForContext(
      web_contents()->GetBrowserContext());
  probe_service->ProbeDns(base::BindOnce(&NetErrorTabHelper::OnDnsProbeFinished,
                                         weak_factory_.GetWeakPtr()));
}

void NetErrorTabHelper::OnDnsProbeFinished(DnsProbeStatus result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(error_page::DNS_PROBE_STARTED, dns_probe_status_);
  DCHECK(error_page::DnsProbeStatusIsFinished(result));

  DVLOG(1) << "Finished DNS probe with result "
           << DnsProbeStatusToString(result) << ".";

  dns_probe_status_ = result;

  if (dns_error_page_committed_)
    SendInfo();
}

// static
void NetErrorTabHelper::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  // embedder_support::kAlternateErrorPagesEnabled is registered by
  // NavigationCorrectionTabObserver.

  prefs->RegisterIntegerPref(prefs::kNetworkEasterEggHighScore, 0,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void NetErrorTabHelper::InitializePref(WebContents* contents) {
  DCHECK(contents);

  BrowserContext* browser_context = contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  resolve_errors_with_web_service_.Init(
      embedder_support::kAlternateErrorPagesEnabled, profile->GetPrefs());
  easter_egg_high_score_.Init(prefs::kNetworkEasterEggHighScore,
                              profile->GetPrefs());
}

bool NetErrorTabHelper::ProbesAllowed() const {
  if (testing_state_ != TESTING_DEFAULT)
    return testing_state_ == TESTING_FORCE_ENABLED;

  // TODO(juliatuttle): Disable on mobile?
  return *resolve_errors_with_web_service_;
}

void NetErrorTabHelper::SendInfo() {
  DCHECK_NE(error_page::DNS_PROBE_POSSIBLE, dns_probe_status_);
  DCHECK(dns_error_page_committed_);

  DVLOG(1) << "Sending status " << DnsProbeStatusToString(dns_probe_status_);
  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();

  mojo::AssociatedRemote<chrome::mojom::NetworkDiagnosticsClient> client;
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  client->DNSProbeStatus(dns_probe_status_);

  if (!dns_probe_status_snoop_callback_.is_null())
    dns_probe_status_snoop_callback_.Run(dns_probe_status_);
}

void NetErrorTabHelper::RunNetworkDiagnostics(const GURL& url) {
  // Only run diagnostics on HTTP or HTTPS URLs.  Shouldn't receive URLs with
  // any other schemes, but the renderer is not trusted.
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return;

  // Sanitize URL prior to running diagnostics on it.
  RunNetworkDiagnosticsHelper(url.DeprecatedGetOriginAsURL().spec());
}

void NetErrorTabHelper::RunNetworkDiagnosticsHelper(
    const std::string& sanitized_url) {
  // The button shouldn't even be shown in this case, but still best to be safe,
  // since the renderer isn't trusted.
  if (!CanShowNetworkDiagnosticsDialog(web_contents()))
    return;

  if (!network_diagnostics_receivers_.GetCurrentTargetFrame()
           ->IsInPrimaryMainFrame()) {
    return;
  }

  ShowNetworkDiagnosticsDialog(web_contents(), sanitized_url);
}

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
void NetErrorTabHelper::DownloadPageLaterHelper(const GURL& page_url) {
  offline_pages::OfflinePageUtils::ScheduleDownload(
      web_contents(), offline_pages::kAsyncNamespace, page_url,
      offline_pages::OfflinePageUtils::DownloadUIActionFlags::PROMPT_DUPLICATE);
}
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

void NetErrorTabHelper::GetHighScore(GetHighScoreCallback callback) {
  std::move(callback).Run(
      static_cast<uint32_t>(easter_egg_high_score_.GetValue()));
}

void NetErrorTabHelper::UpdateHighScore(uint32_t high_score) {
  if (high_score <= static_cast<uint32_t>(easter_egg_high_score_.GetValue()))
    return;
  easter_egg_high_score_.SetValue(static_cast<int>(high_score));
}

void NetErrorTabHelper::ResetHighScore() {
  easter_egg_high_score_.SetValue(0);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NetErrorTabHelper);

}  // namespace chrome_browser_net
