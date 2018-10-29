// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/net_error_diagnostics_dialog.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "components/error_page/common/net_error_info.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

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

void OnDnsProbeFinishedOnIOThread(
    const base::Callback<void(DnsProbeStatus)>& callback,
    DnsProbeStatus result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, result));
}

// Can only access g_browser_process->io_thread() from the browser thread,
// so have to pass it in to the callback instead of dereferencing it here.
void StartDnsProbeOnIOThread(
    const base::Callback<void(DnsProbeStatus)>& callback,
    IOThread* io_thread) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DnsProbeService* probe_service =
      io_thread->globals()->dns_probe_service.get();

  probe_service->ProbeDns(base::Bind(&OnDnsProbeFinishedOnIOThread, callback));
}

}  // namespace

NetErrorTabHelper::~NetErrorTabHelper() {
}

// static
void NetErrorTabHelper::set_state_for_testing(TestingState state) {
  testing_state_ = state;
}

void NetErrorTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // Ignore subframe creation - only main frame error pages can link to the
  // platform's network diagnostics dialog.
  if (render_frame_host->GetParent())
    return;

  chrome::mojom::NetworkDiagnosticsClientAssociatedPtr client;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  client->SetCanShowNetworkDiagnosticsDialog(CanShowNetworkDiagnosticsDialog());
}

void NetErrorTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  if (navigation_handle->IsErrorPage() &&
      PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                               ui::PAGE_TRANSITION_RELOAD)) {
    error_page::RecordEvent(
        error_page::NETWORK_ERROR_PAGE_BROWSER_INITIATED_RELOAD);
  }
}

void NetErrorTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  if (net::IsDnsError(navigation_handle->GetNetErrorCode())) {
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

bool NetErrorTabHelper::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host != web_contents()->GetMainFrame())
    return false;
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NetErrorTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DownloadPageLater,
                        OnDownloadPageLater)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SetIsShowingDownloadButtonInErrorPage,
                        OnSetIsShowingDownloadButtonInErrorPage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
}

NetErrorTabHelper::NetErrorTabHelper(WebContents* contents)
    : WebContentsObserver(contents),
      network_diagnostics_bindings_(contents, this),
      is_error_page_(false),
      dns_error_active_(false),
      dns_error_page_committed_(false),
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
      is_showing_download_button_in_error_page_(false),
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
      dns_probe_status_(error_page::DNS_PROBE_POSSIBLE),
      weak_factory_(this) {
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

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&StartDnsProbeOnIOThread,
                     base::Bind(&NetErrorTabHelper::OnDnsProbeFinished,
                                weak_factory_.GetWeakPtr()),
                     g_browser_process->io_thread()));
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

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
void NetErrorTabHelper::OnDownloadPageLater() {
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

void NetErrorTabHelper::OnSetIsShowingDownloadButtonInErrorPage(
    bool is_showing_download_button) {
  is_showing_download_button_in_error_page_ = is_showing_download_button;
}
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

void NetErrorTabHelper::InitializePref(WebContents* contents) {
  DCHECK(contents);

  BrowserContext* browser_context = contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  resolve_errors_with_web_service_.Init(
      prefs::kAlternateErrorPagesEnabled,
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
  content::RenderFrameHost* rfh = web_contents()->GetMainFrame();

  chrome::mojom::NetworkDiagnosticsClientAssociatedPtr client;
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
  RunNetworkDiagnosticsHelper(url.GetOrigin().spec());
}

void NetErrorTabHelper::RunNetworkDiagnosticsHelper(
    const std::string& sanitized_url) {
  if (network_diagnostics_bindings_.GetCurrentTargetFrame()
          != web_contents()->GetMainFrame()) {
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

}  // namespace chrome_browser_net
