// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_ui_bindings.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_file_watcher.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/url_constants.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
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
#include "content/public/browser/notification_source.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "google_apis/google_api_keys.h"
#include "ipc/ipc_channel.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;

namespace content {
struct LoadCommittedDetails;
struct FrameNavigateParams;
}

namespace {

static const char kFrontendHostId[] = "id";
static const char kFrontendHostMethod[] = "method";
static const char kFrontendHostParams[] = "params";
static const char kTitleFormat[] = "DevTools - %s";

static const char kRemotePageActionInspect[] = "inspect";
static const char kRemotePageActionReload[] = "reload";
static const char kRemotePageActionActivate[] = "activate";
static const char kRemotePageActionClose[] = "close";

static const char kConfigDiscoverUsbDevices[] = "discoverUsbDevices";
static const char kConfigPortForwardingEnabled[] = "portForwardingEnabled";
static const char kConfigPortForwardingConfig[] = "portForwardingConfig";
static const char kConfigNetworkDiscoveryEnabled[] = "networkDiscoveryEnabled";
static const char kConfigNetworkDiscoveryConfig[] = "networkDiscoveryConfig";

// This constant should be in sync with
// the constant
// kShellMaxMessageChunkSize in content/shell/browser/shell_devtools_bindings.cc
// and
// kLayoutTestMaxMessageChunkSize in
// content/shell/browser/layout_test/devtools_protocol_test_bindings.cc.
const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;

base::Value::Dict CreateFileSystemValue(
    DevToolsFileHelper::FileSystem file_system) {
  base::Value::Dict file_system_value;
  file_system_value.Set("type", file_system.type);
  file_system_value.Set("fileSystemName", file_system.file_system_name);
  file_system_value.Set("rootURL", file_system.root_url);
  file_system_value.Set("fileSystemPath", file_system.file_system_path);
  return file_system_value;
}

Browser* FindBrowser(content::WebContents* web_contents) {
  for (auto* browser : *BrowserList::GetInstance()) {
    int tab_index = browser->tab_strip_model()->GetIndexOfWebContents(
        web_contents);
    if (tab_index != TabStripModel::kNoTab)
      return browser;
  }
  return nullptr;
}

// DevToolsUIDefaultDelegate --------------------------------------------------

class DefaultBindingsDelegate : public DevToolsUIBindings::Delegate {
 public:
  explicit DefaultBindingsDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  DefaultBindingsDelegate(const DefaultBindingsDelegate&) = delete;
  DefaultBindingsDelegate& operator=(const DefaultBindingsDelegate&) = delete;

 private:
  ~DefaultBindingsDelegate() override {}

  void ActivateWindow() override;
  void CloseWindow() override {}
  void Inspect(scoped_refptr<content::DevToolsAgentHost> host) override {}
  void SetInspectedPageBounds(const gfx::Rect& rect) override {}
  void InspectElementCompleted() override {}
  void SetIsDocked(bool is_docked) override {}
  void OpenInNewTab(const std::string& url) override;
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
  content::WebContents* web_contents_;
};

void DefaultBindingsDelegate::ActivateWindow() {
  web_contents_->GetDelegate()->ActivateContents(web_contents_);
  web_contents_->Focus();
}

void DefaultBindingsDelegate::OpenInNewTab(const std::string& url) {
  content::OpenURLParams params(GURL(url), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  Browser* browser = FindBrowser(web_contents_);
  browser->OpenURL(params);
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
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value))
    headers.Set(name, value);

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
    if (!(revision[i] == '@' && i == 0)
        && !(revision[i] >= '0' && revision[i] <= '9')
        && !(revision[i] >= 'a' && revision[i] <= 'z')
        && !(revision[i] >= 'A' && revision[i] <= 'Z')) {
      return std::string();
    }
  }
  return revision;
}

std::string SanitizeRemoteVersion(const std::string& remoteVersion) {
  for (size_t i = 0; i < remoteVersion.length(); i++) {
    if (remoteVersion[i] != '.' &&
        !(remoteVersion[i] >= '0' && remoteVersion[i] <= '9'))
      return std::string();
  }
  return remoteVersion;
}

std::string SanitizeFrontendPath(const std::string& path) {
  for (size_t i = 0; i < path.length(); i++) {
    if (path[i] != '/' && path[i] != '-' && path[i] != '_'
        && path[i] != '.' && path[i] != '@'
        && !(path[i] >= '0' && path[i] <= '9')
        && !(path[i] >= 'a' && path[i] <= 'z')
        && !(path[i] >= 'A' && path[i] <= 'Z')) {
      return std::string();
    }
  }
  return path;
}

std::string SanitizeEndpoint(const std::string& value) {
  if (value.find('&') != std::string::npos
      || value.find('?') != std::string::npos)
    return std::string();
  return value;
}

std::string SanitizeRemoteBase(const std::string& value) {
  GURL url(value);
  std::string path = url.path();
  std::vector<std::string> parts = base::SplitString(
      path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string revision = parts.size() > 2 ? parts[2] : "";
  revision = SanitizeRevision(revision);
  path = base::StringPrintf("/%s/%s/", kRemoteFrontendPath, revision.c_str());
  return SanitizeFrontendURL(url, url::kHttpsScheme,
                             kRemoteFrontendDomain, path, false).spec();
}

std::string SanitizeRemoteFrontendURL(const std::string& value) {
  GURL url(base::UnescapeBinaryURLComponent(
      value, base::UnescapeRule::REPLACE_PLUS_WITH_SPACE));
  std::string path = url.path();
  std::vector<std::string> parts = base::SplitString(
      path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string revision = parts.size() > 2 ? parts[2] : "";
  revision = SanitizeRevision(revision);
  std::string filename = !parts.empty() ? parts[parts.size() - 1] : "";
  if (filename != "devtools.html")
    filename = "inspector.html";
  path = base::StringPrintf("/serve_rev/%s/%s",
                            revision.c_str(), filename.c_str());
  std::string sanitized = SanitizeFrontendURL(url, url::kHttpsScheme,
      kRemoteFrontendDomain, path, true).spec();
  return base::EscapeQueryParamValue(sanitized, false);
}

std::string SanitizeEnabledExperiments(const std::string& value) {
  const auto is_legal = [](char ch) {
    return base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch) || ch == ';' ||
           ch == '_';
  };
  return base::ranges::all_of(value, is_legal) ? value : std::string();
}

std::string SanitizeFrontendQueryParam(
    const std::string& key,
    const std::string& value) {
  // Convert boolean flags to true.
  if (key == "can_dock" || key == "debugFrontend" || key == "isSharedWorker" ||
      key == "v8only" || key == "remoteFrontend" || key == "nodeFrontend" ||
      key == "hasOtherClients" || key == "uiDevTools" ||
      key == "browserConnection") {
    return "true";
  }

  // Pass connection endpoints as is.
  if (key == "ws" || key == "service-backend")
    return SanitizeEndpoint(value);

  if (key == "panel" &&
      (value == "elements" || value == "console" || value == "sources"))
    return value;

  if (key == "remoteBase")
    return SanitizeRemoteBase(value);

  if (key == "remoteFrontendUrl")
    return SanitizeRemoteFrontendURL(value);

  if (key == "remoteVersion")
    return SanitizeRemoteVersion(value);

  if (key == "enabledExperiments")
    return SanitizeEnabledExperiments(value);

  if (key == "targetType" && value == "tab")
    return value;

  if (key == "consolePaste" && value == "blockwebui") {
    return value;
  }

  if (key == "noJavaScriptCompletion" && value == "true") {
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
    if (url.has_ref() && url.ref_piece().find('\'') == base::StringPiece::npos)
      fragment = '#' + url.ref();
  }
  std::string query =
      query_parts.empty() ? "" : "?" + base::JoinString(query_parts, "&");
  std::string constructed =
      base::StringPrintf("%s://%s%s%s%s", scheme.c_str(), host.c_str(),
                         path.c_str(), query.c_str(), fragment.c_str());
  GURL result = GURL(constructed);
  if (!result.is_valid())
    return GURL();
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
                     base::TimeDelta retry_delay = base::TimeDelta()) {
    auto resource_loader =
        std::make_unique<DevToolsUIBindings::NetworkResourceLoader>(
            stream_id, bindings, resource_request, traffic_annotation,
            std::move(url_loader_factory), std::move(callback), retry_delay);
    bindings->loaders_.insert(std::move(resource_loader));
  }

  NetworkResourceLoader(
      int stream_id,
      DevToolsUIBindings* bindings,
      const network::ResourceRequest& resource_request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      URLLoaderFactoryHolder url_loader_factory,
      DispatchCallback callback,
      base::TimeDelta delay)
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
    loader_->SetOnResponseStartedCallback(base::BindOnce(
        &NetworkResourceLoader::OnResponseStarted, base::Unretained(this)));
    timer_.Start(FROM_HERE, delay,
                 base::BindOnce(&NetworkResourceLoader::DownloadAsStream,
                                base::Unretained(this)));
  }

  NetworkResourceLoader(const NetworkResourceLoader&) = delete;
  NetworkResourceLoader& operator=(const NetworkResourceLoader&) = delete;

 private:
  void DownloadAsStream() {
    loader_->DownloadAsStream(url_loader_factory_.get(), this);
  }

  base::TimeDelta GetNextExponentialBackoffDelay(const base::TimeDelta& delta) {
    if (delta.is_zero()) {
      return kInitialBackoffDelay;
    } else {
      return delta * 1.3;
    }
  }

  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head) {
    response_headers_ = response_head.headers;
  }

  void OnDataReceived(base::StringPiece chunk,
                      base::OnceClosure resume) override {
    base::Value chunkValue;

    bool encoded = !base::IsStringUTF8AllowingNoncharacters(chunk);
    if (encoded) {
      std::string encoded_string;
      base::Base64Encode(chunk, &encoded_string);
      chunkValue = base::Value(std::move(encoded_string));
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
  DevToolsUIBindings* const bindings_;
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

  DevToolsUIBindings* devtools_bindings_;
};

DevToolsUIBindings::FrontendWebContentsObserver::FrontendWebContentsObserver(
    DevToolsUIBindings* devtools_ui_bindings)
    : WebContentsObserver(devtools_ui_bindings->web_contents()),
      devtools_bindings_(devtools_ui_bindings) {
}

DevToolsUIBindings::FrontendWebContentsObserver::
    ~FrontendWebContentsObserver() {
}

// static
GURL DevToolsUIBindings::SanitizeFrontendURL(const GURL& url) {
  return ::SanitizeFrontendURL(url, content::kChromeDevToolsScheme,
      chrome::kChromeUIDevToolsHost, SanitizeFrontendPath(url.path()), true);
}

// static
bool DevToolsUIBindings::IsValidFrontendURL(const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme) &&
      url.host() == content::kChromeUITracingHost &&
      !url.has_query() && !url.has_ref()) {
    return true;
  }

  return SanitizeFrontendURL(url).spec() == url.spec();
}

bool DevToolsUIBindings::IsValidRemoteFrontendURL(const GURL& url) {
  return ::SanitizeFrontendURL(url, url::kHttpsScheme, kRemoteFrontendDomain,
                               url.path(), true)
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
#if BUILDFLAG(IS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
#endif
      if (devtools_bindings_->agent_host_.get())
        devtools_bindings_->Detach();
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
  for (DevToolsUIBindings* binding : instances) {
    if (binding->web_contents() == web_contents)
      return binding;
  }
  return nullptr;
}

std::string DevToolsUIBindings::GetTypeForMetrics() {
  return "DevTools";
}

DevToolsUIBindings::DevToolsUIBindings(content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      android_bridge_(DevToolsAndroidBridge::Factory::GetForProfile(profile_)),
      web_contents_(web_contents),
      delegate_(new DefaultBindingsDelegate(web_contents_)),
      devices_updates_enabled_(false),
      frontend_loaded_(false),
      settings_(profile_) {
  DevToolsUIBindings::GetDevToolsUIBindings().push_back(this);
  frontend_contents_observer_ =
      std::make_unique<FrontendWebContentsObserver>(this);

  file_helper_ =
      std::make_unique<DevToolsFileHelper>(web_contents_, profile_, this);
  file_system_indexer_ = new DevToolsFileSystemIndexer();

  // Register on-load actions.
  embedder_message_dispatcher_ =
      DevToolsEmbedderMessageDispatcher::CreateForDevToolsFrontend(this);
}

DevToolsUIBindings::~DevToolsUIBindings() {
  if (agent_host_.get())
    agent_host_->DetachClient(this);

  for (IndexingJobsMap::const_iterator jobs_it(indexing_jobs_.begin());
       jobs_it != indexing_jobs_.end(); ++jobs_it) {
    jobs_it->second->Stop();
  }
  indexing_jobs_.clear();
  SetDevicesUpdatesEnabled(false);

  // Remove self from global list.
  DevToolsUIBindingsList& instances =
      DevToolsUIBindings::GetDevToolsUIBindings();
  auto it = base::ranges::find(instances, this);
  DCHECK(it != instances.end());
  instances.erase(it);
}

// content::DevToolsFrontendHost::Delegate implementation ---------------------
void DevToolsUIBindings::HandleMessageFromDevToolsFrontend(
    base::Value::Dict message) {
  if (!frontend_host_)
    return;
  const std::string* method = message.FindString(kFrontendHostMethod);
  base::Value* params = message.Find(kFrontendHostParams);
  if (!method || (params && !params->is_list())) {
    LOG(ERROR) << "Invalid message was sent to embedder: " << message;
    return;
  }
  int id = message.FindInt(kFrontendHostId).value_or(0);
  base::Value::List params_list;
  if (params)
    params_list = std::move(*params).TakeList();
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
  if (!frontend_host_)
    return;

  base::StringPiece message_sp(reinterpret_cast<const char*>(message.data()),
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
}

void DevToolsUIBindings::SetInspectedPageBounds(const gfx::Rect& rect) {
  delegate_->SetInspectedPageBounds(rect);
}

void DevToolsUIBindings::SetIsDocked(DispatchCallback callback,
                                     bool dock_requested) {
  delegate_->SetIsDocked(dock_requested);
  std::move(callback).Run(nullptr);
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
    content::WebContents* target_tab =
        DevToolsWindow::AsDevToolsWindow(web_contents_)
            ->GetInspectedWebContents();
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
        target_tab->GetLastCommittedURL().scheme() == gurl.scheme()) {
      std::vector<std::string> allowed_webui_hosts;
      content::RenderFrameHost* frame_host =
          web_contents()->GetPrimaryMainFrame();

      mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote =
          content::CreateWebUIURLLoaderFactory(
              frame_host, target_tab->GetLastCommittedURL().scheme(),
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
    content::WebContents* target_tab =
        DevToolsWindow::AsDevToolsWindow(web_contents_)
            ->GetInspectedWebContents();
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

void DevToolsUIBindings::ShowItemInFolder(const std::string& file_system_path) {
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  file_helper_->ShowItemInFolder(file_system_path);
}

void DevToolsUIBindings::SaveToFile(const std::string& url,
                                    const std::string& content,
                                    bool save_as) {
  file_helper_->Save(url, content, save_as,
                     base::BindOnce(&DevToolsUIBindings::FileSavedAs,
                                    weak_factory_.GetWeakPtr(), url),
                     base::BindOnce(&DevToolsUIBindings::CanceledFileSaveAs,
                                    weak_factory_.GetWeakPtr(), url));
}

void DevToolsUIBindings::AppendToFile(const std::string& url,
                                      const std::string& content) {
  file_helper_->Append(url, content,
                       base::BindOnce(&DevToolsUIBindings::AppendedTo,
                                      weak_factory_.GetWeakPtr(), url));
}

void DevToolsUIBindings::RequestFileSystems() {
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  base::Value::List file_systems_value;
  for (auto const& file_system : file_helper_->GetFileSystems())
    file_systems_value.Append(CreateFileSystemValue(file_system));
  CallClientMethod("DevToolsAPI", "fileSystemsLoaded",
                   base::Value(std::move(file_systems_value)));
}

void DevToolsUIBindings::AddFileSystem(const std::string& type) {
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  file_helper_->AddFileSystem(
      type, base::BindRepeating(&DevToolsUIBindings::ShowDevToolsInfoBar,
                                weak_factory_.GetWeakPtr()));
}

void DevToolsUIBindings::RemoveFileSystem(const std::string& file_system_path) {
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  file_helper_->RemoveFileSystem(file_system_path);
}

void DevToolsUIBindings::UpgradeDraggedFileSystemPermissions(
    const std::string& file_system_url) {
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  file_helper_->UpgradeDraggedFileSystemPermissions(
      file_system_url,
      base::BindRepeating(&DevToolsUIBindings::ShowDevToolsInfoBar,
                          weak_factory_.GetWeakPtr()));
}

void DevToolsUIBindings::IndexPath(
    int index_request_id,
    const std::string& file_system_path,
    const std::string& excluded_folders_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    IndexingDone(index_request_id, file_system_path);
    return;
  }
  if (indexing_jobs_.count(index_request_id) != 0)
    return;
  std::vector<std::string> excluded_folders;
  absl::optional<base::Value> parsed_excluded_folders =
      base::JSONReader::Read(excluded_folders_message);
  if (parsed_excluded_folders && parsed_excluded_folders->is_list()) {
    for (const base::Value& folder_path : parsed_excluded_folders->GetList()) {
      if (folder_path.is_string())
        excluded_folders.push_back(folder_path.GetString());
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
  if (it == indexing_jobs_.end())
    return;
  it->second->Stop();
  indexing_jobs_.erase(it);
}

void DevToolsUIBindings::SearchInPath(int search_request_id,
                                      const std::string& file_system_path,
                                      const std::string& query) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsValidFrontendURL(web_contents_->GetLastCommittedURL()) &&
        frontend_host_);
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    SearchCompleted(search_request_id,
                    file_system_path,
                    std::vector<std::string>());
    return;
  }
  file_system_indexer_->SearchInPath(file_system_path,
                                     query,
                                     BindOnce(&DevToolsUIBindings::SearchCompleted,
                                              weak_factory_.GetWeakPtr(),
                                              search_request_id,
                                              file_system_path));
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
  absl::optional<base::Value> parsed_port_forwarding =
      base::JSONReader::Read(port_forwarding_config);
  if (!parsed_port_forwarding || !parsed_port_forwarding->is_dict())
    return;
  absl::optional<base::Value> parsed_network =
      base::JSONReader::Read(network_discovery_config);
  if (!parsed_network || !parsed_network->is_list())
    return;
  profile_->GetPrefs()->SetBoolean(
      prefs::kDevToolsDiscoverUsbDevicesEnabled, discover_usb_devices);
  profile_->GetPrefs()->SetBoolean(
      prefs::kDevToolsPortForwardingEnabled, port_forwarding_enabled);
  profile_->GetPrefs()->Set(prefs::kDevToolsPortForwardingConfig,
                            *parsed_port_forwarding);
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
  if (devices_updates_enabled_ == enabled)
    return;
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
}

void DevToolsUIBindings::PerformActionOnRemotePage(const std::string& page_id,
                                                   const std::string& action) {
  if (!remote_targets_handler_)
    return;
  scoped_refptr<content::DevToolsAgentHost> host =
      remote_targets_handler_->GetTarget(page_id);
  if (!host)
    return;
  if (action == kRemotePageActionInspect)
    delegate_->Inspect(host);
  else if (action == kRemotePageActionReload)
    host->Reload();
  else if (action == kRemotePageActionActivate)
    host->Activate();
  else if (action == kRemotePageActionClose)
    host->Close();
}

void DevToolsUIBindings::OpenRemotePage(const std::string& browser_id,
                                        const std::string& url) {
  if (!remote_targets_handler_)
    return;
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

  result.Set("isSyncActive", sync_service->IsSyncFeatureActive());
  result.Set("arePreferencesSynced", sync_service->GetActiveDataTypes().Has(
                                         syncer::ModelType::PREFERENCES));

  CoreAccountInfo account_info = sync_service->GetAccountInfo();
  if (account_info.IsEmpty())
    return result;

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
  if (png_bytes->size() > 0)
    result.Set("accountImage", base::Base64Encode(*png_bytes));

  return result;
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
  if (!agent_host_)
    return;
  agent_host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(message)));
}

void DevToolsUIBindings::RecordCountHistogram(const std::string& name,
                                              int sample,
                                              int min,
                                              int exclusive_max,
                                              int buckets) {
  if (!frontend_host_) {
    return;
  }

  if (!(min <= sample && sample < exclusive_max && buckets >= 3)) {
    frontend_host_->BadMessageReceived();
    return;
  }

  base::UmaHistogramCustomCounts(name, sample, min, exclusive_max, buckets);
}

void DevToolsUIBindings::RecordEnumeratedHistogram(const std::string& name,
                                                   int sample,
                                                   int boundary_value) {
  if (!frontend_host_)
    return;
  if (!(boundary_value >= 0 && boundary_value <= 1000 && sample >= 0 &&
        sample < boundary_value)) {
    // TODO(nick): Replace with chrome::bad_message::ReceivedBadMessage().
    frontend_host_->BadMessageReceived();
    return;
  }

  const std::string kDevToolsHistogramPrefix = "DevTools.";
  if (name.compare(0, kDevToolsHistogramPrefix.size(),
                   kDevToolsHistogramPrefix) == 0)
    base::UmaHistogramExactLinear(name, sample, boundary_value);
  else
    frontend_host_->BadMessageReceived();
}

void DevToolsUIBindings::RecordPerformanceHistogram(const std::string& name,
                                                    double duration) {
  if (!frontend_host_)
    return;
  if (duration < 0) {
    return;
  }
  // Use histogram_functions.h instead of macros as the name comes from the
  // DevTools frontend javascript and so will always have the same call site.
  base::TimeDelta delta = base::Milliseconds(duration);
  base::UmaHistogramTimes(name, delta);
}

void DevToolsUIBindings::RecordUserMetricsAction(const std::string& name) {
  if (!frontend_host_)
    return;
  // Use RecordComputedAction instead of RecordAction as the name comes from
  // DevTools frontend javascript and so will always have the same call site.
  base::RecordComputedAction(name);
}

void DevToolsUIBindings::SendJsonRequest(DispatchCallback callback,
                                         const std::string& browser_id,
                                         const std::string& url) {
  if (!android_bridge_) {
    std::move(callback).Run(nullptr);
    return;
  }
  android_bridge_->SendJsonRequest(
      browser_id, url,
      base::BindOnce(&DevToolsUIBindings::JsonReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DevToolsUIBindings::JsonReceived(DispatchCallback callback,
                                      int result,
                                      const std::string& message) {
  if (result != net::OK) {
    std::move(callback).Run(nullptr);
    return;
  }
  base::Value message_value(message);
  std::move(callback).Run(&message_value);
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
  for (auto const& file_path : file_paths)
    file_paths_value.Append(file_path);
  CallClientMethod("DevToolsAPI", "searchCompleted", base::Value(request_id),
                   base::Value(file_system_path),
                   base::Value(std::move(file_paths_value)));
}

void DevToolsUIBindings::ShowDevToolsInfoBar(
    const std::u16string& message,
    DevToolsInfoBarDelegate::Callback callback) {
  if (!delegate_->GetInfoBarManager()) {
    std::move(callback).Run(false);
    return;
  }
  DevToolsInfoBarDelegate::Create(message, std::move(callback));
}

void DevToolsUIBindings::AddDevToolsExtensionsToClient() {
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_->GetOriginalProfile());
  if (!registry)
    return;

  base::Value::List results;
  base::Value::List component_extension_origins;
  bool have_user_installed_devtools_extensions = false;
  extensions::ExtensionManagement* management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext());
  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    if (extensions::Manifest::IsComponentLocation(extension->location())) {
      component_extension_origins.Append(extension->origin().Serialize());
    }
    if (extensions::chrome_manifest_urls::GetDevToolsPage(extension.get())
            .is_empty()) {
      continue;
    }
    GURL url =
        extensions::chrome_manifest_urls::GetDevToolsPage(extension.get());
    const bool is_extension_url = url.SchemeIs(extensions::kExtensionScheme) &&
                                  url.host_piece() == extension->id();
    CHECK(is_extension_url || url.SchemeIsHTTPOrHTTPS());

    // Each devtools extension will need to be able to run in the devtools
    // process. Grant the devtools process the ability to request URLs from the
    // extension.
    content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestOrigin(
        web_contents_->GetPrimaryMainFrame()->GetProcess()->GetID(),
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
                   base::Value(std::move(component_extension_origins)));
  CallClientMethod("DevToolsAPI", "addExtensions",
                   base::Value(std::move(results)));
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

void DevToolsUIBindings::SetDelegate(Delegate* delegate) {
  delegate_.reset(delegate);
}

void DevToolsUIBindings::AttachTo(
    const scoped_refptr<content::DevToolsAgentHost>& agent_host) {
  if (agent_host_.get())
    Detach();
  agent_host_ = agent_host;
  InnerAttach();
}

void DevToolsUIBindings::AttachViaBrowserTarget(
    const scoped_refptr<content::DevToolsAgentHost>& agent_host) {
  DCHECK(!agent_host_ ||
         agent_host->GetType() == content::DevToolsAgentHost::kTypeBrowser);
  if (!agent_host_)
    agent_host_ = content::DevToolsAgentHost::CreateForBrowser(
        nullptr /* tethering_task_runner */,
        content::DevToolsAgentHost::CreateServerSocketCallback());
  initial_target_id_ = agent_host->GetId();
  InnerAttach();
}

void DevToolsUIBindings::Detach() {
  if (agent_host_.get())
    agent_host_->DetachClient(this);
  agent_host_.reset();
}

bool DevToolsUIBindings::IsAttachedTo(content::DevToolsAgentHost* agent_host) {
  // TODO(caseq): find better way to track attached targets.
  return initial_target_id_.empty() ? agent_host_.get() == agent_host
                                    : initial_target_id_ == agent_host->GetId();
}

void DevToolsUIBindings::CallClientMethod(
    const std::string& object_name,
    const std::string& method_name,
    base::Value arg1,
    base::Value arg2,
    base::Value arg3,
    base::OnceCallback<void(base::Value)> completion_callback) {
  // If we're not exposing bindings, we shouldn't call functions either.
  if (!frontend_host_)
    return;
  // If the client renderer is gone (e.g., the window was closed with both the
  // inspector and client being destroyed), the message can not be sent.
  if (!web_contents_->GetPrimaryMainFrame()->IsRenderFrameLive())
    return;
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
    if (frontend_host_)
      return;
    if (content::RenderFrameHost* opener = web_contents_->GetOpener()) {
      content::WebContents* opener_wc =
          content::WebContents::FromRenderFrameHost(opener);
      DevToolsUIBindings* opener_bindings =
          opener_wc ? DevToolsUIBindings::ForWebContents(opener_wc) : nullptr;
      if (!opener_bindings || !opener_bindings->frontend_host_)
        return;
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
  if (it == extensions_api_.end())
    return;
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
  if (frontend_loaded_)
    return;
  frontend_loaded_ = true;

  // Call delegate first - it seeds importants bit of information.
  delegate_->OnLoadCompleted();

  if (!initial_target_id_.empty())
    CallClientMethod("DevToolsAPI", "setInitialTargetId",
                     base::Value(initial_target_id_));
  AddDevToolsExtensionsToClient();
}

DevToolsUIBindings::DevToolsUIBindingsList&
DevToolsUIBindings::GetDevToolsUIBindings() {
  static base::NoDestructor<DevToolsUIBindings::DevToolsUIBindingsList>
      bindings;
  return *bindings;
}
