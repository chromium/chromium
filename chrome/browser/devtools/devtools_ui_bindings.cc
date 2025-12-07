// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_ui_bindings.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>
#include <variant>

#include "aida_client.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/aida_service_handler.h"
#include "chrome/browser/devtools/devtools_file_watcher.h"
#include "chrome/browser/devtools/devtools_http_service_registry.h"
#include "chrome/browser/devtools/devtools_select_file_dialog.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/devtools/url_constants.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/features.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/permissions/permission_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/search_engines/util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/zoom/page_zoom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "google_apis/google_api_keys.h"
#include "ipc/constants.mojom.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_management.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/devtools_page_handler.h"
#include "extensions/common/permissions/permissions_data.h"
#endif

using content::BrowserThread;

namespace content {
struct LoadCommittedDetails;
struct FrameNavigateParams;
}  // namespace content

namespace {

const char kFrontendHostId[] = "id";
const char kFrontendHostMethod[] = "method";
const char kFrontendHostParams[] = "params";
const char kTitleFormat[] = "DevTools - %s";

const char kConfigDiscoverUsbDevices[] = "discoverUsbDevices";
const char kConfigPortForwardingEnabled[] = "portForwardingEnabled";
const char kConfigPortForwardingConfig[] = "portForwardingConfig";
const char kConfigNetworkDiscoveryEnabled[] = "networkDiscoveryEnabled";
const char kConfigNetworkDiscoveryConfig[] = "networkDiscoveryConfig";

// This constant should be in sync with
// the constant
// kShellMaxMessageChunkSize in content/shell/browser/shell_devtools_bindings.cc
// and
// kLayoutTestMaxMessageChunkSize in
// content/shell/browser/layout_test/devtools_protocol_test_bindings.cc.
const size_t kMaxMessageChunkSize = IPC::mojom::kChannelMaximumMessageSize / 4;

base::Value::Dict CreateFileSystemValue(
    DevToolsFileHelper::FileSystem file_system) {
  base::Value::Dict file_system_value;
  file_system_value.Set("type", file_system.type);
  file_system_value.Set("fileSystemName", file_system.file_system_name);
  file_system_value.Set("rootURL", file_system.root_url);
  file_system_value.Set("fileSystemPath", file_system.file_system_path);
  return file_system_value;
}

// DevToolsUIDefaultDelegate --------------------------------------------------

class DefaultBindingsDelegate : public DevToolsUIBindings::Delegate {
 public:
  explicit DefaultBindingsDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  DefaultBindingsDelegate(const DefaultBindingsDelegate&) = delete;
  DefaultBindingsDelegate& operator=(const DefaultBindingsDelegate&) = delete;

 private:
  ~DefaultBindingsDelegate() override = default;

  content::WebContents* GetInspectedWebContents() override { return nullptr; }
  void ActivateWindow() override;
  void CloseWindow() override {}
  void Inspect(scoped_refptr<content::DevToolsAgentHost> host) override {}
  void SetInspectedPageBounds(const gfx::Rect& rect) override {}
  void InspectElementCompleted() override {}
  void SetIsDocked(bool is_docked) override {}
  void OpenInNewTab(const std::string& url) override;
  void OpenSearchResultsInNewTab(const std::string& query) override;
  void SetWhitelistedShortcuts(const std::string& message) override {}
  void SetEyeDropperActive(bool active) override {}
  void OpenNodeFrontend() override {}
  using DispatchCallback =
      DevToolsEmbedderMessageDispatcher::Delegate::DispatchCallback;

  void InspectedContentsClosing() override;
  void OnLoadCompleted() override {}
  void ReadyForTest() override {}
  void ConnectionReady() override {}
  void SetOpenNewWindowForPopups(bool value) override {}
  infobars::ContentInfoBarManager* GetInfoBarManager() override;
  void RenderProcessGone(bool crashed) override {}
  void ShowCertificateViewer(const std::string& cert_chain) override {}

  int GetDockStateForLogging() override { return 0; }
  int GetOpenedByForLogging() override { return 0; }
  int GetClosedByForLogging() override { return 0; }
  raw_ptr<content::WebContents> web_contents_;
};

void DefaultBindingsDelegate::ActivateWindow() {
  web_contents_->GetDelegate()->ActivateContents(web_contents_);
  web_contents_->Focus();
}

void DefaultBindingsDelegate::OpenInNewTab(const std::string& url) {
  content::OpenURLParams params(GURL(url), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  // Check if the browser is still alive, as it might have been closed in the
  // meantime.
  // TODO(https://crbug.com/403946437): We should definitely understand why this
  // happens.
  if (browser) {
    browser->OpenURL(params, /*navigation_handle_callback=*/{});
  }
#endif
}

void DefaultBindingsDelegate::OpenSearchResultsInNewTab(
    const std::string& query) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(browser->profile());
  DCHECK(url_service);
  GURL url =
      GetDefaultSearchURLForSearchTerms(url_service, base::UTF8ToUTF16(query));
  content::OpenURLParams params(GURL(url), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  browser->OpenURL(params, /*navigation_handle_callback=*/{});
#endif
}

void DefaultBindingsDelegate::InspectedContentsClosing() {
  web_contents_->ClosePage();
}

infobars::ContentInfoBarManager* DefaultBindingsDelegate::GetInfoBarManager() {
  return infobars::ContentInfoBarManager::FromWebContents(web_contents_);
}

base::Value::Dict BuildObjectForResponse(const net::HttpResponseHeaders* rh,
                                         bool success,
                                         int net_error) {
  base::Value::Dict response;
  int responseCode = 200;
  if (rh) {
    responseCode = rh->response_code();
  } else if (!success) {
    // In case of no headers, assume file:// URL and failed to load
    responseCode = 404;
  }
  response.Set("statusCode", responseCode);
  response.Set("netError", net_error);
  response.Set("netErrorName", net::ErrorToString(net_error));

  base::Value::Dict headers;
  size_t iterator = 0;
  std::string name;
  std::string value;
  // TODO(caseq): this probably needs to handle duplicate header names
  // correctly by folding them.
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value)) {
    headers.Set(name, value);
  }

  response.Set("headers", std::move(headers));
  return response;
}

GURL SanitizeFrontendURL(const GURL& url,
                         const std::string& scheme,
                         const std::string& host,
                         const std::string& path,
                         bool allow_query_and_fragment);

std::string SanitizeRevision(const std::string& revision) {
  for (size_t i = 0; i < revision.length(); i++) {
    if (!(revision[i] == '@' && i == 0) &&
        !(revision[i] >= '0' && revision[i] <= '9') &&
        !(revision[i] >= 'a' && revision[i] <= 'z') &&
        !(revision[i] >= 'A' && revision[i] <= 'Z')) {
      return std::string();
    }
  }
  return revision;
}

std::string SanitizeRemoteVersion(const std::string& remoteVersion) {
  for (size_t i = 0; i < remoteVersion.length(); i++) {
    if (remoteVersion[i] != '.' &&
        !(remoteVersion[i] >= '0' && remoteVersion[i] <= '9')) {
      return std::string();
    }
  }
  return remoteVersion;
}

std::string SanitizeFrontendPath(const std::string& path) {
  for (size_t i = 0; i < path.length(); i++) {
    if (path[i] != '/' && path[i] != '-' && path[i] != '_' && path[i] != '.' &&
        path[i] != '@' && !(path[i] >= '0' && path[i] <= '9') &&
        !(path[i] >= 'a' && path[i] <= 'z') &&
        !(path[i] >= 'A' && path[i] <= 'Z')) {
      return std::string();
    }
  }
  return path;
}

std::string SanitizeEndpoint(const std::string& value) {
  if (value.find('&') != std::string::npos ||
      value.find('?') != std::string::npos) {
    return std::string();
  }
  return value;
}

std::string SanitizeRemoteBase(const std::string& value) {
  GURL url(value);
  std::string path = url.GetPath();
  std::vector<std::string> parts =
      base::SplitString(path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string revision = parts.size() > 2 ? parts[2] : "";
  revision = SanitizeRevision(revision);
  path = base::StringPrintf("/%s/%s/", kRemoteFrontendPath, revision.c_str());
  return SanitizeFrontendURL(url, url::kHttpsScheme, kRemoteFrontendDomain,
                             path, false)
      .spec();
}

std::string SanitizeRemoteFrontendURL(const std::string& value) {
  GURL url(base::UnescapeBinaryURLComponent(
      value, base::UnescapeRule::REPLACE_PLUS_WITH_SPACE));
  std::string path = url.GetPath();
  std::vector<std::string> parts =
      base::SplitString(path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string revision = parts.size() > 2 ? parts[2] : "";
  revision = SanitizeRevision(revision);
  std::string filename = !parts.empty() ? parts[parts.size() - 1] : "";
  if (filename != "devtools.html") {
    filename = "inspector.html";
  }
  path = base::StringPrintf("/serve_rev/%s/%s", revision.c_str(),
                            filename.c_str());
  std::string sanitized = SanitizeFrontendURL(url, url::kHttpsScheme,
                                              kRemoteFrontendDomain, path, true)
                              .spec();
  return base::EscapeQueryParamValue(sanitized, false);
}

std::string SanitizeEnabledExperiments(const std::string& value) {
  const auto is_legal = [](char ch) {
    return base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch) || ch == ';' ||
           ch == '_';
  };
  return std::ranges::all_of(value, is_legal) ? value : std::string();
}

std::string SanitizeTraceURL(const std::string& value) {
  if (base::StartsWith(value, "http") &&
      (base::EndsWith(value, ".json") || base::EndsWith(value, ".json.gz"))) {
    return value;
  }

  return std::string();
}

std::string SanitizeFrontendQueryParam(const std::string& key,
                                       const std::string& value) {
  // Convert boolean flags to true.
  if (key == "can_dock" || key == "debugFrontend" || key == "isSharedWorker" ||
      key == "v8only" || key == "remoteFrontend" || key == "nodeFrontend" ||
      key == "hasOtherClients" || key == "uiDevTools" ||
      key == "browserConnection") {
    return "true";
  }

  // Pass connection endpoints as is.
  if (key == "ws" || key == "service-backend") {
    return SanitizeEndpoint(value);
  }

  if (key == "panel" &&
      (value == "elements" || value == "console" || value == "sources" ||
       value == "network" || value == "resources" || value == "performance")) {
    return value;
  }

  if (key == "remoteBase") {
    return SanitizeRemoteBase(value);
  }

  if (key == "remoteFrontendUrl") {
    return SanitizeRemoteFrontendURL(value);
  }

  if (key == "remoteVersion") {
    return SanitizeRemoteVersion(value);
  }

  if (key == "enabledExperiments") {
    return SanitizeEnabledExperiments(value);
  }

  if (key == "traceURL") {
    return SanitizeTraceURL(value);
  }

  if (key == "targetType" && value == "tab") {
    return value;
  }

  if (key == "noJavaScriptCompletion" && value == "true") {
    return value;
  }

  if (key == "veLogging" && value == "true") {
    return value;
  }

  if (key == "isChromeForTesting" && value == "true") {
    return value;
  }

  if (key == "disableSelfXssWarnings" && value == "true") {
    return value;
  }

  return std::string();
}

GURL SanitizeFrontendURL(const GURL& url,
                         const std::string& scheme,
                         const std::string& host,
                         const std::string& path,
                         bool allow_query_and_fragment) {
  std::vector<std::string> query_parts;
  std::string fragment;
  if (allow_query_and_fragment) {
    for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
      const std::string key = std::string(it.GetKey());
      std::string value =
          SanitizeFrontendQueryParam(key, std::string(it.GetValue()));
      if (!value.empty()) {
        query_parts.push_back(
            base::StringPrintf("%s=%s", key.c_str(), value.c_str()));
      }
    }
    if (url.has_ref() && url.ref().find('\'') == std::string_view::npos) {
      fragment = '#' + url.GetRef();
    }
  }
  std::string query =
      query_parts.empty() ? "" : "?" + base::JoinString(query_parts, "&");
  std::string constructed =
      base::StringPrintf("%s://%s%s%s%s", scheme.c_str(), host.c_str(),
                         path.c_str(), query.c_str(), fragment.c_str());
  GURL result = GURL(constructed);
  if (!result.is_valid()) {
    return GURL();
  }
  return result;
}

constexpr base::TimeDelta kInitialBackoffDelay = base::Milliseconds(250);
constexpr base::TimeDelta kMaxBackoffDelay = base::Seconds(10);

}  // namespace

class DevToolsUIBindings::NetworkResourceLoader
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  class URLLoaderFactoryHolder {
   public:
    network::mojom::URLLoaderFactory* get() {
      return ptr_.get() ? ptr_.get() : refptr_.get();
    }
    void operator=(std::unique_ptr<network::mojom::URLLoaderFactory>&& ptr) {
      ptr_ = std::move(ptr);
    }
    void operator=(scoped_refptr<network::SharedURLLoaderFactory>&& refptr) {
      refptr_ = std::move(refptr);
    }

   private:
    std::unique_ptr<network::mojom::URLLoaderFactory> ptr_;
    scoped_refptr<network::SharedURLLoaderFactory> refptr_;
  };

  static void Create(int stream_id,
                     DevToolsUIBindings* bindings,
                     const network::ResourceRequest& resource_request,
                     const net::NetworkTrafficAnnotationTag& traffic_annotation,
                     URLLoaderFactoryHolder url_loader_factory,
                     DevToolsUIBindings::DispatchCallback callback,
                     base::TimeDelta retry_delay = base::TimeDelta(),
                     std::string post_body = "",
                     std::optional<base::TimeDelta> timeout = std::nullopt) {
    auto resource_loader =
        std::make_unique<DevToolsUIBindings::NetworkResourceLoader>(
            stream_id, bindings, resource_request, traffic_annotation,
            std::move(url_loader_factory), std::move(callback), retry_delay,
            post_body, timeout);
    bindings->loaders_.insert(std::move(resource_loader));
  }

  NetworkResourceLoader(
      int stream_id,
      DevToolsUIBindings* bindings,
      const network::ResourceRequest& resource_request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      URLLoaderFactoryHolder url_loader_factory,
      DispatchCallback callback,
      base::TimeDelta delay,
      std::string post_body,
      std::optional<base::TimeDelta> timeout)
      : stream_id_(stream_id),
        bindings_(bindings),
        resource_request_(resource_request),
        traffic_annotation_(traffic_annotation),
        loader_(network::SimpleURLLoader::Create(
            std::make_unique<network::ResourceRequest>(resource_request),
            traffic_annotation)),
        url_loader_factory_(std::move(url_loader_factory)),
        callback_(std::move(callback)),
        retry_delay_(delay) {
    if (timeout.has_value()) {
      loader_->SetTimeoutDuration(timeout.value());
    }
    if (!post_body.empty()) {
      loader_->AttachStringForUpload(std::move(post_body));
    }
    loader_->SetOnResponseStartedCallback(base::BindOnce(
        &NetworkResourceLoader::OnResponseStarted, base::Unretained(this)));
    timer_.Start(FROM_HERE, delay,
                 base::BindOnce(&NetworkResourceLoader::DownloadAsStream,
                                base::Unretained(this)));
  }

  NetworkResourceLoader(const NetworkResourceLoader&) = delete;
  NetworkResourceLoader& operator=(const NetworkResourceLoader&) = delete;

  static base::TimeDelta GetNextExponentialBackoffDelay(
      const base::TimeDelta& delta) {
    if (delta.is_zero()) {
      return kInitialBackoffDelay;
    } else {
      return delta * 1.3;
    }
  }

 private:
  void DownloadAsStream() {
    loader_->DownloadAsStream(url_loader_factory_.get(), this);
  }

  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head) {
    response_headers_ = response_head.headers;
  }

  void OnDataReceived(std::string_view chunk,
                      base::OnceClosure resume) override {
    base::Value chunkValue;

    bool encoded = !base::IsStringUTF8AllowingNoncharacters(chunk);
    if (encoded) {
      chunkValue = base::Value(base::Base64Encode(chunk));
    } else {
      chunkValue = base::Value(chunk);
    }

    bindings_->CallClientMethod("DevToolsAPI", "streamWrite",
                                base::Value(stream_id_), std::move(chunkValue),
                                base::Value(encoded));
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    if (!success && loader_->NetError() == net::ERR_INSUFFICIENT_RESOURCES &&
        retry_delay_ < kMaxBackoffDelay) {
      const base::TimeDelta delay =
          GetNextExponentialBackoffDelay(retry_delay_);
      LOG(WARNING) << "DevToolsUIBindings::NetworkResourceLoader id = "
                   << stream_id_
                   << " failed with insufficient resources, retrying in "
                   << delay << "." << std::endl;
      NetworkResourceLoader::Create(
          stream_id_, bindings_, resource_request_, traffic_annotation_,
          std::move(url_loader_factory_), std::move(callback_), delay);
    } else {
      auto response = base::Value(BuildObjectForResponse(
          response_headers_.get(), success, loader_->NetError()));
      std::move(callback_).Run(&response);
    }
    bindings_->loaders_.erase(bindings_->loaders_.find(this));
  }

  void OnRetry(base::OnceClosure start_retry) override { NOTREACHED(); }

  const int stream_id_;
  const raw_ptr<DevToolsUIBindings> bindings_;
  const network::ResourceRequest resource_request_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  URLLoaderFactoryHolder url_loader_factory_;
  DispatchCallback callback_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  base::OneShotTimer timer_;
  base::TimeDelta retry_delay_;
};

// DevToolsUIBindings::FrontendWebContentsObserver ----------------------------

class DevToolsUIBindings::FrontendWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit FrontendWebContentsObserver(DevToolsUIBindings* ui_bindings);

  FrontendWebContentsObserver(const FrontendWebContentsObserver&) = delete;
  FrontendWebContentsObserver& operator=(const FrontendWebContentsObserver&) =
      delete;

  ~FrontendWebContentsObserver() override;

 private:
  // contents::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void PrimaryPageChanged(content::Page& page) override;

  raw_ptr<DevToolsUIBindings> devtools_bindings_;
};

DevToolsUIBindings::FrontendWebContentsObserver::FrontendWebContentsObserver(
    DevToolsUIBindings* devtools_ui_bindings)
    : WebContentsObserver(devtools_ui_bindings->web_contents()),
      devtools_bindings_(devtools_ui_bindings) {}

DevToolsUIBindings::FrontendWebContentsObserver::
    ~FrontendWebContentsObserver() = default;

// static
GURL DevToolsUIBindings::SanitizeFrontendURL(const GURL& url) {
  return ::SanitizeFrontendURL(url, content::kChromeDevToolsScheme,
                               chrome::kChromeUIDevToolsHost,
                               SanitizeFrontendPath(url.GetPath()), true);
}

// static
bool DevToolsUIBindings::IsValidFrontendURL(const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme) &&
      url.GetHost() == content::kChromeUITracingHost && !url.has_query() &&
      !url.has_ref()) {
    return true;
  }

  return SanitizeFrontendURL(url).spec() == url.spec();
}

bool DevToolsUIBindings::IsValidRemoteFrontendURL(const GURL& url) {
  return ::SanitizeFrontendURL(url, url::kHttpsScheme, kRemoteFrontendDomain,
                               url.GetPath(), true)
             .spec() == url.spec();
}

void DevToolsUIBindings::FrontendWebContentsObserver::
    PrimaryMainFrameRenderProcessGone(base::TerminationStatus status) {
  bool crashed = true;
  switch (status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
#if BUILDFLAG(IS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
    case base::TERMINATION_STATUS_OOM:
    case base::TERMINATION_STATUS_EVICTED_FOR_MEMORY:
#if BUILDFLAG(IS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
#endif
      if (devtools_bindings_->agent_host_.get()) {
        devtools_bindings_->Detach();
      }
      break;
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
#if BUILDFLAG(IS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
    case base::TERMINATION_STATUS_MAX_ENUM:
      crashed = false;
      break;
  }
  devtools_bindings_->delegate_->RenderProcessGone(crashed);
}

void DevToolsUIBindings::FrontendWebContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  devtools_bindings_->ReadyToCommitNavigation(navigation_handle);
}

void DevToolsUIBindings::FrontendWebContentsObserver::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  devtools_bindings_->DocumentOnLoadCompletedInPrimaryMainFrame();
}

void DevToolsUIBindings::FrontendWebContentsObserver::PrimaryPageChanged(
    content::Page&) {
  devtools_bindings_->PrimaryPageChanged();
}

// DevToolsUIBindings ---------------------------------------------------------

DevToolsUIBindings* DevToolsUIBindings::ForWebContents(
    content::WebContents* web_contents) {
  DevToolsUIBindingsList& instances =
      DevToolsUIBindings::GetDevToolsUIBindings();
  auto binding = std::find_if(
      instances.rbegin(), instances.rend(),
      [&](auto binding) { return binding->web_contents() == web_contents; });
  return binding == instances.rend() ? nullptr : *binding;
}

std::string DevToolsUIBindings::GetTypeForMetrics() {
  return "DevTools";
}

namespace {
bool IsAnyAidaPoweredFeatureEnabled() {
  return base::FeatureList::IsEnabled(::features::kDevToolsConsoleInsights) ||
         base::FeatureList::IsEnabled(::features::kDevToolsFreestyler) ||
         base::FeatureList::IsEnabled(
             ::features::kDevToolsAiAssistanceFileAgent) ||
         base::FeatureList::IsEnabled(
             ::features::kDevToolsAiAssistanceNetworkAgent) ||
         base::FeatureList::IsEnabled(
             ::features::kDevToolsAiAssistancePerformanceAgent) ||
         base::FeatureList::IsEnabled(
             ::features::kDevToolsAiCodeCompletion) ||
         base::FeatureList::IsEnabled(::features::kDevToolsAiCodeGeneration);
}
}  // namespace

DevToolsUIBindings::DevToolsUIBindings(content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      delegate_(new DefaultBindingsDelegate(web_contents_)),
      file_storage_(web_contents),
      file_helper_(profile_, this, &file_storage_),
      devices_updates_enabled_(false),
      frontend_loaded_(false),
      settings_(profile_),
      http_service_registry_(std::make_unique<DevToolsHttpServiceRegistry>()) {
  DevToolsUIBindings::GetDevToolsUIBindings().push_back(this);
  frontend_contents_observer_ =
      std::make_unique<FrontendWebContentsObserver>(this);

  file_system_indexer_ = new DevToolsFileSystemIndexer();

  // Register on-load actions.
  embedder_message_dispatcher_ =
      DevToolsEmbedderMessageDispatcher::CreateForDevToolsFrontend(this);
#if !BUILDFLAG(IS_ANDROID)
  ThemeServiceFactory::GetForProfile(profile_->GetOriginalProfile())
      ->AddObserver(this);
#endif
  can_access_aida_ = IsAnyAidaPoweredFeatureEnabled();
}

DevToolsUIBindings::~DevToolsUIBindings() {
  if (!session_id_for_logging_.is_empty()) {
    metrics::structured::StructuredMetricsClient::Record(
        metrics::structured::events::v2::dev_tools::SessionEnd()
            .SetTrigger(delegate_->GetClosedByForLogging())
            .SetTimeSinceSessionStart(
                GetTimeSinceSessionStart().InMilliseconds())
            .SetSessionId(session_id_for_logging_.GetLowForSerialization()));
  }
#if !BUILDFLAG(IS_ANDROID)
  ThemeServiceFactory::GetForProfile(profile_->GetOriginalProfile())
      ->RemoveObserver(this);
#endif

  if (agent_host_.get()) {
    agent_host_->DetachClient(this);
  }

  for (IndexingJobsMap::const_iterator jobs_it(indexing_jobs_.begin());
       jobs_it != indexing_jobs_.end(); ++jobs_it) {
    jobs_it->second->Stop();
  }
  indexing_jobs_.clear();
  SetDevicesUpdatesEnabled(false);

  // Remove self from global list.
  DevToolsUIBindingsList& instances =
      DevToolsUIBindings::GetDevToolsUIBindings();
  auto it = std::ranges::find(instances, this);
  CHECK(it != instances.end());
  instances.erase(it);
}

// content::DevToolsFrontendHost::Delegate implementation ---------------------
void DevToolsUIBindings::HandleMessageFromDevToolsFrontend(
    base::Value::Dict message) {
  if (!frontend_host_) {
    return;
  }
  const std::string* method = message.FindString(kFrontendHostMethod);
  base::Value* params = message.Find(kFrontendHostParams);
  if (!method || (params && !params->is_list())) {
    LOG(ERROR) << "Invalid message was sent to embedder: " << message;
    return;
  }
  int id = message.FindInt(kFrontendHostId).value_or(0);
  base::Value::List params_list;
  if (params) {
    params_list = std::move(*params).TakeList();
  }
  embedder_message_dispatcher_->Dispatch(
      base::BindOnce(&DevToolsUIBindings::SendMessageAck,
                     weak_factory_.GetWeakPtr(), id),
      *method, params_list);
}

// content::DevToolsAgentHostClient implementation --------------------------
// There is a sibling implementation of DevToolsAgentHostClient in
//   content/shell/browser/shell_devtools_bindings.cc
// that is used in layout tests, which only use content_shell.
// The two implementations needs to be kept in sync wrt. the interface they
// provide to the DevTools front-end.

void DevToolsUIBindings::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  DCHECK(agent_host == agent_host_.get());
  if (!frontend_host_) {
    return;
  }

  std::string_view message_sp(reinterpret_cast<const char*>(message.data()),
                              message.size());
  if (message_sp.length() < kMaxMessageChunkSize) {
    CallClientMethod("DevToolsAPI", "dispatchMessage", base::Value(message_sp));
    return;
  }

  for (size_t pos = 0; pos < message_sp.length(); pos += kMaxMessageChunkSize) {
    base::Value message_value(message_sp.substr(pos, kMaxMessageChunkSize));
    if (pos == 0) {
      CallClientMethod("DevToolsAPI", "dispatchMessageChunk",
                       std::move(message_value),
                       base::Value(static_cast<int>(message_sp.length())));

    } else {
      CallClientMethod("DevToolsAPI", "dispatchMessageChunk",
                       std::move(message_value));
    }
  }
}

void DevToolsUIBindings::AgentHostClosed(
    content::DevToolsAgentHost* agent_host) {
  DCHECK(agent_host == agent_host_.get());
  agent_host_.reset();
  delegate_->InspectedContentsClosing();
}

bool DevToolsUIBindings::MayWriteLocalFiles() {
  // Do not allow local file system access via the front-end on Chrome OS.
#if BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

void DevToolsUIBindings::SendMessageAck(int request_id,
                                        const base::Value* arg) {
  if (arg) {
    CallClientMethod("DevToolsAPI", "embedderMessageAck",
                     base::Value(request_id), arg->Clone());
  } else {
    CallClientMethod("DevToolsAPI", "embedderMessageAck",
                     base::Value(request_id));
  }
}

void DevToolsUIBindings::InnerAttach() {
  DCHECK(agent_host_.get());
  // TODO(dgozman): handle return value of AttachClient.
  agent_host_->AttachClient(this);
}

// DevToolsEmbedderMessageDispatcher::Delegate implementation -----------------

void DevToolsUIBindings::ActivateWindow() {
  delegate_->ActivateWindow();
}

void DevToolsUIBindings::CloseWindow() {
  delegate_->CloseWindow();
}

void DevToolsUIBindings::LoadCompleted() {
  FrontendLoaded();

#if BUILDFLAG(IS_ANDROID)
  // On Android we don't support showing menus with custom menu info provided
  // by blink::ContextMenuProvider. Use the soft menu to work around it.
  CallClientMethod("DevToolsAPI", "setUseSoftMenu", base::Value(true));
#endif  // BUILDFLAG(IS_ANDROID)
}

void DevToolsUIBindings::SetInspectedPageBounds(const gfx::Rect& rect) {
  delegate_->SetInspectedPageBounds(rect);
}

void DevToolsUIBindings::SetIsDocked(DispatchCallback callback,
                                     bool dock_requested) {
  delegate_->SetIsDocked(dock_requested);
  std::move(callback).Run(nullptr);
}

void DevToolsUIBindings::HandleAidaRequestError(
    DispatchCallback callback,
    std::variant<network::ResourceRequest, std::string>
        resource_request_or_error) {
  base::Value::Dict response_dict;
  response_dict.Set("response",
                    std::get<std::string>(resource_request_or_error));
  auto response_value = base::Value(std::move(response_dict));
  std::move(callback).Run(&response_value);
}

void DevToolsUIBindings::OnAidaConversationRequest(
    DispatchCallback callback,
    int stream_id,
    const std::string& request,
    base::TimeDelta delay,
    std::variant<network::ResourceRequest, std::string>
        resource_request_or_error) {
  if (std::holds_alternative<std::string>(resource_request_or_error)) {
    HandleAidaRequestError(std::move(callback),
                           std::move(resource_request_or_error));
    return;
  }
  DevToolsUIBindings::NetworkResourceLoader::URLLoaderFactoryHolder
      url_loader_factory;
  url_loader_factory = profile_->GetDefaultStoragePartition()
                           ->GetURLLoaderFactoryForBrowserProcess();
  auto resource_request =
      std::get<network::ResourceRequest>(resource_request_or_error);
  resource_request.url = GURL(AidaClient::kDoConversationUrl);
  // Set a maximum timeout value, individual features may send a shorter timeout
  // in the DevTools repo.
  base::TimeDelta timeout = base::Seconds(120);
  auto response_handler_callback =
      base::BindOnce(&DevToolsUIBindings::OnAidaConversationResponse,
                     base::Unretained(this), std::move(callback), stream_id,
                     request, delay, resource_request, base::TimeTicks::Now());
  NetworkResourceLoader::Create(
      stream_id, this, resource_request,
      AidaServiceHandler::TrafficAnnotation(), std::move(url_loader_factory),
      std::move(response_handler_callback), delay, std::move(request), timeout);
}

void DevToolsUIBindings::OnAidaRequest(
    const GURL& url,
    const std::string& response_histogram_name,
    DispatchCallback callback,
    const std::string& request,
    std::variant<network::ResourceRequest, std::string>
        resource_request_or_error) {
  if (std::holds_alternative<std::string>(resource_request_or_error)) {
    HandleAidaRequestError(std::move(callback),
                           std::move(resource_request_or_error));
    return;
  }
  auto url_loader_factory = profile_->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  auto resource_request = std::make_unique<network::ResourceRequest>(
      std::get<network::ResourceRequest>(resource_request_or_error));
  resource_request->url = url;
  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), AidaServiceHandler::TrafficAnnotation());
  simple_url_loader->AttachStringForUpload(request);

  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();
  simple_url_loader_ptr->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&DevToolsUIBindings::OnAidaResponse,
                     base::Unretained(this), response_histogram_name,
                     std::move(callback), std::move(simple_url_loader),
                     base::TimeTicks::Now()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void DevToolsUIBindings::OnAidaConversationResponse(
    DispatchCallback callback,
    int stream_id,
    const std::string request,
    base::TimeDelta delay,
    std::variant<network::ResourceRequest, std::string>
        resource_request_or_error,
    base::TimeTicks start_time,
    const base::Value* response) {
  int response_code =
      response->is_dict()
          ? response->GetDict().FindInt("statusCode").value_or(0)
          : 0;

  if (response_code >= 500 || response_code == net::HTTP_TOO_MANY_REQUESTS) {
    OnAidaConversationRequest(
        std::move(callback), stream_id, request,
        NetworkResourceLoader::GetNextExponentialBackoffDelay(delay),
        resource_request_or_error);
  } else if (response_code == net::HTTP_UNAUTHORIZED) {
    aida_client_->RemoveAccessToken();
    aida_client_->PrepareRequestOrFail(base::BindOnce(
        &DevToolsUIBindings::OnAidaConversationRequest, base::Unretained(this),
        std::move(callback), stream_id, request,
        NetworkResourceLoader::GetNextExponentialBackoffDelay(delay)));
  } else {
    base::UmaHistogramTimes("DevTools.AidaResponseTime",
                            base::TimeTicks::Now() - start_time);
    std::move(callback).Run(response);
  }
}

void DevToolsUIBindings::OnAidaResponse(
    const std::string& histogram_name,
    DispatchCallback callback,
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
    base::TimeTicks start_time,
    std::optional<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader->ResponseInfo() &&
      simple_url_loader->ResponseInfo()->headers) {
    response_code = simple_url_loader->ResponseInfo()->headers->response_code();
  }
  if (response_code != net::HTTP_OK) {
    base::Value::Dict error_dict;
    error_dict.Set("error", "Got error response from AIDA");
    error_dict.Set("detail", response_body.value_or(""));
    auto error = base::Value(std::move(error_dict));
    std::move(callback).Run(&error);
    return;
  }

  base::Value::Dict response_dict;
  response_dict.Set("response", response_body.value_or(""));
  auto response = base::Value(std::move(response_dict));
  base::UmaHistogramTimes(histogram_name, base::TimeTicks::Now() - start_time);
  std::move(callback).Run(&response);
}

void DevToolsUIBindings::DispatchHttpRequest(
    DispatchCallback callback,
    const DevToolsDispatchHttpRequestParams& params) {
  http_service_registry_->Request(
      profile_, params,
      base::BindOnce(&DevToolsUIBindings::OnHttpRequestPerformed,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DevToolsUIBindings::OnHttpRequestPerformed(
    DispatchCallback callback,
    std::unique_ptr<DevToolsHttpServiceHandler::Result> result) {
  base::Value::Dict response_dict;
  using Error = DevToolsHttpServiceHandler::Result::Error;
  switch (result->error) {
    case Error::kNone:
      response_dict.Set("response", result->response_body.value_or(""));
      response_dict.Set("statusCode", result->http_status);
      break;
    case Error::kServiceNotFound:
      response_dict.Set("error", "Service not found");
      break;
    case Error::kAccessDenied:
      response_dict.Set("error", "Disallowed path or method");
      break;
    case Error::kValidationFailed:
      response_dict.Set("error", "Request validation failed");
      break;
    case Error::kTokenFetchFailed:
      response_dict.Set("error", "Token fetch error");
      response_dict.Set("detail", result->error_detail);
      break;
    case Error::kNetworkError:
    case Error::kHttpError:
      response_dict.Set("error", "Request failed");
      response_dict.Set("detail", result->response_body.value_or(""));
      response_dict.Set("netError", result->net_error);
      response_dict.Set("netErrorName", net::ErrorToString(result->net_error));
      response_dict.Set("statusCode", result->http_status);
      break;
  }
  auto response = base::Value(std::move(response_dict));
  std::move(callback).Run(&response);
}

void DevToolsUIBindings::InspectElementCompleted() {
  delegate_->InspectElementCompleted();
}

void DevToolsUIBindings::InspectedURLChanged(const std::string& url) {
  content::NavigationController& controller = web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();

  const std::string kHttpPrefix = "http://";
  const std::string kHttpsPrefix = "https://";
  const std::string simplified_url =
      base::StartsWith(url, kHttpsPrefix, base::CompareCase::SENSITIVE)
          ? url.substr(kHttpsPrefix.length())
      : base::StartsWith(url, kHttpPrefix, base::CompareCase::SENSITIVE)
          ? url.substr(kHttpPrefix.length())
          : url;
  // DevTools UI is not localized.
  web_contents()->UpdateTitleForEntry(
      entry, base::UTF8ToUTF16(
                 base::StringPrintf(kTitleFormat, simplified_url.c_str())));
}

void DevToolsUIBindings::LoadNetworkResource(DispatchCallback callback,
                                             const std::string& url,
                                             const std::string& headers,
                                             int stream_id) {
  GURL gurl(url);
  if (!gurl.is_valid()) {
    base::Value::Dict response_dict;
    response_dict.Set("statusCode", 404);
    response_dict.Set("urlValid", false);
    auto response = base::Value(std::move(response_dict));
    std::move(callback).Run(&response);
    return;
  }
  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("devtools_network_resource", R"(
        semantics {
          sender: "Developer Tools"
          description:
            "When user opens Developer Tools, the browser may fetch additional "
            "resources from the network to enrich the debugging experience "
            "(e.g. source map resources)."
          trigger: "User opens Developer Tools to debug a web page."
          data: "Any resources requested by Developer Tools."
          destination: WEBSITE
          internal {
            contacts {
              email: "chrome-devtools@google.com"
            }
          }
          user_data {
            type: WEB_CONTENT
          }
          last_reviewed: "2024-02-09"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "It's not possible to disable this feature from settings."
          chrome_policy {
            DeveloperToolsAvailability {
              policy_options {mode: MANDATORY}
              DeveloperToolsAvailability: 2
            }
          }
        })");

  network::ResourceRequest resource_request;
  resource_request.url = gurl;
  // TODO(caseq): this preserves behavior of URLFetcher-based implementation.
  // We really need to pass proper first party origin from the front-end.
  resource_request.site_for_cookies = net::SiteForCookies::FromUrl(gurl);
  resource_request.headers.AddHeadersFromString(headers);

  NetworkResourceLoader::URLLoaderFactoryHolder url_loader_factory;
  if (gurl.SchemeIsFile()) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote =
        content::CreateFileURLLoaderFactory(
            base::FilePath() /* profile_path */,
            nullptr /* shared_cors_origin_access_list */);
    url_loader_factory = network::SharedURLLoaderFactory::Create(
        std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
            std::move(pending_remote)));
  } else if (content::HasWebUIScheme(gurl)) {
    content::WebContents* target_tab = delegate_->GetInspectedWebContents();
#if defined(NDEBUG)
    // In release builds, allow files from the chrome://, devtools:// and
    // chrome-untrusted:// schemes if a custom devtools front-end was specified.
    const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    const bool allow_web_ui_scheme =
        cmd_line->HasSwitch(switches::kCustomDevtoolsFrontend);
#else
    // In debug builds, always allow retrieving files from the chrome://,
    // devtools:// and chrome-untrusted:// schemes.
    const bool allow_web_ui_scheme = true;
#endif
    // Only allow retrieval if the scheme of the file is the same as the
    // top-level frame of the inspected page.
    // TODO(sigurds): Track which frame triggered the load, match schemes to the
    // committed URL of that frame, and use the loader associated with that
    // frame to allow nested frames with different schemes to load files.
    if (allow_web_ui_scheme && target_tab &&
        target_tab->GetLastCommittedURL().GetScheme() == gurl.GetScheme()) {
      std::vector<std::string> allowed_webui_hosts;
      content::RenderFrameHost* frame_host =
          web_contents()->GetPrimaryMainFrame();

      mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote =
          content::CreateWebUIURLLoaderFactory(
              frame_host, target_tab->GetLastCommittedURL().GetScheme(),
              std::move(allowed_webui_hosts));
      url_loader_factory = network::SharedURLLoaderFactory::Create(
          std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
              std::move(pending_remote)));
    } else {
      base::Value::Dict response_dict;
      response_dict.Set("schemeSupported", false);
      response_dict.Set("statusCode", 403);
      auto response = base::Value(std::move(response_dict));
      std::move(callback).Run(&response);
      return;
    }
  } else {
    content::WebContents* target_tab = delegate_->GetInspectedWebContents();
    if (target_tab) {
      auto* partition =
          target_tab->GetPrimaryMainFrame()->GetStoragePartition();
      url_loader_factory = partition->GetURLLoaderFactoryForBrowserProcess();
    } else {
      base::Value::Dict response_dict;
      response_dict.Set("statusCode", 409);
      auto response = base::Value(std::move(response_dict));
      std::move(callback).Run(&response);
      return;
    }
  }

  NetworkResourceLoader::Create(
      stream_id, this, resource_request, traffic_annotation,
      std::move(url_loader_factory), std::move(callback));
}

void DevToolsUIBindings::OpenInNewTab(const std::string& url) {
  delegate_->OpenInNewTab(url);
}

void DevToolsUIBindings::OpenSearchResultsInNewTab(const std::string& query) {
  delegate_->OpenSearchResultsInNewTab(query);
}

void DevToolsUIBindings::ShowItemInFolder(const std::string& file_system_path) {
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  file_helper_.ShowItemInFolder(file_system_path);
}

void DevToolsUIBindings::SaveToFile(const std::string& url,
                                    const std::string& content,
                                    bool save_as,
                                    bool is_base64) {
  file_helper_.Save(
      url, content, save_as, is_base64,
      base::BindOnce(&DevToolsSelectFileDialog::SelectFile, web_contents_,
                     ui::SelectFileDialog::SELECT_SAVEAS_FILE),
      base::BindOnce(&DevToolsUIBindings::FileSavedAs,
                     weak_factory_.GetWeakPtr(), url),
      base::BindOnce(&DevToolsUIBindings::CanceledFileSaveAs,
                     weak_factory_.GetWeakPtr(), url));
}

void DevToolsUIBindings::AppendToFile(const std::string& url,
                                      const std::string& content) {
  file_helper_.Append(url, content,
                      base::BindOnce(&DevToolsUIBindings::AppendedTo,
                                     weak_factory_.GetWeakPtr(), url));
}

void DevToolsUIBindings::RequestFileSystems() {
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  base::Value::List file_systems_value;
  for (auto const& file_system : file_helper_.GetFileSystems()) {
    file_systems_value.Append(CreateFileSystemValue(file_system));
  }
  CallClientMethod("DevToolsAPI", "fileSystemsLoaded",
                   base::Value(std::move(file_systems_value)));
}

void DevToolsUIBindings::AddFileSystem(const std::string& type) {
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  file_helper_.AddFileSystem(
      type,
      base::BindOnce(&DevToolsSelectFileDialog::SelectFile, web_contents_,
                     ui::SelectFileDialog::SELECT_FOLDER),
      base::BindRepeating(&DevToolsUIBindings::HandleDirectoryPermissions,
                          weak_factory_.GetWeakPtr()));
}

void DevToolsUIBindings::RemoveFileSystem(const std::string& file_system_path) {
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  file_helper_.RemoveFileSystem(file_system_path);
}

void DevToolsUIBindings::UpgradeDraggedFileSystemPermissions(
    const std::string& file_system_url) {
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  file_helper_.UpgradeDraggedFileSystemPermissions(
      file_system_url,
      base::BindRepeating(&DevToolsUIBindings::HandleDirectoryPermissions,
                          weak_factory_.GetWeakPtr()));
}

void DevToolsUIBindings::ConnectAutomaticFileSystem(
    DispatchCallback callback,
    const std::string& file_system_path,
    const std::string& file_system_uuid,
    bool add_if_missing) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  // Ensure that the |file_system_uuid| is indeed a valid UUID.
  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(file_system_uuid);
  if (!uuid.is_valid()) {
    LOG(ERROR) << "Rejecting automatic file system " << file_system_path
               << " with invalid UUID " << file_system_uuid << ".";
    ConnectAutomaticFileSystemDone(std::move(callback), false);
    return;
  }

  file_helper_.ConnectAutomaticFileSystem(
      file_system_path, uuid, add_if_missing,
      BindRepeating(&DevToolsUIBindings::HandleDirectoryPermissions,
                    weak_factory_.GetWeakPtr()),
      BindOnce(&DevToolsUIBindings::ConnectAutomaticFileSystemDone,
               weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DevToolsUIBindings::ConnectAutomaticFileSystemDone(
    DispatchCallback callback,
    bool success) {
  base::Value::Dict result_dict;
  result_dict.Set("success", success);
  base::Value result(std::move(result_dict));
  std::move(callback).Run(&result);
}

void DevToolsUIBindings::DisconnectAutomaticFileSystem(
    const std::string& file_system_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  file_helper_.DisconnectAutomaticFileSystem(file_system_path);
}

void DevToolsUIBindings::IndexPath(
    int index_request_id,
    const std::string& file_system_path,
    const std::string& excluded_folders_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  if (!file_helper_.IsFileSystemAdded(file_system_path)) {
    IndexingDone(index_request_id, file_system_path);
    return;
  }
  if (indexing_jobs_.count(index_request_id) != 0) {
    return;
  }
  std::vector<std::string> excluded_folders;
  std::optional<base::Value> parsed_excluded_folders = base::JSONReader::Read(
      excluded_folders_message, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (parsed_excluded_folders && parsed_excluded_folders->is_list()) {
    for (const base::Value& folder_path : parsed_excluded_folders->GetList()) {
      if (folder_path.is_string()) {
        excluded_folders.push_back(folder_path.GetString());
      }
    }
  }

  indexing_jobs_[index_request_id] =
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob>(
          file_system_indexer_->IndexPath(
              file_system_path, excluded_folders,
              BindOnce(&DevToolsUIBindings::IndexingTotalWorkCalculated,
                       weak_factory_.GetWeakPtr(), index_request_id,
                       file_system_path),
              BindRepeating(&DevToolsUIBindings::IndexingWorked,
                            weak_factory_.GetWeakPtr(), index_request_id,
                            file_system_path),
              BindOnce(&DevToolsUIBindings::IndexingDone,
                       weak_factory_.GetWeakPtr(), index_request_id,
                       file_system_path)));
}

void DevToolsUIBindings::StopIndexing(int index_request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = indexing_jobs_.find(index_request_id);
  if (it == indexing_jobs_.end()) {
    return;
  }
  it->second->Stop();
  indexing_jobs_.erase(it);
}

void DevToolsUIBindings::SearchInPath(int search_request_id,
                                      const std::string& file_system_path,
                                      const std::string& query) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  if (!file_helper_.IsFileSystemAdded(file_system_path)) {
    SearchCompleted(search_request_id, file_system_path,
                    std::vector<std::string>());
    return;
  }
  file_system_indexer_->SearchInPath(
      file_system_path, query,
      BindOnce(&DevToolsUIBindings::SearchCompleted, weak_factory_.GetWeakPtr(),
               search_request_id, file_system_path));
}

void DevToolsUIBindings::SetWhitelistedShortcuts(const std::string& message) {
  delegate_->SetWhitelistedShortcuts(message);
}

void DevToolsUIBindings::SetEyeDropperActive(bool active) {
  delegate_->SetEyeDropperActive(active);
}

void DevToolsUIBindings::ShowCertificateViewer(const std::string& cert_chain) {
  delegate_->ShowCertificateViewer(cert_chain);
}

void DevToolsUIBindings::ZoomIn() {
  zoom::PageZoom::Zoom(web_contents(), content::PAGE_ZOOM_IN);
}

void DevToolsUIBindings::ZoomOut() {
  zoom::PageZoom::Zoom(web_contents(), content::PAGE_ZOOM_OUT);
}

void DevToolsUIBindings::ResetZoom() {
  zoom::PageZoom::Zoom(web_contents(), content::PAGE_ZOOM_RESET);
}

void DevToolsUIBindings::SetDevicesDiscoveryConfig(
    bool discover_usb_devices,
    bool port_forwarding_enabled,
    const std::string& port_forwarding_config,
    bool network_discovery_enabled,
    const std::string& network_discovery_config) {
  std::optional<base::Value::Dict> parsed_port_forwarding =
      base::JSONReader::ReadDict(port_forwarding_config,
                                 base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!parsed_port_forwarding) {
    return;
  }
  std::optional<base::Value> parsed_network = base::JSONReader::Read(
      network_discovery_config, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!parsed_network || !parsed_network->is_list()) {
    return;
  }
  profile_->GetPrefs()->SetBoolean(prefs::kDevToolsDiscoverUsbDevicesEnabled,
                                   discover_usb_devices);
  profile_->GetPrefs()->SetBoolean(prefs::kDevToolsPortForwardingEnabled,
                                   port_forwarding_enabled);
  profile_->GetPrefs()->Set(prefs::kDevToolsPortForwardingConfig,
                            base::Value(std::move(*parsed_port_forwarding)));
  profile_->GetPrefs()->SetBoolean(prefs::kDevToolsDiscoverTCPTargetsEnabled,
                                   network_discovery_enabled);
  profile_->GetPrefs()->Set(prefs::kDevToolsTCPDiscoveryConfig,
                            *parsed_network);
}

void DevToolsUIBindings::DevicesDiscoveryConfigUpdated() {
  base::Value::Dict config;
  config.Set(kConfigDiscoverUsbDevices,
             profile_->GetPrefs()
                 ->FindPreference(prefs::kDevToolsDiscoverUsbDevicesEnabled)
                 ->GetValue()
                 ->Clone());
  config.Set(kConfigPortForwardingEnabled,
             profile_->GetPrefs()
                 ->FindPreference(prefs::kDevToolsPortForwardingEnabled)
                 ->GetValue()
                 ->Clone());
  config.Set(kConfigPortForwardingConfig,
             profile_->GetPrefs()
                 ->FindPreference(prefs::kDevToolsPortForwardingConfig)
                 ->GetValue()
                 ->Clone());
  config.Set(kConfigNetworkDiscoveryEnabled,
             profile_->GetPrefs()
                 ->FindPreference(prefs::kDevToolsDiscoverTCPTargetsEnabled)
                 ->GetValue()
                 ->Clone());
  config.Set(kConfigNetworkDiscoveryConfig,
             profile_->GetPrefs()
                 ->FindPreference(prefs::kDevToolsTCPDiscoveryConfig)
                 ->GetValue()
                 ->Clone());
  CallClientMethod("DevToolsAPI", "devicesDiscoveryConfigChanged",
                   base::Value(std::move(config)));
}

void DevToolsUIBindings::SendPortForwardingStatus(base::Value status) {
  CallClientMethod("DevToolsAPI", "devicesPortForwardingStatusChanged",
                   std::move(status));
}

void DevToolsUIBindings::SetDevicesUpdatesEnabled(bool enabled) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else
  if (devices_updates_enabled_ == enabled) {
    return;
  }
  devices_updates_enabled_ = enabled;
  if (enabled) {
    remote_targets_handler_ = DevToolsTargetsUIHandler::CreateForAdb(
        base::BindRepeating(&DevToolsUIBindings::DevicesUpdated,
                            base::Unretained(this)),
        profile_);
    pref_change_registrar_.Init(profile_->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kDevToolsDiscoverUsbDevicesEnabled,
        base::BindRepeating(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kDevToolsPortForwardingEnabled,
        base::BindRepeating(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kDevToolsPortForwardingConfig,
        base::BindRepeating(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kDevToolsDiscoverTCPTargetsEnabled,
        base::BindRepeating(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kDevToolsTCPDiscoveryConfig,
        base::BindRepeating(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                            base::Unretained(this)));
    port_status_serializer_ = std::make_unique<PortForwardingStatusSerializer>(
        base::BindRepeating(&DevToolsUIBindings::SendPortForwardingStatus,
                            base::Unretained(this)),
        profile_);
    DevicesDiscoveryConfigUpdated();
  } else {
    remote_targets_handler_.reset();
    port_status_serializer_.reset();
    pref_change_registrar_.RemoveAll();
    SendPortForwardingStatus(base::Value());
  }
#endif
}

void DevToolsUIBindings::OpenRemotePage(const std::string& browser_id,
                                        const std::string& url) {
  if (!remote_targets_handler_) {
    return;
  }
  remote_targets_handler_->Open(browser_id, url);
}

void DevToolsUIBindings::OpenNodeFrontend() {
  delegate_->OpenNodeFrontend();
}

void DevToolsUIBindings::RegisterPreference(const std::string& name,
                                            const RegisterOptions& options) {
  settings_.Register(name, options);
}

void DevToolsUIBindings::GetPreferences(DispatchCallback callback) {
  base::Value settings = base::Value(settings_.Get());
  std::move(callback).Run(&settings);
}

void DevToolsUIBindings::GetPreference(DispatchCallback callback,
                                       const std::string& name) {
  base::Value pref = settings_.Get(name).value_or(base::Value());
  std::move(callback).Run(&pref);
}

void DevToolsUIBindings::SetPreference(const std::string& name,
                                       const std::string& value) {
  settings_.Set(name, value);
}

void DevToolsUIBindings::RemovePreference(const std::string& name) {
  settings_.Remove(name);
}

void DevToolsUIBindings::ClearPreferences() {
  settings_.Clear();
}

void DevToolsUIBindings::GetSyncInformation(DispatchCallback callback) {
  auto result =
      base::Value(DevToolsUIBindings::GetSyncInformationForProfile(profile_));
  std::move(callback).Run(&result);
}

base::Value::Dict DevToolsUIBindings::GetSyncInformationForProfile(
    Profile* profile) {
  base::Value::Dict result;
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    result.Set("isSyncActive", false);
    return result;
  }

  result.Set("isSyncActive", base::FeatureList::IsEnabled(
                                 switches::kEnablePreferencesAccountStorage) ||
                                 sync_service->IsSyncFeatureActive());
  result.Set("arePreferencesSynced", sync_service->GetActiveDataTypes().Has(
                                         syncer::DataType::PREFERENCES));
  result.Set("isSyncPaused", sync_service->GetTransportState() ==
                                 syncer::SyncService::TransportState::PAUSED);

  CoreAccountInfo account_info = sync_service->GetAccountInfo();
  if (account_info.IsEmpty()) {
    return result;
  }

  result.Set("accountEmail", account_info.email);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo extended_info =
      identity_manager->FindExtendedAccountInfo(account_info);
  gfx::Image account_image;
  if (extended_info.IsEmpty() || extended_info.account_image.IsEmpty()) {
    account_image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  } else {
    account_image = extended_info.account_image;
  }
  scoped_refptr<base::RefCountedMemory> png_bytes =
      account_image.As1xPNGBytes();
  if (png_bytes->size() > 0) {
    result.Set("accountImage", base::Base64Encode(*png_bytes));
  }

  if (!extended_info.IsEmpty()) {
    result.Set("accountFullName", extended_info.full_name);
  }

  return result;
}

void DevToolsUIBindings::GetHostConfig(DispatchCallback callback) {
  base::Value::Dict response_dict;

  AidaClient::Availability availability = AidaClient::CanUseAida(profile_);

  base::Value::Dict aida_availability;
  aida_availability.Set("enabled", availability.available);
  aida_availability.Set("blockedByAge", availability.blocked_by_age);
  aida_availability.Set("blockedByEnterprisePolicy",
                        availability.blocked_by_enterprise_policy);
  aida_availability.Set("blockedByGeo", availability.blocked_by_geo);
  aida_availability.Set("disallowLogging", availability.disallow_logging);
  aida_availability.Set("enterprisePolicyValue",
                        static_cast<int>(availability.enterprise_policy_value));
  response_dict.Set("aidaAvailability", std::move(aida_availability));

  version_info::Channel channel = chrome::GetChannel();
  if (channel != version_info::Channel::UNKNOWN) {
    response_dict.Set("channel", version_info::GetChannelString(channel));
  }

  base::Value::Dict console_insights_dict;
  console_insights_dict.Set(
      "enabled",
      base::FeatureList::IsEnabled(::features::kDevToolsConsoleInsights));
  console_insights_dict.Set("modelId",
                            features::kDevToolsConsoleInsightsModelId.Get());
  console_insights_dict.Set(
      "temperature", features::kDevToolsConsoleInsightsTemperature.Get());
  response_dict.Set("devToolsConsoleInsights",
                    std::move(console_insights_dict));

  if (base::FeatureList::IsEnabled(::features::kDevToolsFreestyler)) {
    base::Value::Dict freestyler_dict;
    freestyler_dict.Set("enabled", base::FeatureList::IsEnabled(
                                       ::features::kDevToolsFreestyler));
    freestyler_dict.Set("featureName", ::features::kDevToolsFreestyler.name);
    freestyler_dict.Set("modelId", features::kDevToolsFreestylerModelId.Get());
    freestyler_dict.Set("temperature",
                        features::kDevToolsFreestylerTemperature.Get());
    freestyler_dict.Set("userTier",
                        features::kDevToolsFreestylerUserTier.GetName(
                            features::kDevToolsFreestylerUserTier.Get()));
    freestyler_dict.Set("executionMode",
                        features::kDevToolsFreestylerExecutionMode.GetName(
                            features::kDevToolsFreestylerExecutionMode.Get()));
    freestyler_dict.Set("patching",
                        features::kDevToolsFreestylerPatching.Get());
    freestyler_dict.Set("multimodal",
                        features::kDevToolsFreestylerMultimodal.Get());
    freestyler_dict.Set(
        "multimodalUploadInput",
        features::kDevToolsFreestylerMultimodalUploadInput.Get());
    freestyler_dict.Set("functionCalling",
                        features::kDevToolsFreestylerFunctionCalling.Get());
    response_dict.Set("devToolsFreestyler", std::move(freestyler_dict));
  }

  if (base::FeatureList::IsEnabled(
          ::features::kDevToolsAiAssistanceNetworkAgent)) {
    base::Value::Dict network_agent_dict;
    network_agent_dict.Set("enabled",
                           base::FeatureList::IsEnabled(
                               ::features::kDevToolsAiAssistanceNetworkAgent));
    network_agent_dict.Set("featureName",
                           ::features::kDevToolsAiAssistanceNetworkAgent.name);
    network_agent_dict.Set(
        "modelId", features::kDevToolsAiAssistanceNetworkAgentModelId.Get());
    network_agent_dict.Set(
        "temperature",
        features::kDevToolsAiAssistanceNetworkAgentTemperature.Get());
    network_agent_dict.Set(
        "userTier",
        features::kDevToolsAiAssistanceNetworkAgentUserTier.GetName(
            features::kDevToolsAiAssistanceNetworkAgentUserTier.Get()));
    response_dict.Set("devToolsAiAssistanceNetworkAgent",
                      std::move(network_agent_dict));
  }

  if (base::FeatureList::IsEnabled(
          ::features::kDevToolsAiAssistancePerformanceAgent)) {
    base::Value::Dict ai_assistance_performance_agent_dict;
    ai_assistance_performance_agent_dict.Set(
        "enabled", base::FeatureList::IsEnabled(
                       ::features::kDevToolsAiAssistancePerformanceAgent));
    ai_assistance_performance_agent_dict.Set(
        "featureName", ::features::kDevToolsAiAssistancePerformanceAgent.name);
    ai_assistance_performance_agent_dict.Set(
        "modelId",
        features::kDevToolsAiAssistancePerformanceAgentModelId.Get());
    ai_assistance_performance_agent_dict.Set(
        "temperature",
        features::kDevToolsAiAssistancePerformanceAgentTemperature.Get());
    ai_assistance_performance_agent_dict.Set(
        "userTier",
        features::kDevToolsAiAssistancePerformanceAgentUserTier.GetName(
            features::kDevToolsAiAssistancePerformanceAgentUserTier.Get()));
    ai_assistance_performance_agent_dict.Set(
        "insightsEnabled",
        features::kDevToolsAiAssistancePerformanceAgentInsightsEnabled.Get());
    response_dict.Set("devToolsAiAssistancePerformanceAgent",
                      std::move(ai_assistance_performance_agent_dict));
  }

  if (base::FeatureList::IsEnabled(
          ::features::kDevToolsAiAssistanceFileAgent)) {
    base::Value::Dict ai_assistance_file_agent_dict;
    ai_assistance_file_agent_dict.Set(
        "enabled", base::FeatureList::IsEnabled(
                       ::features::kDevToolsAiAssistanceFileAgent));
    ai_assistance_file_agent_dict.Set("featureName",
                                       ::features::kDevToolsAiAssistanceFileAgent.name);
    ai_assistance_file_agent_dict.Set(
        "modelId", features::kDevToolsAiAssistanceFileAgentModelId.Get());
    ai_assistance_file_agent_dict.Set(
        "temperature",
        features::kDevToolsAiAssistanceFileAgentTemperature.Get());
    ai_assistance_file_agent_dict.Set(
        "userTier",
        features::kDevToolsAiAssistanceFileAgentUserTier.GetName(
            features::kDevToolsAiAssistanceFileAgentUserTier.Get()));
    response_dict.Set("devToolsAiAssistanceFileAgent",
                      std::move(ai_assistance_file_agent_dict));
  }

  if (base::FeatureList::IsEnabled(::features::kDevToolsAiCodeCompletion)) {
    base::Value::Dict ai_code_completion_dict;
    ai_code_completion_dict.Set(
        "enabled",
        base::FeatureList::IsEnabled(::features::kDevToolsAiCodeCompletion));
    ai_code_completion_dict.Set(
        "modelId", features::kDevToolsAiCodeCompletionModelId.Get());
    ai_code_completion_dict.Set(
        "temperature", features::kDevToolsAiCodeCompletionTemperature.Get());
    ai_code_completion_dict.Set(
        "userTier",
        features::kDevToolsAiCodeCompletionUserTier.GetName(
            features::kDevToolsAiCodeCompletionUserTier.Get()));
    response_dict.Set("devToolsAiCodeCompletion",
                      std::move(ai_code_completion_dict));
  }

  if (base::FeatureList::IsEnabled(::features::kDevToolsAiCodeGeneration)) {
    base::Value::Dict ai_code_generation_dict;
    ai_code_generation_dict.Set(
        "enabled",
        base::FeatureList::IsEnabled(::features::kDevToolsAiCodeGeneration));
    ai_code_generation_dict.Set(
        "modelId", features::kDevToolsAiCodeGenerationModelId.Get());
    ai_code_generation_dict.Set(
        "temperature", features::kDevToolsAiCodeGenerationTemperature.Get());
    ai_code_generation_dict.Set(
        "userTier",
        features::kDevToolsAiCodeGenerationUserTier.GetName(
            features::kDevToolsAiCodeGenerationUserTier.Get()));
    response_dict.Set("devToolsAiCodeGeneration",
                      std::move(ai_code_generation_dict));
  }

  if (base::FeatureList::IsEnabled(
          ::features::kDevToolsEnableDurableMessages)) {
    base::Value::Dict devtools_durable_message_dict;
    devtools_durable_message_dict.Set(
        "enabled",
        base::FeatureList::IsEnabled(features::kDevToolsEnableDurableMessages));
    response_dict.Set("devToolsEnableDurableMessages",
                      std::move(devtools_durable_message_dict));
  }

  base::Value::Dict devtools_well_known_dict;
  devtools_well_known_dict.Set(
      "enabled", base::FeatureList::IsEnabled(::features::kDevToolsWellKnown));
  response_dict.Set("devToolsWellKnown", std::move(devtools_well_known_dict));

  base::Value::Dict ve_logging_dict;
  ve_logging_dict.Set("enabled", true);
  ve_logging_dict.Set("testing", false);
  response_dict.Set("devToolsVeLogging", std::move(ve_logging_dict));

  response_dict.Set("isOffTheRecord", profile_->IsOffTheRecord());

  base::Value::Dict devtools_privacy_ui_dict;
  devtools_privacy_ui_dict.Set(
      "enabled", base::FeatureList::IsEnabled(::features::kDevToolsPrivacyUI));
  response_dict.Set("devToolsPrivacyUI", std::move(devtools_privacy_ui_dict));

  if (base::FeatureList::IsEnabled(features::kDevToolsPrivacyUI)) {
    base::Value::Dict third_party_cookie_controls_dict;
    third_party_cookie_controls_dict.Set(
        "thirdPartyCookieRestrictionEnabled",
        TrackingProtectionSettingsFactory::GetForProfile(profile())
            ->IsTrackingProtection3pcdEnabled());

    third_party_cookie_controls_dict.Set(
        "thirdPartyCookieMetadataEnabled",
        base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants));
    third_party_cookie_controls_dict.Set(
        "thirdPartyCookieHeuristicsEnabled",
        base::FeatureList::IsEnabled(
            content_settings::features::kTpcdHeuristicsGrants));

    policy::PolicyService* policy_service =
        profile()->GetProfilePolicyConnector()->policy_service();
    CHECK(policy_service);
    const policy::PolicyMap& policies = policy_service->GetPolicies(
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
    const base::Value* block_third_party_cookies = policies.GetValue(
        policy::key::kBlockThirdPartyCookies, base::Value::Type::BOOLEAN);
    if (block_third_party_cookies && block_third_party_cookies->is_bool()) {
      third_party_cookie_controls_dict.Set(
          "managedBlockThirdPartyCookies",
          block_third_party_cookies->GetBool());
    } else {
      third_party_cookie_controls_dict.Set("managedBlockThirdPartyCookies",
                                           "Unset");
    }
    // TODO: Add enterprise policy CookiesAllowedForUrls.

    response_dict.Set("thirdPartyCookieControls",
                      std::move(third_party_cookie_controls_dict));
  }
  base::Value::Dict origin_bound_cookies_dict;
  origin_bound_cookies_dict.Set(
      "portBindingEnabled",
      base::FeatureList::IsEnabled(net::features::kEnablePortBoundCookies));
  origin_bound_cookies_dict.Set(
      "schemeBindingEnabled",
      base::FeatureList::IsEnabled(net::features::kEnableSchemeBoundCookies));
  response_dict.Set("devToolsEnableOriginBoundCookies",
                    std::move(origin_bound_cookies_dict));

  if (base::FeatureList::IsEnabled(features::kDevToolsGreenDevUi)) {
    response_dict.Set("devToolsGreenDevUi",
                      base::Value::Dict().Set("enabled", true));
  }

  if (base::FeatureList::IsEnabled(
          ::features::kDevToolsAnimationStylesInStylesTab)) {
    base::Value::Dict devtools_animation_styles_in_styles_tab_dict;
    devtools_animation_styles_in_styles_tab_dict.Set(
        "enabled", base::FeatureList::IsEnabled(
                       ::features::kDevToolsAnimationStylesInStylesTab));
    response_dict.Set("devToolsAnimationStylesInStylesTab",
                      std::move(devtools_animation_styles_in_styles_tab_dict));
  }

  base::Value::Dict deep_links_via_extensibility_api_dict;
  deep_links_via_extensibility_api_dict.Set(
      "enabled",
      base::FeatureList::IsEnabled(
          ::blink::features::kEnableDevtoolsDeepLinkViaExtensibilityApi));
  response_dict.Set("devToolsDeepLinksViaExtensibilityApi",
                    std::move(deep_links_via_extensibility_api_dict));

  base::Value::Dict ai_generated_timeline_labels_dict;
  ai_generated_timeline_labels_dict.Set(
      "enabled", base::FeatureList::IsEnabled(
                     ::features::kDevToolsAiGeneratedTimelineLabels));
  response_dict.Set("devToolsAiGeneratedTimelineLabels",
                    std::move(ai_generated_timeline_labels_dict));

  base::Value::Dict devtools_force_popover_dict;
  devtools_force_popover_dict.Set(
      "enabled", base::FeatureList::IsEnabled(
                     blink::features::kDevToolsAllowPopoverForcing));
  response_dict.Set("devToolsAllowPopoverForcing",
                    std::move(devtools_force_popover_dict));

  base::Value::Dict flexible_layout_dict;
  flexible_layout_dict.Set(
      "verticalDrawerEnabled",
      base::FeatureList::IsEnabled(::features::kDevToolsVerticalDrawer));
  response_dict.Set("devToolsFlexibleLayout", std::move(flexible_layout_dict));

  base::Value::Dict ai_submenu_prompts_dict;
  ai_submenu_prompts_dict.Set(
      "enabled", base::FeatureList::IsEnabled(
                     ::features::kDevToolsAiSubmenuPrompts));
  ai_submenu_prompts_dict.Set("featureName",
                              ::features::kDevToolsAiSubmenuPrompts.name);
  response_dict.Set("devToolsAiSubmenuPrompts",
                    std::move(ai_submenu_prompts_dict));

  base::Value::Dict ai_debug_with_ai_dict;
  ai_debug_with_ai_dict.Set("enabled", base::FeatureList::IsEnabled(
                                           ::features::kDevToolsAiDebugWithAi));
  ai_debug_with_ai_dict.Set("featureName",
                            ::features::kDevToolsAiDebugWithAi.name);
  response_dict.Set("devToolsAiDebugWithAi", std::move(ai_debug_with_ai_dict));

  if (base::FeatureList::IsEnabled(::features::kDevToolsGlobalAiButton)) {
    base::Value::Dict global_ai_button_dict;
    global_ai_button_dict.Set(
        "enabled",
        base::FeatureList::IsEnabled(::features::kDevToolsGlobalAiButton));
    global_ai_button_dict.Set(
        "promotionEnabled",
        features::kDevToolsGlobalAiButtonPromotionEnabled.Get());
    response_dict.Set("devToolsGlobalAiButton",
                      std::move(global_ai_button_dict));
  }

  // Once the feature is fully launched and the base::Features are enabled by
  // default, this dict can be removed.
  if (base::FeatureList::IsEnabled(::features::kDevToolsGdpProfiles)) {
    base::Value::Dict gdp_profiles_dict;
    gdp_profiles_dict.Set("enabled", base::FeatureList::IsEnabled(
                                         ::features::kDevToolsGdpProfiles));
    gdp_profiles_dict.Set("badgesEnabled",
                          features::kDevToolsGdpProfilesBadgesEnabled.Get());
    gdp_profiles_dict.Set(
        "starterBadgeEnabled",
        features::kDevToolsGdpProfilesStarterBadgeEnabled.Get());
    response_dict.Set("devToolsGdpProfiles", std::move(gdp_profiles_dict));
  }

  base::Value::Dict gdp_profiles_availability_dict;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  gdp_profiles_availability_dict.Set("enabled", true);
#else
  gdp_profiles_availability_dict.Set("enabled", false);
#endif
  gdp_profiles_availability_dict.Set(
      "enterprisePolicyValue",
      profile_->GetPrefs()->GetInteger(
          prefs::kDevToolsGoogleDeveloperProgramProfileAvailability));
  response_dict.Set("devToolsGdpProfilesAvailability",
                    std::move(gdp_profiles_availability_dict));

  response_dict.Set(
      "devToolsLiveEdit",
      base::Value::Dict().Set("enabled", base::FeatureList::IsEnabled(
                                             ::features::kDevToolsLiveEdit)));

  response_dict.Set(
      "devToolsIndividualRequestThrottling",
      base::Value::Dict().Set(
          "enabled", base::FeatureList::IsEnabled(
                         ::features::kDevToolsIndividualRequestThrottling)));

  base::Value::Dict starting_style_debugging;
  starting_style_debugging.Set(
      "enabled", base::FeatureList::IsEnabled(
                     ::features::kDevToolsStartingStyleDebugging));
  response_dict.Set("devToolsStartingStyleDebugging",
                    std::move(starting_style_debugging));

  base::Value::Dict prompt_api_dict;
  prompt_api_dict.Set("enabled", base::FeatureList::IsEnabled(
                                     ::features::kDevToolsAiPromptApi));
  prompt_api_dict.Set("allowWithoutGpu",
                      features::kDevToolsAiPromptApiAllowWithoutGpu.Get());
  response_dict.Set("devToolsAiPromptApi", std::move(prompt_api_dict));

  if (base::FeatureList::IsEnabled(
          ::features::kDevToolsAiAssistanceContextSelectionAgent)) {
    base::Value::Dict devtools_context_selection_agent;
    devtools_context_selection_agent.Set(
        "enabled", base::FeatureList::IsEnabled(
                       ::features::kDevToolsAiAssistanceContextSelectionAgent));
    response_dict.Set("devToolsAiAssistanceContextSelectionAgent",
                      std::move(devtools_context_selection_agent));
  }

  base::Value response = base::Value(std::move(response_dict));
  std::move(callback).Run(&response);
}

void DevToolsUIBindings::Reattach(DispatchCallback callback) {
  if (agent_host_.get()) {
    agent_host_->DetachClient(this);
    InnerAttach();
  }
  std::move(callback).Run(nullptr);
}

void DevToolsUIBindings::ReadyForTest() {
  delegate_->ReadyForTest();
}

void DevToolsUIBindings::ConnectionReady() {
  delegate_->ConnectionReady();
}

void DevToolsUIBindings::SetOpenNewWindowForPopups(bool value) {
  delegate_->SetOpenNewWindowForPopups(value);
}

void DevToolsUIBindings::DispatchProtocolMessageFromDevToolsFrontend(
    const std::string& message) {
  if (!agent_host_) {
    return;
  }
  agent_host_->DispatchProtocolMessage(this, base::as_byte_span(message));
}

void DevToolsUIBindings::SetHttpServiceRegistryForTesting(
    std::unique_ptr<DevToolsHttpServiceRegistry> service_registry) {
  http_service_registry_ = std::move(service_registry);
}

void DevToolsUIBindings::RecordCountHistogram(const std::string& name,
                                              int sample,
                                              int min,
                                              int exclusive_max,
                                              int buckets) {
  if (!frontend_host_) {
    return;
  }

  // DevTools previously would crash if histogram counts didn't make sense.
  // We've changed this to a DCHECK and instead clamp the value for counts,
  // because it doesn't really make sense to crash if the histogram is out
  // of range.
  DCHECK_GE(sample, min);
  DCHECK_LT(sample, exclusive_max);

  if (sample < min) {
    sample = 0;
  } else if (sample >= exclusive_max) {
    sample = exclusive_max - 1;
  }

  base::UmaHistogramCustomCounts(name, sample, min, exclusive_max, buckets);
}

void DevToolsUIBindings::RecordEnumeratedHistogram(const std::string& name,
                                                   int sample,
                                                   int boundary_value) {
  if (!frontend_host_) {
    return;
  }

  DCHECK_GE(boundary_value, 0);
  DCHECK_LT(boundary_value, 1000);
  DCHECK_GE(sample, 0);
  DCHECK_LT(sample, boundary_value);
  if (!(boundary_value >= 0 && boundary_value <= 1000 && sample >= 0 &&
        sample < boundary_value)) {
    // We should have DCHECK'd in debug builds; for release builds, if we're
    // out of range, just omit the histogram
    return;
  }

  const std::string kDevToolsHistogramPrefix = "DevTools.";
  DCHECK_EQ(name.compare(0, kDevToolsHistogramPrefix.size(),
                         kDevToolsHistogramPrefix),
            0);
  base::UmaHistogramExactLinear(name, sample, boundary_value);
}

void DevToolsUIBindings::RecordPerformanceHistogram(const std::string& name,
                                                    double duration) {
  if (!frontend_host_) {
    return;
  }
  if (duration < 0) {
    return;
  }
  // Use histogram_functions.h instead of macros as the name comes from the
  // DevTools frontend javascript and so will always have the same call site.
  base::TimeDelta delta = base::Milliseconds(duration);
  base::UmaHistogramTimes(name, delta);
}

void DevToolsUIBindings::RecordPerformanceHistogramMedium(
    const std::string& name,
    double duration) {
  if (!frontend_host_) {
    return;
  }
  if (duration < 0) {
    return;
  }
  // Use histogram_functions.h instead of macros as the name comes from the
  // DevTools frontend javascript and so will always have the same call site.
  base::TimeDelta delta = base::Milliseconds(duration);
  base::UmaHistogramMediumTimes(name, delta);
}

void DevToolsUIBindings::RecordUserMetricsAction(const std::string& name) {
  if (!frontend_host_) {
    return;
  }
  // Use RecordComputedAction instead of RecordAction as the name comes from
  // DevTools frontend javascript and so will always have the same call site.
  base::RecordComputedAction(name);
}

void DevToolsUIBindings::RecordNewBadgeUsage(const std::string& feature_name) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else

  auto* user_education_service =
      UserEducationServiceFactory::GetForBrowserContext(profile_);
  if (!user_education_service ||
      !user_education_service->new_badge_registry()) {
    return;
  }

  const base::Feature* feature_to_register = nullptr;
  for (const auto& [feature, spec] :
       user_education_service->new_badge_registry()->feature_data()) {
    if (feature_name == feature->name) {
      feature_to_register = feature;
      break;
    }
  }

  if (feature_to_register) {
    UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
        web_contents()->GetBrowserContext(), *feature_to_register);
  }
#endif
}

void DevToolsUIBindings::MaybeStartLogging() {
  if (session_id_for_logging_.is_empty()) {
    session_id_for_logging_ = base::UnguessableToken::Create();
    session_start_time_ = base::TimeTicks::Now();
    base::Value::Dict sync_info = GetSyncInformationForProfile(profile_);
    int64_t session_tags = 0;
    bool is_signed_in = sync_info.FindBool("accountEmail").has_value() &&
                        !sync_info.FindBool("isSyncPaused").value_or(false);
    if (is_signed_in) {
      session_tags |= SessionTags::kUserSignedIn;
    }
    int gen_ai_settings =
        profile_->GetPrefs()->GetInteger(prefs::kDevToolsGenAiSettings);
    if (gen_ai_settings ==
        static_cast<int>(DevToolsGenAiEnterprisePolicyValue::kDisable)) {
      session_tags |= SessionTags::kDevToolsGetAiEnterprisePolicyDisabled;
    }
    if (gen_ai_settings ==
        static_cast<int>(
            DevToolsGenAiEnterprisePolicyValue::kAllowWithoutLogging)) {
      session_tags |=
          SessionTags::kDevToolsGetAiEnterprisePolicyAllowWithoutLogging;
    }
    bool remote_debugging_enabled =
        g_browser_process->local_state()->GetBoolean(
            prefs::kDevToolsRemoteDebuggingAllowed);
    if (!remote_debugging_enabled) {
      session_tags |= SessionTags::kDevToolsRemoteDebuggingDisabled;
    }
    metrics::structured::StructuredMetricsClient::Record(
        metrics::structured::events::v2::dev_tools::SessionStart()
            .SetTags(session_tags)
            .SetTrigger(delegate_->GetOpenedByForLogging())
            .SetDockSide(delegate_->GetDockStateForLogging())
            .SetSessionId(session_id_for_logging_.GetLowForSerialization())
            .SetIsSignedIn(is_signed_in));
  }
}

base::TimeDelta DevToolsUIBindings::GetTimeSinceSessionStart() {
  return base::TimeTicks::Now() - session_start_time_;
}

void DevToolsUIBindings::RecordImpression(const ImpressionEvent& event) {
  MaybeStartLogging();
  for (const auto& ve : event.impressions) {
    metrics::structured::StructuredMetricsClient::Record(
        metrics::structured::events::v2::dev_tools::Impression()
            .SetVeId(ve.id)
            .SetVeType(ve.type)
            .SetVeParent(ve.parent)
            .SetVeContext(ve.context)
            .SetWidth(ve.width)
            .SetHeight(ve.height)
            .SetTimeSinceSessionStart(
                GetTimeSinceSessionStart().InMilliseconds())
            .SetSessionId(session_id_for_logging_.GetLowForSerialization()));
  }
}

void DevToolsUIBindings::RecordResize(const ResizeEvent& event) {
  MaybeStartLogging();
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::dev_tools::Resize()
          .SetVeId(event.veid)
          .SetWidth(event.width)
          .SetHeight(event.height)
          .SetTimeSinceSessionStart(GetTimeSinceSessionStart().InMilliseconds())
          .SetSessionId(session_id_for_logging_.GetLowForSerialization()));
}

void DevToolsUIBindings::RecordClick(const ClickEvent& event) {
  MaybeStartLogging();
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::dev_tools::Click()
          .SetVeId(event.veid)
          .SetMouseButton(event.mouse_button)
          .SetDoubleClick(event.double_click)
          .SetContext(event.context)
          .SetTimeSinceSessionStart(GetTimeSinceSessionStart().InMilliseconds())
          .SetSessionId(session_id_for_logging_.GetLowForSerialization()));
}

void DevToolsUIBindings::RecordHover(const HoverEvent& event) {
  MaybeStartLogging();
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::dev_tools::Hover()
          .SetVeId(event.veid)
          .SetTime(event.time)
          .SetContext(event.context)
          .SetTimeSinceSessionStart(GetTimeSinceSessionStart().InMilliseconds())
          .SetSessionId(session_id_for_logging_.GetLowForSerialization()));
}

void DevToolsUIBindings::RecordDrag(const DragEvent& event) {
  MaybeStartLogging();
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::dev_tools::Drag()
          .SetVeId(event.veid)
          .SetDistance(event.distance)
          .SetContext(event.context)
          .SetTimeSinceSessionStart(GetTimeSinceSessionStart().InMilliseconds())
          .SetSessionId(session_id_for_logging_.GetLowForSerialization()));
}

void DevToolsUIBindings::RecordChange(const ChangeEvent& event) {
  MaybeStartLogging();
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::dev_tools::Change()
          .SetVeId(event.veid)
          .SetContext(event.context)
          .SetTimeSinceSessionStart(GetTimeSinceSessionStart().InMilliseconds())
          .SetSessionId(session_id_for_logging_.GetLowForSerialization()));
}

void DevToolsUIBindings::RecordKeyDown(const KeyDownEvent& event) {
  MaybeStartLogging();
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::dev_tools::KeyDown()
          .SetVeId(event.veid)
          .SetContext(event.context)
          .SetTimeSinceSessionStart(GetTimeSinceSessionStart().InMilliseconds())
          .SetSessionId(session_id_for_logging_.GetLowForSerialization()));
}

void DevToolsUIBindings::RecordSettingAccess(const SettingAccessEvent& event) {
  MaybeStartLogging();
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::dev_tools::SettingAccess()
          .SetName(event.name)
          .SetNumericValue(event.numeric_value)
          .SetStringValue(event.string_value)
          .SetTimeSinceSessionStart(GetTimeSinceSessionStart().InMilliseconds())
          .SetSessionId(session_id_for_logging_.GetLowForSerialization()));
}

void DevToolsUIBindings::RecordFunctionCall(const FunctionCallEvent& event) {
  MaybeStartLogging();
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::dev_tools::FunctionCall()
          .SetName(event.name)
          .SetContext(event.context)
          .SetTimeSinceSessionStart(GetTimeSinceSessionStart().InMilliseconds())
          .SetSessionId(session_id_for_logging_.GetLowForSerialization()));
}

void DevToolsUIBindings::DeviceCountChanged(int count) {
  CallClientMethod("DevToolsAPI", "deviceCountUpdated", base::Value(count));
}

void DevToolsUIBindings::DevicesUpdated(const std::string& source,
                                        const base::Value& targets) {
  CallClientMethod("DevToolsAPI", "devicesUpdated", targets.Clone());
}

void DevToolsUIBindings::FileSavedAs(const std::string& url,
                                     const std::string& file_system_path) {
  CallClientMethod("DevToolsAPI", "savedURL", base::Value(url),
                   base::Value(file_system_path));
}

void DevToolsUIBindings::CanceledFileSaveAs(const std::string& url) {
  CallClientMethod("DevToolsAPI", "canceledSaveURL", base::Value(url));
}

void DevToolsUIBindings::AppendedTo(const std::string& url) {
  CallClientMethod("DevToolsAPI", "appendedToURL", base::Value(url));
}

void DevToolsUIBindings::FileSystemAdded(
    const std::string& error,
    const DevToolsFileHelper::FileSystem* file_system) {
  if (file_system) {
    CallClientMethod("DevToolsAPI", "fileSystemAdded", base::Value(error),
                     base::Value(CreateFileSystemValue(*file_system)));
  } else {
    CallClientMethod("DevToolsAPI", "fileSystemAdded", base::Value(error));
  }
}

void DevToolsUIBindings::FileSystemRemoved(
    const std::string& file_system_path) {
  CallClientMethod("DevToolsAPI", "fileSystemRemoved",
                   base::Value(file_system_path));
}

void DevToolsUIBindings::FilePathsChanged(
    const std::vector<std::string>& changed_paths,
    const std::vector<std::string>& added_paths,
    const std::vector<std::string>& removed_paths) {
  const int kMaxPathsPerMessage = 1000;
  size_t changed_index = 0;
  size_t added_index = 0;
  size_t removed_index = 0;
  // Dispatch limited amount of file paths in a time to avoid
  // IPC max message size limit. See https://crbug.com/797817.
  while (changed_index < changed_paths.size() ||
         added_index < added_paths.size() ||
         removed_index < removed_paths.size()) {
    int budget = kMaxPathsPerMessage;
    base::Value::List changed, added, removed;
    while (budget > 0 && changed_index < changed_paths.size()) {
      changed.Append(changed_paths[changed_index++]);
      --budget;
    }
    while (budget > 0 && added_index < added_paths.size()) {
      added.Append(added_paths[added_index++]);
      --budget;
    }
    while (budget > 0 && removed_index < removed_paths.size()) {
      removed.Append(removed_paths[removed_index++]);
      --budget;
    }
    CallClientMethod("DevToolsAPI", "fileSystemFilesChangedAddedRemoved",
                     base::Value(std::move(changed)),
                     base::Value(std::move(added)),
                     base::Value(std::move(removed)));
  }
}

void DevToolsUIBindings::IndexingTotalWorkCalculated(
    int request_id,
    const std::string& file_system_path,
    int total_work) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallClientMethod("DevToolsAPI", "indexingTotalWorkCalculated",
                   base::Value(request_id), base::Value(file_system_path),
                   base::Value(total_work));
}

void DevToolsUIBindings::IndexingWorked(int request_id,
                                        const std::string& file_system_path,
                                        int worked) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallClientMethod("DevToolsAPI", "indexingWorked", base::Value(request_id),
                   base::Value(file_system_path), base::Value(worked));
}

void DevToolsUIBindings::IndexingDone(int request_id,
                                      const std::string& file_system_path) {
  indexing_jobs_.erase(request_id);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CallClientMethod("DevToolsAPI", "indexingDone", base::Value(request_id),
                   base::Value(file_system_path));
}

void DevToolsUIBindings::SearchCompleted(
    int request_id,
    const std::string& file_system_path,
    const std::vector<std::string>& file_paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Value::List file_paths_value;
  for (auto const& file_path : file_paths) {
    file_paths_value.Append(file_path);
  }
  CallClientMethod("DevToolsAPI", "searchCompleted", base::Value(request_id),
                   base::Value(file_system_path),
                   base::Value(std::move(file_paths_value)));
}

void DevToolsUIBindings::HandleDirectoryPermissions(
    const std::string& directory_path,
    const std::u16string& message,
    DevToolsInfoBarDelegate::Callback callback) {
  if (base::FeatureList::IsEnabled(::features::kDevToolsNewPermissionDialog)) {
    ShowDirectoryPermissionDialog(directory_path, std::move(callback));
  } else {
    ShowDevToolsInfoBar(message, std::move(callback));
  }
}

void DevToolsUIBindings::ShowDevToolsInfoBar(
    const std::u16string& message,
    DevToolsInfoBarDelegate::Callback callback) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else
  if (!delegate_->GetInfoBarManager()) {
    std::move(callback).Run(false);
    return;
  }
  DevToolsInfoBarDelegate::Create(message, std::move(callback));
#endif
}

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCancelButtonId);

void DevToolsUIBindings::ShowDirectoryPermissionDialog(
    const std::string& directory_path,
    DevToolsInfoBarDelegate::Callback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  auto accept_callback = base::BindOnce(std::move(split_callback.first), true);
  auto cancel_callbacks = base::SplitOnceCallback(
      base::BindOnce(std::move(split_callback.second), false));
  std::u16string origin_identity_name = u"DevTools";
  chrome::ShowTabModal(
      ui::DialogModel::Builder()
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_DEV_TOOLS_EDIT_DIRECTORY_PERMISSION_TITLE))
          .AddParagraph(ui::DialogModelLabel::CreateWithReplacements(
              IDS_FILE_SYSTEM_ACCESS_WRITE_PERMISSION_DIRECTORY_TEXT,
              {ui::DialogModelLabel::CreateEmphasizedText(origin_identity_name),
               ui::DialogModelLabel::CreateEmphasizedText(
                   base::FilePath::FromUTF8Unsafe(directory_path)
                       .LossyDisplayName())}))
          .AddOkButton(
              std::move(accept_callback),
              ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
                  IDS_FILE_SYSTEM_ACCESS_EDIT_DIRECTORY_PERMISSION_ALLOW_TEXT)))
          .AddCancelButton(
              std::move(cancel_callbacks.first),
              ui::DialogModel::Button::Params().SetId(kCancelButtonId))
          .SetCloseActionCallback(std::move(cancel_callbacks.second))
          .SetInitiallyFocusedField(kCancelButtonId)
          .Build(),
      web_contents_);
}

void DevToolsUIBindings::OnPermissionDialogResult(
    DevToolsInfoBarDelegate::Callback callback,
    permissions::PermissionAction result) {
  std::move(callback).Run(result == permissions::PermissionAction::GRANTED);
}

void DevToolsUIBindings::AddDevToolsExtensionsToClient() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_->GetOriginalProfile());
  if (!registry) {
    return;
  }

  base::Value::List results;
  base::Value::List forbidden_origins;
  bool have_user_installed_devtools_extensions = false;
  extensions::ExtensionManagement* management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext());
  forbidden_origins.Append(
      url::Origin::Create(search::GetNewTabPageURL(profile_)).Serialize());
  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    if (extensions::Manifest::IsComponentLocation(extension->location())) {
      forbidden_origins.Append(extension->origin().Serialize());
    }
    if (extensions::chrome_manifest_urls::GetDevToolsPage(extension.get())
            .is_empty()) {
      continue;
    }
    GURL url =
        extensions::chrome_manifest_urls::GetDevToolsPage(extension.get());
    const bool is_extension_url = url.SchemeIs(extensions::kExtensionScheme) &&
                                  url.host() == extension->id();
    CHECK(is_extension_url || url.SchemeIsHTTPOrHTTPS());

    // Each devtools extension will need to be able to run in the devtools
    // process. Grant the devtools process the ability to request URLs from the
    // extension.
    content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestOrigin(
        web_contents_->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID(),
        url::Origin::Create(extension->url()));

    base::Value::List runtime_allowed_hosts;
    std::vector<std::string> allowed_hosts =
        management->GetPolicyAllowedHosts(extension.get()).ToStringVector();
    for (auto& host : allowed_hosts) {
      runtime_allowed_hosts.Append(std::move(host));
    }
    base::Value::List runtime_blocked_hosts;
    std::vector<std::string> blocked_hosts =
        management->GetPolicyBlockedHosts(extension.get()).ToStringVector();
    for (auto& host : blocked_hosts) {
      runtime_blocked_hosts.Append(std::move(host));
    }

    base::Value::Dict extension_info;
    extension_info.Set("startPage", url.spec());
    extension_info.Set("name", extension->name());
    extension_info.Set("exposeExperimentalAPIs",
                       extension->permissions_data()->HasAPIPermission(
                           extensions::mojom::APIPermissionID::kExperimental));
    extension_info.Set("allowFileAccess", extensions::util::AllowFileAccess(
                                              extension->id(), profile_));
    extension_info.Set(
        "hostsPolicy",
        base::Value::Dict()
            .Set("runtimeAllowedHosts", std::move(runtime_allowed_hosts))
            .Set("runtimeBlockedHosts", std::move(runtime_blocked_hosts)));
    results.Append(std::move(extension_info));

    if (!(extensions::Manifest::IsPolicyLocation(extension->location()) ||
          extensions::Manifest::IsComponentLocation(extension->location()))) {
      have_user_installed_devtools_extensions = true;
    }
  }

  if (have_user_installed_devtools_extensions) {
    bool is_developer_mode =
        profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode);
    base::UmaHistogramBoolean("Extensions.DevTools.UserIsInDeveloperMode",
                              is_developer_mode);
  }

  CallClientMethod("DevToolsAPI", "setOriginsForbiddenForExtensions",
                   base::Value(std::move(forbidden_origins)));
  CallClientMethod("DevToolsAPI", "addExtensions",
                   base::Value(std::move(results)));
#endif
}

void DevToolsUIBindings::RegisterExtensionsAPI(const std::string& origin,
                                               const std::string& script) {
  extensions_api_[origin + "/"] = script;
}

namespace {

void ShowSurveyCallback(DevToolsUIBindings::DispatchCallback callback,
                        bool survey_shown) {
  base::Value::Dict response_dict;
  response_dict.Set("surveyShown", survey_shown);
  base::Value response = base::Value(std::move(response_dict));
  std::move(callback).Run(&response);
}

}  // namespace

void DevToolsUIBindings::ShowSurvey(DispatchCallback callback,
                                    const std::string& trigger) {
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_->GetOriginalProfile(), true);
  if (!hats_service) {
    ShowSurveyCallback(std::move(callback), false);
    return;
  }
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  hats_service->LaunchSurvey(
      trigger,
      base::BindOnce(ShowSurveyCallback, std::move(split_callback.first), true),
      base::BindOnce(ShowSurveyCallback, std::move(split_callback.second),
                     false));
}

void DevToolsUIBindings::CanShowSurvey(DispatchCallback callback,
                                       const std::string& trigger) {
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_->GetOriginalProfile(), true);
  bool can_show = hats_service ? hats_service->CanShowSurvey(trigger) : false;
  base::Value::Dict response_dict;
  response_dict.Set("canShowSurvey", can_show);
  base::Value response = base::Value(std::move(response_dict));
  std::move(callback).Run(&response);
}

bool DevToolsUIBindings::EnsureAidaClientAvailable() {
  if (!can_access_aida_ || AidaClient::CanUseAida(profile_).blocked) {
    return false;
  }
  if (!aida_client_) {
    aida_client_ = std::make_unique<AidaClient>(profile_);
  }
  return true;
}

void DevToolsUIBindings::HandleAidaClientUnavailable(
    DispatchCallback callback) {
  base::Value::Dict response_dict;
  response_dict.Set("error", "AIDA request was blocked");
  base::Value response = base::Value(std::move(response_dict));
  std::move(callback).Run(&response);
}

void DevToolsUIBindings::DoAidaConversation(DispatchCallback callback,
                                            const std::string& request,
                                            int stream_id) {
  if (!EnsureAidaClientAvailable()) {
    HandleAidaClientUnavailable(std::move(callback));
    return;
  }
  aida_client_->PrepareRequestOrFail(base::BindOnce(
      &DevToolsUIBindings::OnAidaConversationRequest, base::Unretained(this),
      std::move(callback), stream_id, request, base::TimeDelta()));
}

void DevToolsUIBindings::AidaCodeComplete(DispatchCallback callback,
                                          const std::string& request) {
  if (!EnsureAidaClientAvailable()) {
    HandleAidaClientUnavailable(std::move(callback));
    return;
  }
  aida_client_->PrepareRequestOrFail(base::BindOnce(
      &DevToolsUIBindings::OnAidaRequest, base::Unretained(this),
      GURL(AidaClient::kCompleteCodeUrl),
      "DevTools.AidaCodeCompleteResponseTime", std::move(callback), request));
}

void DevToolsUIBindings::RegisterAidaClientEvent(DispatchCallback callback,
                                                 const std::string& request) {
  if (!EnsureAidaClientAvailable()) {
    HandleAidaClientUnavailable(std::move(callback));
    return;
  }
  aida_client_->PrepareRequestOrFail(
      base::BindOnce(&DevToolsUIBindings::OnAidaRequest, base::Unretained(this),
                     GURL(AidaClient::kRegisterClientEventUrl),
                     "DevTools.RegisterAidaClientEventResponseTime",
                     std::move(callback), request));
}

void DevToolsUIBindings::SetDelegate(Delegate* delegate) {
  delegate_.reset(delegate);
  MaybeStartLogging();
}

void DevToolsUIBindings::TransferDelegate(DevToolsUIBindings& other) {
  std::swap(delegate_, other.delegate_);
  if (auto agent_host = agent_host_) {
    Detach();
    other.AttachTo(agent_host);
  }
}

void DevToolsUIBindings::AttachTo(
    const scoped_refptr<content::DevToolsAgentHost>& agent_host) {
  if (agent_host_.get()) {
    Detach();
  }
  agent_host_ = agent_host;
  InnerAttach();
}

void DevToolsUIBindings::AttachViaBrowserTarget(
    const scoped_refptr<content::DevToolsAgentHost>& agent_host) {
  DCHECK(!agent_host_ ||
         agent_host->GetType() == content::DevToolsAgentHost::kTypeBrowser);
  if (!agent_host_) {
    agent_host_ = content::DevToolsAgentHost::CreateForBrowser(
        nullptr /* tethering_task_runner */,
        content::DevToolsAgentHost::CreateServerSocketCallback());
  }
  initial_target_id_ = agent_host->GetId();
  InnerAttach();
}

void DevToolsUIBindings::Detach() {
  if (agent_host_.get()) {
    agent_host_->DetachClient(this);
  }
  agent_host_.reset();
}

bool DevToolsUIBindings::IsAttachedTo(content::DevToolsAgentHost* agent_host) {
  // TODO(caseq): find better way to track attached targets.
  return initial_target_id_.empty() ? agent_host_.get() == agent_host
                                    : initial_target_id_ == agent_host->GetId();
}

#if !BUILDFLAG(IS_ANDROID)
void DevToolsUIBindings::OnThemeChanged() {
  CallClientMethod("DevToolsAPI", "colorThemeChanged");
}
#endif

void DevToolsUIBindings::CallClientMethod(
    const std::string& object_name,
    const std::string& method_name,
    base::Value arg1,
    base::Value arg2,
    base::Value arg3,
    base::OnceCallback<void(base::Value)> completion_callback) {
  // If we're not exposing bindings, we shouldn't call functions either.
  if (!frontend_host_) {
    return;
  }
  // If the client renderer is gone (e.g., the window was closed with both the
  // inspector and client being destroyed), the message can not be sent.
  if (!web_contents_->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    return;
  }
  base::Value::List arguments;
  if (!arg1.is_none()) {
    arguments.Append(std::move(arg1));
    if (!arg2.is_none()) {
      arguments.Append(std::move(arg2));
      if (!arg3.is_none()) {
        arguments.Append(std::move(arg3));
      }
    }
  }
  web_contents_->GetPrimaryMainFrame()->ExecuteJavaScriptMethod(
      base::ASCIIToUTF16(object_name), base::ASCIIToUTF16(method_name),
      std::move(arguments), std::move(completion_callback));
}

void DevToolsUIBindings::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    if (frontend_loaded_ && agent_host_.get()) {
      agent_host_->DetachClient(this);
      InnerAttach();
    }
    if (!IsValidFrontendURL(navigation_handle->GetURL())) {
      LOG(ERROR) << "Attempt to navigate to an invalid DevTools front-end URL: "
                 << navigation_handle->GetURL().spec();
      frontend_host_.reset();
      return;
    }
    if (frontend_host_) {
      return;
    }
    if (content::RenderFrameHost* opener = web_contents_->GetOpener()) {
      content::WebContents* opener_wc =
          content::WebContents::FromRenderFrameHost(opener);
      DevToolsUIBindings* opener_bindings =
          opener_wc ? DevToolsUIBindings::ForWebContents(opener_wc) : nullptr;
      if (!opener_bindings || !opener_bindings->frontend_host_) {
        return;
      }
    }
    frontend_host_ = content::DevToolsFrontendHost::Create(
        navigation_handle->GetRenderFrameHost(),
        base::BindRepeating(
            &DevToolsUIBindings::HandleMessageFromDevToolsFrontend,
            base::Unretained(this)));
    return;
  }

  content::RenderFrameHost* frame = navigation_handle->GetRenderFrameHost();
  std::string origin =
      navigation_handle->GetURL().DeprecatedGetOriginAsURL().spec();
  auto it = extensions_api_.find(origin);
  if (it == extensions_api_.end()) {
    return;
  }
  std::string script = base::StringPrintf(
      "%s(\"%s\")", it->second.c_str(),
      base::Uuid::GenerateRandomV4().AsLowercaseString().c_str());
  content::DevToolsFrontendHost::SetupExtensionsAPI(frame, script);
}

void DevToolsUIBindings::DocumentOnLoadCompletedInPrimaryMainFrame() {
  FrontendLoaded();
}

void DevToolsUIBindings::PrimaryPageChanged() {
  frontend_loaded_ = false;
}

void DevToolsUIBindings::FrontendLoaded() {
  if (frontend_loaded_) {
    return;
  }
  frontend_loaded_ = true;

  // Call delegate first - it seeds importants bit of information.
  delegate_->OnLoadCompleted();

  if (!initial_target_id_.empty()) {
    CallClientMethod("DevToolsAPI", "setInitialTargetId",
                     base::Value(initial_target_id_));
  }
  AddDevToolsExtensionsToClient();
}

DevToolsUIBindings::DevToolsUIBindingsList&
DevToolsUIBindings::GetDevToolsUIBindings() {
  static base::NoDestructor<DevToolsUIBindings::DevToolsUIBindingsList>
      bindings;
  return *bindings;
}
