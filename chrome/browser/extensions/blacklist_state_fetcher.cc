// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist_state_fetcher.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/safe_browsing/crx_info.pb.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace extensions {

BlacklistStateFetcher::BlacklistStateFetcher() {}

BlacklistStateFetcher::~BlacklistStateFetcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BlacklistStateFetcher::Request(const std::string& id,
                                    const RequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!safe_browsing_config_) {
    if (g_browser_process && g_browser_process->safe_browsing_service()) {
      SetSafeBrowsingConfig(
          g_browser_process->safe_browsing_service()->GetV4ProtocolConfig());
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(callback, BLACKLISTED_UNKNOWN));
      return;
    }
  }

  bool request_already_sent = base::Contains(callbacks_, id);
  callbacks_.insert(std::make_pair(id, callback));
  if (request_already_sent)
    return;

  if (g_browser_process && g_browser_process->safe_browsing_service()) {
    url_loader_factory_ =
        g_browser_process->safe_browsing_service()->GetURLLoaderFactory();
  }

  SendRequest(id);
}

void BlacklistStateFetcher::SendRequest(const std::string& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ClientCRXListInfoRequest request;
  request.set_id(id);
  std::string request_str;
  request.SerializeToString(&request_str);

  GURL request_url = GURL(safe_browsing::GetReportUrl(
      *safe_browsing_config_, "clientreport/crx-list-info"));
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("extension_blacklist", R"(
        semantics {
          sender: "Extension Blacklist"
          description:
            "Chromium protects the users from malicious extensions by checking "
            "extensions that are being installed or have been installed "
            "against a list of known malwares. Chromium sends the identifiers "
            "of extensions to Google and Google responds with whether it "
            "believes each extension is malware or not. Only extensions that "
            "match the safe browsing blacklist can trigger this request."
          trigger:
            "When extensions are being installed and at startup when existing "
            "extensions are scanned."
          data: "The identifier of the installed extension."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing cookies store"
          setting:
            "Users can enable or disable this feature by toggling 'Protect you "
            "and your device from dangerous sites' in Chromium settings under "
            "Privacy. This feature is enabled by default."
          chrome_policy {
            SafeBrowsingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = request_url;
  resource_request->method = "POST";
  std::unique_ptr<network::SimpleURLLoader> fetcher_ptr =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  auto* fetcher = fetcher_ptr.get();
  fetcher->AttachStringForUpload(request_str, "application/octet-stream");
  requests_[fetcher] = {std::move(fetcher_ptr), id};
  fetcher->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&BlacklistStateFetcher::OnURLLoaderComplete,
                     base::Unretained(this), fetcher));
}

void BlacklistStateFetcher::SetSafeBrowsingConfig(
    const safe_browsing::V4ProtocolConfig& config) {
  safe_browsing_config_ =
      std::make_unique<safe_browsing::V4ProtocolConfig>(config);
}

void BlacklistStateFetcher::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();

  std::string response_body_str;
  if (response_body.get())
    response_body_str = std::move(*response_body.get());

  OnURLLoaderCompleteInternal(url_loader, response_body_str, response_code,
                              url_loader->NetError());
}

void BlacklistStateFetcher::OnURLLoaderCompleteInternal(
    network::SimpleURLLoader* url_loader,
    const std::string& response_body,
    int response_code,
    int net_error) {
  auto it = requests_.find(url_loader);
  if (it == requests_.end()) {
    NOTREACHED();
    return;
  }

  std::unique_ptr<network::SimpleURLLoader> loader =
      std::move(it->second.first);
  std::string id = it->second.second;
  requests_.erase(it);

  BlacklistState state;
  if (net_error == net::OK && response_code == 200) {
    ClientCRXListInfoResponse response;
    if (response.ParseFromString(response_body)) {
      state = static_cast<BlacklistState>(response.verdict());
    } else {
      state = BLACKLISTED_UNKNOWN;
    }
  } else {
    if (net_error != net::OK) {
      VLOG(1) << "Blacklist request for: " << id
              << " failed with error: " << net_error;
    } else {
      VLOG(1) << "Blacklist request for: " << id
              << " failed with error: " << response_code;
    }

    state = BLACKLISTED_UNKNOWN;
  }

  std::pair<CallbackMultiMap::iterator, CallbackMultiMap::iterator> range =
      callbacks_.equal_range(id);
  for (CallbackMultiMap::const_iterator callback_it = range.first;
       callback_it != range.second; ++callback_it) {
    callback_it->second.Run(state);
  }

  callbacks_.erase(range.first, range.second);
}

}  // namespace extensions
