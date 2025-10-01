// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/qwac_web_contents_observer.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/link_header_util/link_header_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace {

// TODO(crbug.com/436300890): These limits are arbitrary. Decide if they are
// okay or need tweaking.
constexpr base::TimeDelta k2QwacLoaderTimeout = base::Seconds(15);
constexpr size_t k2QwacMaxSize = 100 * 1024;

// Traffic annotation for RequestDelegate.
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("certificate_verifier_2qwac_loader", R"(
      semantics {
        sender: "Certificate Verifier"
        description:
          "Web pages can use a Qualified certificate for Website "
          "Authentication (QWAC) which is delivered separately from the "
          "TLS connection certificate. See ETSI TS 119 411-5."
        trigger:
          "User visits an HTTPS web page containing a link header with "
          "rel=\"tls-certificate-binding\"."
        data: "None"
        user_data {
          type: NONE
        }
        destination: WEBSITE
        internal {
          contacts {
            email: "chrome-secure-web-and-net@chromium.org"
          }
        }
        last_reviewed: "2025-05-16"
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled in settings."
        policy_exception_justification: "QWAC verification is required."
      })");

void RecordHistogram(QwacWebContentsObserver::QwacLinkProcessingResult result) {
  base::UmaHistogramEnumeration("Net.CertVerifier.Qwac.2QwacLinkProcessing",
                                result);
}

}  // namespace

PAGE_USER_DATA_KEY_IMPL(QwacWebContentsObserver::QwacStatus);

QwacWebContentsObserver::QwacStatus::QwacStatus(
    content::Page& page,
    std::string hostname,
    scoped_refptr<net::X509Certificate> tls_cert,
    GURL qwac_url,
    const url::Origin& initiator,
    mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory)
    : content::PageUserData<QwacStatus>(page),
      hostname_(std::move(hostname)),
      tls_cert_(std::move(tls_cert)) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(qwac_url);
  resource_request->request_initiator = initiator;

  // Set referrer_policy to a safe default. (It shouldn't matter since we set
  // mode to same-origin and we don't set `referrer` anyway.)
  resource_request->referrer_policy =
      net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN;

  // We don't expect that credentials should be needed to fetch the 2-QWAC, so
  // use a safe default.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // The QWAC url must be relative, which we take to mean that it must be
  // served from the same origin, so set same-origin mode to enforce that it
  // doesn't redirect to a different origin.
  resource_request->mode = network::mojom::RequestMode::kSameOrigin;

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  simple_url_loader_->SetTimeoutDuration(k2QwacLoaderTimeout);

  // TODO(crbug.com/436300891): Is it possible to link this request to the
  // initiating request in the netlog? ResourceRequest has
  // `net_log_create_info` and `net_log_reference_info` but the comments say
  // they should only be used from within the network service, and I don't know
  // if the netlog id of the initiating request is even available here.

  // If the loader is destroyed, the callback will be canceled, so using
  // base::Unretained here is safe.
  simple_url_loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&QwacStatus::On2QwacDownloadComplete,
                     base::Unretained(this)),
      k2QwacMaxSize);
}

QwacWebContentsObserver::QwacStatus::~QwacStatus() {
  if (!is_finished_) {
    RecordHistogram(QwacLinkProcessingResult::kDestroyedBeforeFinish);
  }
}

base::CallbackListSubscription
QwacWebContentsObserver::QwacStatus::RegisterCallback(
    CallbackList::CallbackType cb) {
  CHECK(!is_finished());
  return callback_list_.Add(std::move(cb));
}

void QwacWebContentsObserver::QwacStatus::On2QwacDownloadComplete(
    std::optional<std::string> response_body) {
  if (!response_body) {
    RecordHistogram(QwacLinkProcessingResult::kDownloadFailed);
    is_finished_ = true;
    callback_list_.Notify();
    return;
  }

  page()
      .GetMainDocument()
      .GetProcess()
      ->GetStoragePartition()
      ->GetNetworkContext()
      ->Verify2QwacCertBinding(
          *response_body, hostname_, tls_cert_,
          base::BindOnce(&QwacStatus::On2QwacVerificationComplete,
                         weak_ptr_factory_.GetWeakPtr()));
}

void QwacWebContentsObserver::QwacStatus::On2QwacVerificationComplete(
    const scoped_refptr<net::X509Certificate>& verified_2qwac) {
  RecordHistogram(verified_2qwac
                      ? QwacLinkProcessingResult::kValid2Qwac
                      : QwacLinkProcessingResult::k2QwacVerificationFailed);
  is_finished_ = true;
  verified_2qwac_ = verified_2qwac;
  callback_list_.Notify();
}

QwacWebContentsObserver::QwacWebContentsObserver(tabs::TabInterface& tab)
    : content::WebContentsObserver(tab.GetContents()) {
  // Unretained is safe as the callback will be unregistered when the
  // CallbackListSubscription is destroyed.
  tab_subscription_ = tab.RegisterWillDiscardContents(base::BindRepeating(
      &QwacWebContentsObserver::WillDiscardContents, base::Unretained(this)));
}

QwacWebContentsObserver::QwacWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}
QwacWebContentsObserver::~QwacWebContentsObserver() = default;

void QwacWebContentsObserver::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Observe(new_contents);
}

namespace {

bool NavigationHandleHasAcceptableSSLInfo(
    content::NavigationHandle* navigation_handle) {
  // Only TLS connections with a valid certificate are eligible to be a 2-QWAC.
  // ETSI TS 119 411-5 V2.1.1 - 6.2.2.1:
  //   Establish a secure TLS connection with the site using the web browsers'
  //   procedures and configuration ... If this step fails, the procedure
  //   finishes negatively.
  //
  // Also if the connection used a valid 1-QWAC certificate, there's no point
  // to checking for 2-QWACs.
  const std::optional<net::SSLInfo>& ssl_info = navigation_handle->GetSSLInfo();
  return (ssl_info.has_value() && ssl_info->is_valid() &&
          !net::IsCertStatusError(ssl_info->cert_status) &&
          !(ssl_info->cert_status & net::CERT_STATUS_IS_QWAC));
}

}  // namespace

void QwacWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // Ignore non-TLS navigations.
  // Also, navigation might not have actually loaded a resource from the
  // network. The !navigation_handle->IsSameDocument() check above should
  // catch these, but if there are other cases of navigations that don't have
  // the network information, safely ignore those too.
  if (!navigation_handle->GetSSLInfo() ||
      !navigation_handle->GetResponseHeaders()) {
    return;
  }

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  if (!render_frame_host) {
    return;
  }

  content::Page& page = render_frame_host->GetPage();

  // A QwacStatus may have already been created for this page. Don't recreate
  // it if it is still applicable.
  if (QwacStatus* status = QwacStatus::GetForPage(page); status) {
    if (NavigationHandleHasAcceptableSSLInfo(navigation_handle) &&
        navigation_handle->GetSSLInfo()->cert->EqualsIncludingChain(
            status->tls_cert())) {
      // If the page wasn't reloaded, or was reloaded but the certificate
      // didn't change, then we can just reuse the existing QwacStatus.
      RecordHistogram(QwacLinkProcessingResult::kQwacStatusAlreadyPresent);
      return;
    }
    // Otherwise, clear the existing entry and go through the fetching and
    // verification again.
    QwacStatus::DeleteForPage(page);
  }

  if (!NavigationHandleHasAcceptableSSLInfo(navigation_handle)) {
    RecordHistogram(QwacLinkProcessingResult::kUnacceptableSslInfo);
    return;
  }

  // ETSI TS 119 411-5 V2.1.1 - 6.2.2.2:
  //   Examine the HTTP headers included in any main frame navigation response
  //   from the server (relating to navigation by the web browser to the
  //   address as displayed in the address bar) for a HTTP 'Link' response
  //   header (as defined in IETF RFC 8288 [6]) with a rel value of
  //   tls-certificate-binding.
  std::optional<std::string> link_header =
      navigation_handle->GetResponseHeaders()->GetNormalizedHeader("link");
  if (!link_header.has_value()) {
    RecordHistogram(QwacLinkProcessingResult::kNoQwacLinkHeader);
    return;
  }

  std::string qwac_binding_url;
  for (const auto& value : link_header_util::SplitLinkHeader(*link_header)) {
    std::unordered_map<std::string, std::optional<std::string>> params;
    std::optional<std::string> link_url =
        link_header_util::ParseLinkHeaderValue(value, params);
    if (!link_url) {
      continue;
    }

    auto rel = params.find("rel");
    if (rel == params.end()) {
      continue;
    }
    if (!rel->second) {
      continue;
    }
    if (rel->second == "tls-certificate-binding") {
      qwac_binding_url = *link_url;
      break;
    }
  }

  if (qwac_binding_url.empty()) {
    RecordHistogram(QwacLinkProcessingResult::kNoQwacLinkHeader);
    return;
  }

  GURL full_qwac_url = navigation_handle->GetURL().Resolve(qwac_binding_url);
  if (!full_qwac_url.is_valid()) {
    RecordHistogram(QwacLinkProcessingResult::kInvalidQwacLinkHeader);
    return;
  }

  // The 2-QWAC url must be relative:
  // ETSI TS 119 411-5 V2.1.1 - 5.2:
  //   When using a 2-QWAC, website operators shall:
  //   ...
  //   Configure their website to serve: an HTTP 'Link' response header (as
  //   defined in IETF RFC 8288 [6]) with a relative reference to the TLS
  //   Certificate Binding, and a rel value of tls-certificate-binding
  if (url::SchemeHostPort(full_qwac_url) !=
      url::SchemeHostPort(navigation_handle->GetURL())) {
    RecordHistogram(QwacLinkProcessingResult::kNonrelativeQwacLinkUrl);
    return;
  }

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
  render_frame_host->CreateNetworkServiceDefaultFactory(
      url_loader_factory.BindNewPipeAndPassReceiver());

  QwacStatus::CreateForPage(
      page, navigation_handle->GetURL().GetHost(),
      navigation_handle->GetSSLInfo()->cert, std::move(full_qwac_url),
      /*initiator=*/render_frame_host->GetLastCommittedOrigin(),
      std::move(url_loader_factory));
}
