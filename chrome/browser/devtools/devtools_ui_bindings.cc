// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_ui_bindings.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_file_watcher.h"
#include "chrome/browser/devtools/url_constants.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/zoom/page_zoom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_channel.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/page_transition_types.h"

using base::DictionaryValue;
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

static const char kDevToolsActionTakenHistogram[] = "DevTools.ActionTaken";
static const char kDevToolsPanelShownHistogram[] = "DevTools.PanelShown";

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

typedef std::vector<DevToolsUIBindings*> DevToolsUIBindingsList;
base::LazyInstance<DevToolsUIBindingsList>::Leaky
    g_devtools_ui_bindings_instances = LAZY_INSTANCE_INITIALIZER;

std::unique_ptr<base::DictionaryValue> CreateFileSystemValue(
    DevToolsFileHelper::FileSystem file_system) {
  auto file_system_value = std::make_unique<base::DictionaryValue>();
  file_system_value->SetString("type", file_system.type);
  file_system_value->SetString("fileSystemName", file_system.file_system_name);
  file_system_value->SetString("rootURL", file_system.root_url);
  file_system_value->SetString("fileSystemPath", file_system.file_system_path);
  return file_system_value;
}

Browser* FindBrowser(content::WebContents* web_contents) {
  for (auto* browser : *BrowserList::GetInstance()) {
    int tab_index = browser->tab_strip_model()->GetIndexOfWebContents(
        web_contents);
    if (tab_index != TabStripModel::kNoTab)
      return browser;
  }
  return NULL;
}

// DevToolsUIDefaultDelegate --------------------------------------------------

class DefaultBindingsDelegate : public DevToolsUIBindings::Delegate {
 public:
  explicit DefaultBindingsDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

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
  InfoBarService* GetInfoBarService() override;
  void RenderProcessGone(bool crashed) override {}
  void ShowCertificateViewer(const std::string& cert_chain) override{};

  content::WebContents* web_contents_;
  DISALLOW_COPY_AND_ASSIGN(DefaultBindingsDelegate);
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

InfoBarService* DefaultBindingsDelegate::GetInfoBarService() {
  return InfoBarService::FromWebContents(web_contents_);
}

std::unique_ptr<base::DictionaryValue> BuildObjectForResponse(
    const net::HttpResponseHeaders* rh) {
  auto response = std::make_unique<base::DictionaryValue>();
  response->SetInteger("statusCode", rh ? rh->response_code() : 200);

  auto headers = std::make_unique<base::DictionaryValue>();
  size_t iterator = 0;
  std::string name;
  std::string value;
  // TODO(caseq): this probably needs to handle duplicate header names
  // correctly by folding them.
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value))
    headers->SetString(name, value);

  response->Set("headers", std::move(headers));
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
  GURL url(net::UnescapeURLComponent(value,
      net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
      net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
      net::UnescapeRule::REPLACE_PLUS_WITH_SPACE));
  std::string path = url.path();
  std::vector<std::string> parts = base::SplitString(
      path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string revision = parts.size() > 2 ? parts[2] : "";
  revision = SanitizeRevision(revision);
  std::string filename = parts.size() ? parts[parts.size() - 1] : "";
  if (filename != "devtools.html")
    filename = "inspector.html";
  path = base::StringPrintf("/serve_rev/%s/%s",
                            revision.c_str(), filename.c_str());
  std::string sanitized = SanitizeFrontendURL(url, url::kHttpsScheme,
      kRemoteFrontendDomain, path, true).spec();
  return net::EscapeQueryParamValue(sanitized, false);
}

std::string SanitizeFrontendQueryParam(
    const std::string& key,
    const std::string& value) {
  // Convert boolean flags to true.
  if (key == "can_dock" || key == "debugFrontend" || key == "experiments" ||
      key == "isSharedWorker" || key == "v8only" || key == "remoteFrontend" ||
      key == "nodeFrontend" || key == "hasOtherClients")
    return "true";

  // Pass connection endpoints as is.
  if (key == "ws" || key == "service-backend")
    return SanitizeEndpoint(value);

  // Only support undocked for old frontends.
  if (key == "dockSide" && value == "undocked")
    return value;

  if (key == "panel" && (value == "elements" || value == "console"))
    return value;

  if (key == "remoteBase")
    return SanitizeRemoteBase(value);

  if (key == "remoteFrontendUrl")
    return SanitizeRemoteFrontendURL(value);

  if (key == "remoteVersion")
    return SanitizeRemoteVersion(value);

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
      std::string value = SanitizeFrontendQueryParam(it.GetKey(),
          it.GetValue());
      if (!value.empty()) {
        query_parts.push_back(
            base::StringPrintf("%s=%s", it.GetKey().c_str(), value.c_str()));
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

}  // namespace

class DevToolsUIBindings::NetworkResourceLoader
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  NetworkResourceLoader(int stream_id,
                        DevToolsUIBindings* bindings,
                        std::unique_ptr<network::SimpleURLLoader> loader,
                        network::mojom::URLLoaderFactory* url_loader_factory,
                        const DispatchCallback& callback)
      : stream_id_(stream_id),
        bindings_(bindings),
        loader_(std::move(loader)),
        callback_(callback) {
    loader_->DownloadAsStream(url_loader_factory, this);
    loader_->SetOnResponseStartedCallback(base::BindOnce(
        &NetworkResourceLoader::OnResponseStarted, base::Unretained(this)));
  }

 private:
  void OnResponseStarted(const GURL& final_url,
                         const network::ResourceResponseHead& response_head) {
    response_headers_ = response_head.headers;
  }

  void OnDataReceived(base::StringPiece chunk,
                      base::OnceClosure resume) override {
    base::Value chunkValue;

    bool encoded = !base::IsStringUTF8(chunk);
    if (encoded) {
      std::string encoded_string;
      base::Base64Encode(chunk, &encoded_string);
      chunkValue = base::Value(std::move(encoded_string));
    } else {
      chunkValue = base::Value(chunk);
    }
    base::Value id(stream_id_);
    base::Value encodedValue(encoded);

    bindings_->CallClientFunction("DevToolsAPI.streamWrite", &id, &chunkValue,
                                  &encodedValue);
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    auto response = BuildObjectForResponse(response_headers_.get());
    callback_.Run(response.get());

    bindings_->loaders_.erase(bindings_->loaders_.find(this));
  }

  void OnRetry(base::OnceClosure start_retry) override { NOTREACHED(); }

  const int stream_id_;
  DevToolsUIBindings* const bindings_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  DispatchCallback callback_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

  DISALLOW_COPY_AND_ASSIGN(NetworkResourceLoader);
};

// DevToolsUIBindings::FrontendWebContentsObserver ----------------------------

class DevToolsUIBindings::FrontendWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit FrontendWebContentsObserver(DevToolsUIBindings* ui_bindings);
  ~FrontendWebContentsObserver() override;

 private:
  // contents::WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentAvailableInMainFrame() override;
  void DocumentOnLoadCompletedInMainFrame() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  DevToolsUIBindings* devtools_bindings_;
  DISALLOW_COPY_AND_ASSIGN(FrontendWebContentsObserver);
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

void DevToolsUIBindings::FrontendWebContentsObserver::RenderProcessGone(
    base::TerminationStatus status) {
  bool crashed = true;
  switch (status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      if (devtools_bindings_->agent_host_.get())
        devtools_bindings_->Detach();
      break;
    default:
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
    DocumentAvailableInMainFrame() {
  devtools_bindings_->DocumentAvailableInMainFrame();
}

void DevToolsUIBindings::FrontendWebContentsObserver::
    DocumentOnLoadCompletedInMainFrame() {
  devtools_bindings_->DocumentOnLoadCompletedInMainFrame();
}

void DevToolsUIBindings::FrontendWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() && navigation_handle->HasCommitted())
    devtools_bindings_->DidNavigateMainFrame();
}

// DevToolsUIBindings ---------------------------------------------------------

DevToolsUIBindings* DevToolsUIBindings::ForWebContents(
     content::WebContents* web_contents) {
  if (!g_devtools_ui_bindings_instances.IsCreated())
    return NULL;
  DevToolsUIBindingsList* instances =
      g_devtools_ui_bindings_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->web_contents() == web_contents)
      return *it;
  }
  return NULL;
}

DevToolsUIBindings::DevToolsUIBindings(content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      android_bridge_(DevToolsAndroidBridge::Factory::GetForProfile(profile_)),
      web_contents_(web_contents),
      delegate_(new DefaultBindingsDelegate(web_contents_)),
      devices_updates_enabled_(false),
      frontend_loaded_(false),
      reloading_(false),
      weak_factory_(this) {
  g_devtools_ui_bindings_instances.Get().push_back(this);
  frontend_contents_observer_.reset(new FrontendWebContentsObserver(this));
  web_contents_->GetMutableRendererPrefs()->can_accept_load_drops = false;

  file_helper_.reset(new DevToolsFileHelper(web_contents_, profile_, this));
  file_system_indexer_ = new DevToolsFileSystemIndexer();
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents_);

  // Register on-load actions.
  embedder_message_dispatcher_.reset(
      DevToolsEmbedderMessageDispatcher::CreateForDevToolsFrontend(this));
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
  DevToolsUIBindingsList* instances =
      g_devtools_ui_bindings_instances.Pointer();
  auto it(std::find(instances->begin(), instances->end(), this));
  DCHECK(it != instances->end());
  instances->erase(it);
}

// content::DevToolsFrontendHost::Delegate implementation ---------------------
void DevToolsUIBindings::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  if (!frontend_host_)
    return;
  std::string method;
  base::ListValue empty_params;
  base::ListValue* params = &empty_params;

  base::DictionaryValue* dict = NULL;
  std::unique_ptr<base::Value> parsed_message = base::JSONReader::Read(message);
  if (!parsed_message ||
      !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString(kFrontendHostMethod, &method) ||
      (dict->HasKey(kFrontendHostParams) &&
          !dict->GetList(kFrontendHostParams, &params))) {
    LOG(ERROR) << "Invalid message was sent to embedder: " << message;
    return;
  }
  int id = 0;
  dict->GetInteger(kFrontendHostId, &id);
  embedder_message_dispatcher_->Dispatch(
      base::Bind(&DevToolsUIBindings::SendMessageAck,
                 weak_factory_.GetWeakPtr(),
                 id),
      method,
      params);
}

// content::DevToolsAgentHostClient implementation --------------------------
void DevToolsUIBindings::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host, const std::string& message) {
  DCHECK(agent_host == agent_host_.get());
  if (!frontend_host_ || reloading_)
    return;

  if (message.length() < kMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(message, true, &param);
    base::string16 javascript =
        base::UTF8ToUTF16("DevToolsAPI.dispatchMessage(" + param + ");");
    web_contents_->GetMainFrame()->ExecuteJavaScript(javascript);
    return;
  }

  base::Value total_size(static_cast<int>(message.length()));
  for (size_t pos = 0; pos < message.length(); pos += kMaxMessageChunkSize) {
    base::Value message_value(message.substr(pos, kMaxMessageChunkSize));
    CallClientFunction("DevToolsAPI.dispatchMessageChunk",
                       &message_value, pos ? NULL : &total_size, NULL);
  }
}

void DevToolsUIBindings::AgentHostClosed(
    content::DevToolsAgentHost* agent_host) {
  DCHECK(agent_host == agent_host_.get());
  agent_host_ = NULL;
  delegate_->InspectedContentsClosing();
}

void DevToolsUIBindings::SendMessageAck(int request_id,
                                        const base::Value* arg) {
  base::Value id_value(request_id);
  CallClientFunction("DevToolsAPI.embedderMessageAck",
                     &id_value, arg, nullptr);
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

void DevToolsUIBindings::SetIsDocked(const DispatchCallback& callback,
                                     bool dock_requested) {
  delegate_->SetIsDocked(dock_requested);
  callback.Run(nullptr);
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

void DevToolsUIBindings::LoadNetworkResource(const DispatchCallback& callback,
                                             const std::string& url,
                                             const std::string& headers,
                                             int stream_id) {
  GURL gurl(url);
  if (!gurl.is_valid()) {
    base::DictionaryValue response;
    response.SetInteger("statusCode", 404);
    callback.Run(&response);
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

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = gurl;
  resource_request->headers.AddHeadersFromString(headers);

  auto* partition = content::BrowserContext::GetStoragePartitionForSite(
      web_contents_->GetBrowserContext(), gurl);
  auto factory = partition->GetURLLoaderFactoryForBrowserProcess();

  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  auto resource_loader = std::make_unique<NetworkResourceLoader>(
      stream_id, this, std::move(simple_url_loader), factory.get(), callback);
  loaders_.insert(std::move(resource_loader));
}

void DevToolsUIBindings::OpenInNewTab(const std::string& url) {
  delegate_->OpenInNewTab(url);
}

void DevToolsUIBindings::ShowItemInFolder(const std::string& file_system_path) {
  CHECK(IsValidFrontendURL(web_contents_->GetURL()) && frontend_host_);
  file_helper_->ShowItemInFolder(file_system_path);
}

void DevToolsUIBindings::SaveToFile(const std::string& url,
                                    const std::string& content,
                                    bool save_as) {
  file_helper_->Save(url, content, save_as,
                     base::Bind(&DevToolsUIBindings::FileSavedAs,
                                weak_factory_.GetWeakPtr(), url),
                     base::Bind(&DevToolsUIBindings::CanceledFileSaveAs,
                                weak_factory_.GetWeakPtr(), url));
}

void DevToolsUIBindings::AppendToFile(const std::string& url,
                                      const std::string& content) {
  file_helper_->Append(url, content,
                       base::Bind(&DevToolsUIBindings::AppendedTo,
                                  weak_factory_.GetWeakPtr(), url));
}

void DevToolsUIBindings::RequestFileSystems() {
  CHECK(IsValidFrontendURL(web_contents_->GetURL()) && frontend_host_);
  std::vector<DevToolsFileHelper::FileSystem> file_systems =
      file_helper_->GetFileSystems();
  base::ListValue file_systems_value;
  for (size_t i = 0; i < file_systems.size(); ++i)
    file_systems_value.Append(CreateFileSystemValue(file_systems[i]));
  CallClientFunction("DevToolsAPI.fileSystemsLoaded",
                     &file_systems_value, NULL, NULL);
}

void DevToolsUIBindings::AddFileSystem(const std::string& type) {
  CHECK(IsValidFrontendURL(web_contents_->GetURL()) && frontend_host_);
  file_helper_->AddFileSystem(
      type, base::Bind(&DevToolsUIBindings::ShowDevToolsInfoBar,
                       weak_factory_.GetWeakPtr()));
}

void DevToolsUIBindings::RemoveFileSystem(const std::string& file_system_path) {
  CHECK(IsValidFrontendURL(web_contents_->GetURL()) && frontend_host_);
  file_helper_->RemoveFileSystem(file_system_path);
}

void DevToolsUIBindings::UpgradeDraggedFileSystemPermissions(
    const std::string& file_system_url) {
  CHECK(IsValidFrontendURL(web_contents_->GetURL()) && frontend_host_);
  file_helper_->UpgradeDraggedFileSystemPermissions(
      file_system_url, base::Bind(&DevToolsUIBindings::ShowDevToolsInfoBar,
                                  weak_factory_.GetWeakPtr()));
}

void DevToolsUIBindings::IndexPath(
    int index_request_id,
    const std::string& file_system_path,
    const std::string& excluded_folders_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsValidFrontendURL(web_contents_->GetURL()) && frontend_host_);
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    IndexingDone(index_request_id, file_system_path);
    return;
  }
  if (indexing_jobs_.count(index_request_id) != 0)
    return;
  std::vector<std::string> excluded_folders;
  std::unique_ptr<base::Value> parsed_excluded_folders =
      base::JSONReader::Read(excluded_folders_message);
  if (parsed_excluded_folders && parsed_excluded_folders->is_list()) {
    const std::vector<base::Value>& folder_paths =
        parsed_excluded_folders->GetList();
    for (const base::Value& folder_path : folder_paths) {
      if (folder_path.is_string())
        excluded_folders.push_back(folder_path.GetString());
    }
  }

  indexing_jobs_[index_request_id] =
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob>(
          file_system_indexer_->IndexPath(
              file_system_path, excluded_folders,
              Bind(&DevToolsUIBindings::IndexingTotalWorkCalculated,
                   weak_factory_.GetWeakPtr(), index_request_id,
                   file_system_path),
              Bind(&DevToolsUIBindings::IndexingWorked,
                   weak_factory_.GetWeakPtr(), index_request_id,
                   file_system_path),
              Bind(&DevToolsUIBindings::IndexingDone,
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
  CHECK(IsValidFrontendURL(web_contents_->GetURL()) && frontend_host_);
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    SearchCompleted(search_request_id,
                    file_system_path,
                    std::vector<std::string>());
    return;
  }
  file_system_indexer_->SearchInPath(file_system_path,
                                     query,
                                     Bind(&DevToolsUIBindings::SearchCompleted,
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
  base::DictionaryValue* port_forwarding_dict = nullptr;
  std::unique_ptr<base::Value> parsed_port_forwarding =
      base::JSONReader::Read(port_forwarding_config);
  if (!parsed_port_forwarding ||
      !parsed_port_forwarding->GetAsDictionary(&port_forwarding_dict)) {
    return;
  }

  base::ListValue* network_list = nullptr;
  std::unique_ptr<base::Value> parsed_network =
      base::JSONReader::Read(network_discovery_config);
  if (!parsed_network || !parsed_network->GetAsList(&network_list))
    return;

  profile_->GetPrefs()->SetBoolean(
      prefs::kDevToolsDiscoverUsbDevicesEnabled, discover_usb_devices);
  profile_->GetPrefs()->SetBoolean(
      prefs::kDevToolsPortForwardingEnabled, port_forwarding_enabled);
  profile_->GetPrefs()->Set(prefs::kDevToolsPortForwardingConfig,
                            *port_forwarding_dict);
  profile_->GetPrefs()->SetBoolean(prefs::kDevToolsDiscoverTCPTargetsEnabled,
                                   network_discovery_enabled);
  profile_->GetPrefs()->Set(prefs::kDevToolsTCPDiscoveryConfig, *network_list);
}

void DevToolsUIBindings::DevicesDiscoveryConfigUpdated() {
  base::DictionaryValue config;
  config.Set(kConfigDiscoverUsbDevices,
             profile_->GetPrefs()
                 ->FindPreference(prefs::kDevToolsDiscoverUsbDevicesEnabled)
                 ->GetValue()
                 ->CreateDeepCopy());
  config.Set(kConfigPortForwardingEnabled,
             profile_->GetPrefs()
                 ->FindPreference(prefs::kDevToolsPortForwardingEnabled)
                 ->GetValue()
                 ->CreateDeepCopy());
  config.Set(kConfigPortForwardingConfig,
             profile_->GetPrefs()
                 ->FindPreference(prefs::kDevToolsPortForwardingConfig)
                 ->GetValue()
                 ->CreateDeepCopy());
  config.Set(kConfigNetworkDiscoveryEnabled,
             profile_->GetPrefs()
                 ->FindPreference(prefs::kDevToolsDiscoverTCPTargetsEnabled)
                 ->GetValue()
                 ->CreateDeepCopy());
  config.Set(kConfigNetworkDiscoveryConfig,
             profile_->GetPrefs()
                 ->FindPreference(prefs::kDevToolsTCPDiscoveryConfig)
                 ->GetValue()
                 ->CreateDeepCopy());
  CallClientFunction("DevToolsAPI.devicesDiscoveryConfigChanged", &config,
                     nullptr, nullptr);
}

void DevToolsUIBindings::SendPortForwardingStatus(const base::Value& status) {
  CallClientFunction("DevToolsAPI.devicesPortForwardingStatusChanged", &status,
                     nullptr, nullptr);
}

void DevToolsUIBindings::SetDevicesUpdatesEnabled(bool enabled) {
  if (devices_updates_enabled_ == enabled)
    return;
  devices_updates_enabled_ = enabled;
  if (enabled) {
    remote_targets_handler_ = DevToolsTargetsUIHandler::CreateForAdb(
        base::Bind(&DevToolsUIBindings::DevicesUpdated,
                   base::Unretained(this)),
        profile_);
    pref_change_registrar_.Init(profile_->GetPrefs());
    pref_change_registrar_.Add(prefs::kDevToolsDiscoverUsbDevicesEnabled,
        base::Bind(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kDevToolsPortForwardingEnabled,
        base::Bind(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kDevToolsPortForwardingConfig,
        base::Bind(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                   base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kDevToolsDiscoverTCPTargetsEnabled,
        base::Bind(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                   base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kDevToolsTCPDiscoveryConfig,
        base::Bind(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                   base::Unretained(this)));
    port_status_serializer_.reset(new PortForwardingStatusSerializer(
        base::Bind(&DevToolsUIBindings::SendPortForwardingStatus,
                   base::Unretained(this)),
        profile_));
    DevicesDiscoveryConfigUpdated();
  } else {
    remote_targets_handler_.reset();
    port_status_serializer_.reset();
    pref_change_registrar_.RemoveAll();
    SendPortForwardingStatus(base::DictionaryValue());
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

void DevToolsUIBindings::GetPreferences(const DispatchCallback& callback) {
  const DictionaryValue* prefs =
      profile_->GetPrefs()->GetDictionary(prefs::kDevToolsPreferences);
  callback.Run(prefs);
}

void DevToolsUIBindings::SetPreference(const std::string& name,
                                   const std::string& value) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsPreferences);
  update.Get()->SetKey(name, base::Value(value));
}

void DevToolsUIBindings::RemovePreference(const std::string& name) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsPreferences);
  update.Get()->RemoveWithoutPathExpansion(name, nullptr);
}

void DevToolsUIBindings::ClearPreferences() {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsPreferences);
  update.Get()->Clear();
}

void DevToolsUIBindings::Reattach(const DispatchCallback& callback) {
  if (agent_host_.get()) {
    agent_host_->DetachClient(this);
    InnerAttach();
  }
  callback.Run(nullptr);
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
  if (agent_host_.get() && !reloading_)
    agent_host_->DispatchProtocolMessage(this, message);
}

void DevToolsUIBindings::RecordEnumeratedHistogram(const std::string& name,
                                                   int sample,
                                                   int boundary_value) {
  if (!frontend_host_)
    return;
  if (!(boundary_value >= 0 && boundary_value <= 100 && sample >= 0 &&
        sample < boundary_value)) {
    // TODO(nick): Replace with chrome::bad_message::ReceivedBadMessage().
    frontend_host_->BadMessageRecieved();
    return;
  }
  // Each histogram name must follow a different code path in
  // order to UMA_HISTOGRAM_EXACT_LINEAR work correctly.
  if (name == kDevToolsActionTakenHistogram)
    UMA_HISTOGRAM_EXACT_LINEAR(name, sample, boundary_value);
  else if (name == kDevToolsPanelShownHistogram)
    UMA_HISTOGRAM_EXACT_LINEAR(name, sample, boundary_value);
  else
    frontend_host_->BadMessageRecieved();
}

void DevToolsUIBindings::SendJsonRequest(const DispatchCallback& callback,
                                         const std::string& browser_id,
                                         const std::string& url) {
  if (!android_bridge_) {
    callback.Run(nullptr);
    return;
  }
  android_bridge_->SendJsonRequest(browser_id, url,
      base::Bind(&DevToolsUIBindings::JsonReceived,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void DevToolsUIBindings::JsonReceived(const DispatchCallback& callback,
                                      int result,
                                      const std::string& message) {
  if (result != net::OK) {
    callback.Run(nullptr);
    return;
  }
  base::Value message_value(message);
  callback.Run(&message_value);
}

void DevToolsUIBindings::DeviceCountChanged(int count) {
  base::Value value(count);
  CallClientFunction("DevToolsAPI.deviceCountUpdated", &value, NULL,
                     NULL);
}

void DevToolsUIBindings::DevicesUpdated(
    const std::string& source,
    const base::ListValue& targets) {
  CallClientFunction("DevToolsAPI.devicesUpdated", &targets, NULL,
                     NULL);
}

void DevToolsUIBindings::FileSavedAs(const std::string& url,
                                     const std::string& file_system_path) {
  base::Value url_value(url);
  base::Value file_system_path_value(file_system_path);
  CallClientFunction("DevToolsAPI.savedURL", &url_value,
                     &file_system_path_value, NULL);
}

void DevToolsUIBindings::CanceledFileSaveAs(const std::string& url) {
  base::Value url_value(url);
  CallClientFunction("DevToolsAPI.canceledSaveURL",
                     &url_value, NULL, NULL);
}

void DevToolsUIBindings::AppendedTo(const std::string& url) {
  base::Value url_value(url);
  CallClientFunction("DevToolsAPI.appendedToURL", &url_value, NULL,
                     NULL);
}

void DevToolsUIBindings::FileSystemAdded(
    const std::string& error,
    const DevToolsFileHelper::FileSystem* file_system) {
  base::Value error_value(error);
  std::unique_ptr<base::DictionaryValue> file_system_value(
      file_system ? CreateFileSystemValue(*file_system) : nullptr);
  CallClientFunction("DevToolsAPI.fileSystemAdded", &error_value,
                     file_system_value.get(), NULL);
}

void DevToolsUIBindings::FileSystemRemoved(
    const std::string& file_system_path) {
  base::Value file_system_path_value(file_system_path);
  CallClientFunction("DevToolsAPI.fileSystemRemoved",
                     &file_system_path_value, NULL, NULL);
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
    base::ListValue changed, added, removed;
    while (budget > 0 && changed_index < changed_paths.size()) {
      changed.AppendString(changed_paths[changed_index++]);
      --budget;
    }
    while (budget > 0 && added_index < added_paths.size()) {
      added.AppendString(added_paths[added_index++]);
      --budget;
    }
    while (budget > 0 && removed_index < removed_paths.size()) {
      removed.AppendString(removed_paths[removed_index++]);
      --budget;
    }
    CallClientFunction("DevToolsAPI.fileSystemFilesChangedAddedRemoved",
                       &changed, &added, &removed);
  }
}

void DevToolsUIBindings::IndexingTotalWorkCalculated(
    int request_id,
    const std::string& file_system_path,
    int total_work) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Value request_id_value(request_id);
  base::Value file_system_path_value(file_system_path);
  base::Value total_work_value(total_work);
  CallClientFunction("DevToolsAPI.indexingTotalWorkCalculated",
                     &request_id_value, &file_system_path_value,
                     &total_work_value);
}

void DevToolsUIBindings::IndexingWorked(int request_id,
                                        const std::string& file_system_path,
                                        int worked) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Value request_id_value(request_id);
  base::Value file_system_path_value(file_system_path);
  base::Value worked_value(worked);
  CallClientFunction("DevToolsAPI.indexingWorked", &request_id_value,
                     &file_system_path_value, &worked_value);
}

void DevToolsUIBindings::IndexingDone(int request_id,
                                      const std::string& file_system_path) {
  indexing_jobs_.erase(request_id);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Value request_id_value(request_id);
  base::Value file_system_path_value(file_system_path);
  CallClientFunction("DevToolsAPI.indexingDone", &request_id_value,
                     &file_system_path_value, NULL);
}

void DevToolsUIBindings::SearchCompleted(
    int request_id,
    const std::string& file_system_path,
    const std::vector<std::string>& file_paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ListValue file_paths_value;
  for (auto it(file_paths.begin()); it != file_paths.end(); ++it) {
    file_paths_value.AppendString(*it);
  }
  base::Value request_id_value(request_id);
  base::Value file_system_path_value(file_system_path);
  CallClientFunction("DevToolsAPI.searchCompleted", &request_id_value,
                     &file_system_path_value, &file_paths_value);
}

void DevToolsUIBindings::ShowDevToolsInfoBar(
    const base::string16& message,
    const DevToolsInfoBarDelegate::Callback& callback) {
  if (!delegate_->GetInfoBarService()) {
    callback.Run(false);
    return;
  }
  DevToolsInfoBarDelegate::Create(message, callback);
}

void DevToolsUIBindings::AddDevToolsExtensionsToClient() {
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_->GetOriginalProfile());
  if (!registry)
    return;

  base::ListValue results;
  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    if (extensions::chrome_manifest_urls::GetDevToolsPage(extension.get())
            .is_empty())
      continue;

    // Each devtools extension will need to be able to run in the devtools
    // process. Grant the devtools process the ability to request URLs from the
    // extension.
    content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestOrigin(
        web_contents_->GetMainFrame()->GetProcess()->GetID(),
        url::Origin::Create(extension->url()));

    std::unique_ptr<base::DictionaryValue> extension_info(
        new base::DictionaryValue());
    extension_info->SetString(
        "startPage",
        extensions::chrome_manifest_urls::GetDevToolsPage(extension.get())
            .spec());
    extension_info->SetString("name", extension->name());
    extension_info->SetBoolean("exposeExperimentalAPIs",
                               extension->permissions_data()->HasAPIPermission(
                                   extensions::APIPermission::kExperimental));
    results.Append(std::move(extension_info));
  }

  CallClientFunction("DevToolsAPI.addExtensions",
                     &results, NULL, NULL);
}

void DevToolsUIBindings::RegisterExtensionsAPI(const std::string& origin,
                                               const std::string& script) {
  extensions_api_[origin + "/"] = script;
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

void DevToolsUIBindings::Reload() {
  reloading_ = true;
  web_contents_->GetController().Reload(content::ReloadType::NORMAL, false);
}

void DevToolsUIBindings::Detach() {
  if (agent_host_.get())
    agent_host_->DetachClient(this);
  agent_host_ = NULL;
}

bool DevToolsUIBindings::IsAttachedTo(content::DevToolsAgentHost* agent_host) {
  return agent_host_.get() == agent_host;
}

void DevToolsUIBindings::CallClientFunction(const std::string& function_name,
                                            const base::Value* arg1,
                                            const base::Value* arg2,
                                            const base::Value* arg3) {
  // If we're not exposing bindings, we shouldn't call functions either.
  if (!frontend_host_)
    return;
  std::string javascript = function_name + "(";
  if (arg1) {
    std::string json;
    base::JSONWriter::Write(*arg1, &json);
    javascript.append(json);
    if (arg2) {
      base::JSONWriter::Write(*arg2, &json);
      javascript.append(", ").append(json);
      if (arg3) {
        base::JSONWriter::Write(*arg3, &json);
        javascript.append(", ").append(json);
      }
    }
  }
  javascript.append(");");
  web_contents_->GetMainFrame()->ExecuteJavaScript(
      base::UTF8ToUTF16(javascript));
}

void DevToolsUIBindings::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame()) {
    if (!IsValidFrontendURL(navigation_handle->GetURL())) {
      LOG(ERROR) << "Attempt to navigate to an invalid DevTools front-end URL: "
                 << navigation_handle->GetURL().spec();
      frontend_host_.reset();
      return;
    }
    if (navigation_handle->GetRenderFrameHost() ==
            web_contents_->GetMainFrame() &&
        frontend_host_) {
      return;
    }
    if (content::RenderFrameHost* opener = web_contents_->GetOpener()) {
      content::WebContents* opener_wc =
          content::WebContents::FromRenderFrameHost(opener);
      DevToolsUIBindings* opener_bindings =
          opener_wc ? DevToolsUIBindings::ForWebContents(opener_wc) : nullptr;
      if (!opener_bindings || !opener_bindings->frontend_host_)
        return;
    }
    frontend_host_.reset(content::DevToolsFrontendHost::Create(
        navigation_handle->GetRenderFrameHost(),
        base::Bind(&DevToolsUIBindings::HandleMessageFromDevToolsFrontend,
                   base::Unretained(this))));
    return;
  }

  content::RenderFrameHost* frame = navigation_handle->GetRenderFrameHost();
  std::string origin = navigation_handle->GetURL().GetOrigin().spec();
  auto it = extensions_api_.find(origin);
  if (it == extensions_api_.end())
    return;
  std::string script = base::StringPrintf("%s(\"%s\")", it->second.c_str(),
                                          base::GenerateGUID().c_str());
  content::DevToolsFrontendHost::SetupExtensionsAPI(frame, script);
}

void DevToolsUIBindings::DocumentAvailableInMainFrame() {
  if (!reloading_)
    return;
  reloading_ = false;
  if (agent_host_.get()) {
    agent_host_->DetachClient(this);
    InnerAttach();
  }
}

void DevToolsUIBindings::DocumentOnLoadCompletedInMainFrame() {
  // In the DEBUG_DEVTOOLS mode, the DocumentOnLoadCompletedInMainFrame event
  // arrives before the LoadCompleted event, thus it should not trigger the
  // frontend load handling.
#if !BUILDFLAG(DEBUG_DEVTOOLS)
  FrontendLoaded();
#endif
}

void DevToolsUIBindings::DidNavigateMainFrame() {
  frontend_loaded_ = false;
}

void DevToolsUIBindings::FrontendLoaded() {
  if (frontend_loaded_)
    return;
  frontend_loaded_ = true;

  // Call delegate first - it seeds importants bit of information.
  delegate_->OnLoadCompleted();

  AddDevToolsExtensionsToClient();
}
