// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Debugger API.

#include "chrome/browser/extensions/api/debugger/debugger_api.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/extensions/api/debugger/debugger_api_constants.h"
#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"

using content::DevToolsAgentHost;
using content::RenderProcessHost;
using content::RenderWidgetHost;
using content::WebContents;

namespace Attach = extensions::api::debugger::Attach;
namespace Detach = extensions::api::debugger::Detach;
namespace OnDetach = extensions::api::debugger::OnDetach;
namespace OnEvent = extensions::api::debugger::OnEvent;
namespace SendCommand = extensions::api::debugger::SendCommand;

namespace extensions {
class ExtensionRegistry;
class ExtensionDevToolsClientHost;

namespace {

// Helpers --------------------------------------------------------------------

void CopyDebuggee(Debuggee* dst, const Debuggee& src) {
  if (src.tab_id)
    dst->tab_id.reset(new int(*src.tab_id));
  if (src.extension_id)
    dst->extension_id.reset(new std::string(*src.extension_id));
  if (src.target_id)
    dst->target_id.reset(new std::string(*src.target_id));
}

// Returns true if the given |Extension| is allowed to attach to the specified
// |url|.
bool ExtensionCanAttachToURL(const Extension& extension,
                             const GURL& url,
                             Profile* profile,
                             std::string* error) {
  if (url == content::kUnreachableWebDataURL)
    return true;

  // NOTE: The `debugger` permission implies all URLs access (and indicates
  // such to the user), so we don't check explicit page access. However, we
  // still need to check if it's an otherwise-restricted URL.
  if (extension.permissions_data()->IsRestrictedUrl(url, error))
    return false;

  if (url.SchemeIsFile() && !util::AllowFileAccess(extension.id(), profile)) {
    *error = debugger_api_constants::kRestrictedError;
    return false;
  }

  return true;
}

}  // namespace

// ExtensionDevToolsClientHost ------------------------------------------------

using AttachedClientHosts = std::set<ExtensionDevToolsClientHost*>;
base::LazyInstance<AttachedClientHosts>::Leaky g_attached_client_hosts =
    LAZY_INSTANCE_INITIALIZER;

class ExtensionDevToolsClientHost : public content::DevToolsAgentHostClient,
                                    public content::NotificationObserver,
                                    public ExtensionRegistryObserver {
 public:
  ExtensionDevToolsClientHost(Profile* profile,
                              DevToolsAgentHost* agent_host,
                              scoped_refptr<const Extension> extension,
                              const Debuggee& debuggee);

  ~ExtensionDevToolsClientHost() override;

  bool Attach();
  const std::string& extension_id() { return extension_->id(); }
  DevToolsAgentHost* agent_host() { return agent_host_.get(); }
  void RespondDetachedToPendingRequests();
  void Close();
  void SendMessageToBackend(DebuggerSendCommandFunction* function,
                            const std::string& method,
                            SendCommand::Params::CommandParams* command_params);

  // Closes connection as terminated by the user.
  void InfoBarDismissed();

  // DevToolsAgentHostClient interface.
  void AgentHostClosed(DevToolsAgentHost* agent_host) override;
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override;
  bool MayAttachToRenderer(content::RenderFrameHost* render_frame_host,
                           bool is_webui) override;
  bool MayAttachToBrowser() override;
  bool MayDiscoverTargets() override;
  bool MayAffectLocalFiles() override;

 private:
  using PendingRequests =
      std::map<int, scoped_refptr<DebuggerSendCommandFunction>>;

  void SendDetachedEvent();

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  Profile* profile_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  scoped_refptr<const Extension> extension_;
  Debuggee debuggee_;
  content::NotificationRegistrar registrar_;
  int last_request_id_;
  PendingRequests pending_requests_;
  ExtensionDevToolsInfoBar* infobar_;
  api::debugger::DetachReason detach_reason_;

  // Listen to extension unloaded notification.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsClientHost);
};

ExtensionDevToolsClientHost::ExtensionDevToolsClientHost(
    Profile* profile,
    DevToolsAgentHost* agent_host,
    scoped_refptr<const Extension> extension,
    const Debuggee& debuggee)
    : profile_(profile),
      agent_host_(agent_host),
      extension_(std::move(extension)),
      last_request_id_(0),
      infobar_(nullptr),
      detach_reason_(api::debugger::DETACH_REASON_TARGET_CLOSED),
      extension_registry_observer_(this) {
  CopyDebuggee(&debuggee_, debuggee);

  g_attached_client_hosts.Get().insert(this);

  // ExtensionRegistryObserver listen extension unloaded and detach debugger
  // from there.
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));

  // RVH-based agents disconnect from their clients when the app is terminating
  // but shared worker-based agents do not.
  // Disconnect explicitly to make sure that |this| observer is not leaked.
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

bool ExtensionDevToolsClientHost::Attach() {
  // Attach to debugger and tell it we are ready.
  if (!agent_host_->AttachClient(this))
    return false;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kSilentDebuggerExtensionAPI)) {
    return true;
  }

  // We allow policy-installed extensions to circumvent the normal
  // infobar warning. See crbug.com/693621.
  if (Manifest::IsPolicyLocation(extension_->location()))
    return true;

  infobar_ = ExtensionDevToolsInfoBar::Create(
      extension_id(), extension_->name(), this,
      base::Bind(&ExtensionDevToolsClientHost::InfoBarDismissed,
                 base::Unretained(this)));
  return true;
}

ExtensionDevToolsClientHost::~ExtensionDevToolsClientHost() {
  if (infobar_)
    infobar_->Remove(this);
  g_attached_client_hosts.Get().erase(this);
}

// DevToolsAgentHostClient implementation.
void ExtensionDevToolsClientHost::AgentHostClosed(
    DevToolsAgentHost* agent_host) {
  DCHECK(agent_host == agent_host_.get());
  RespondDetachedToPendingRequests();
  SendDetachedEvent();
  delete this;
}

void ExtensionDevToolsClientHost::Close() {
  agent_host_->DetachClient(this);
  delete this;
}

void ExtensionDevToolsClientHost::SendMessageToBackend(
    DebuggerSendCommandFunction* function,
    const std::string& method,
    SendCommand::Params::CommandParams* command_params) {
  base::DictionaryValue protocol_request;
  int request_id = ++last_request_id_;
  pending_requests_[request_id] = function;
  protocol_request.SetInteger("id", request_id);
  protocol_request.SetString("method", method);
  if (command_params) {
    protocol_request.Set(
        "params", command_params->additional_properties.CreateDeepCopy());
  }

  std::string json_args;
  base::JSONWriter::Write(protocol_request, &json_args);
  agent_host_->DispatchProtocolMessage(this, json_args);
}

void ExtensionDevToolsClientHost::InfoBarDismissed() {
  detach_reason_ = api::debugger::DETACH_REASON_CANCELED_BY_USER;
  RespondDetachedToPendingRequests();
  SendDetachedEvent();
  Close();
}

void ExtensionDevToolsClientHost::RespondDetachedToPendingRequests() {
  for (const auto& it : pending_requests_)
    it.second->SendDetachedError();
  pending_requests_.clear();
}

void ExtensionDevToolsClientHost::SendDetachedEvent() {
  if (!EventRouter::Get(profile_))
    return;

  std::unique_ptr<base::ListValue> args(
      OnDetach::Create(debuggee_, detach_reason_));
  auto event =
      std::make_unique<Event>(events::DEBUGGER_ON_DETACH, OnDetach::kEventName,
                              std::move(args), profile_);
  EventRouter::Get(profile_)->DispatchEventToExtension(extension_id(),
                                                       std::move(event));
}

void ExtensionDevToolsClientHost::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  if (extension->id() == extension_id())
    Close();
}

void ExtensionDevToolsClientHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  Close();
}

void ExtensionDevToolsClientHost::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host, const std::string& message) {
  DCHECK(agent_host == agent_host_.get());
  if (!EventRouter::Get(profile_))
    return;

  std::unique_ptr<base::Value> result = base::JSONReader::Read(message);
  if (!result || !result->is_dict())
    return;
  base::DictionaryValue* dictionary =
      static_cast<base::DictionaryValue*>(result.get());

  int id;
  if (!dictionary->GetInteger("id", &id)) {
    std::string method_name;
    if (!dictionary->GetString("method", &method_name))
      return;

    OnEvent::Params params;
    base::DictionaryValue* params_value;
    if (dictionary->GetDictionary("params", &params_value))
      params.additional_properties.Swap(params_value);

    std::unique_ptr<base::ListValue> args(
        OnEvent::Create(debuggee_, method_name, params));
    auto event =
        std::make_unique<Event>(events::DEBUGGER_ON_EVENT, OnEvent::kEventName,
                                std::move(args), profile_);
    EventRouter::Get(profile_)->DispatchEventToExtension(extension_id(),
                                                         std::move(event));
  } else {
    DebuggerSendCommandFunction* function = pending_requests_[id].get();
    if (!function)
      return;

    function->SendResponseBody(dictionary);
    pending_requests_.erase(id);
  }
}

bool ExtensionDevToolsClientHost::MayAttachToRenderer(
    content::RenderFrameHost* render_frame_host,
    bool is_webui) {
  if (is_webui)
    return false;

  if (!render_frame_host)
    return true;

  std::string error;
  // We check the site instance URL here (instead of
  // RenderFrameHost::GetLastCommittedURL()) because it's too early in the
  // navigation for anything else.
  const GURL& site_instance_url =
      render_frame_host->GetSiteInstance()->GetSiteURL();

  if (site_instance_url.is_empty() || site_instance_url == "about:") {
    // Allow the extension to attach to about:blank.
    return true;
  }

  return ExtensionCanAttachToURL(*extension_, site_instance_url, profile_,
                                 &error);
}

bool ExtensionDevToolsClientHost::MayAttachToBrowser() {
  return false;
}

bool ExtensionDevToolsClientHost::MayDiscoverTargets() {
  return false;
}

bool ExtensionDevToolsClientHost::MayAffectLocalFiles() {
  return false;
}

// DebuggerFunction -----------------------------------------------------------

DebuggerFunction::DebuggerFunction()
    : client_host_(NULL) {
}

DebuggerFunction::~DebuggerFunction() {
}

void DebuggerFunction::FormatErrorMessage(const std::string& format) {
  if (debuggee_.tab_id)
    error_ = ErrorUtils::FormatErrorMessage(
        format, debugger_api_constants::kTabTargetType,
        base::IntToString(*debuggee_.tab_id));
  else if (debuggee_.extension_id)
    error_ = ErrorUtils::FormatErrorMessage(
        format, debugger_api_constants::kBackgroundPageTargetType,
        *debuggee_.extension_id);
  else
    error_ = ErrorUtils::FormatErrorMessage(
        format, debugger_api_constants::kOpaqueTargetType,
        *debuggee_.target_id);
}

bool DebuggerFunction::InitAgentHost() {
  if (debuggee_.tab_id) {
    WebContents* web_contents = NULL;
    bool result = ExtensionTabUtil::GetTabById(*debuggee_.tab_id, GetProfile(),
                                               include_incognito_information(),
                                               NULL, NULL, &web_contents, NULL);
    if (result && web_contents) {
      // TODO(rdevlin.cronin) This should definitely be GetLastCommittedURL().
      GURL url = web_contents->GetVisibleURL();

      if (!ExtensionCanAttachToURL(*extension(), url, GetProfile(), &error_))
        return false;

      agent_host_ = DevToolsAgentHost::GetOrCreateFor(web_contents);
    }
  } else if (debuggee_.extension_id) {
    ExtensionHost* extension_host =
        ProcessManager::Get(GetProfile())
            ->GetBackgroundHostForExtension(*debuggee_.extension_id);
    if (extension_host) {
      if (extension()->permissions_data()->IsRestrictedUrl(
              extension_host->GetURL(), &error_)) {
        return false;
      }
      agent_host_ =
          DevToolsAgentHost::GetOrCreateFor(extension_host->host_contents());
    }
  } else if (debuggee_.target_id) {
    agent_host_ = DevToolsAgentHost::GetForId(*debuggee_.target_id);
    if (agent_host_.get()) {
      if (extension()->permissions_data()->IsRestrictedUrl(
              agent_host_->GetURL(), &error_)) {
        agent_host_ = nullptr;
        return false;
      }
    }
  } else {
    error_ = debugger_api_constants::kInvalidTargetError;
    return false;
  }

  if (!agent_host_.get()) {
    FormatErrorMessage(debugger_api_constants::kNoTargetError);
    return false;
  }
  return true;
}

bool DebuggerFunction::InitClientHost() {
  if (!InitAgentHost())
    return false;

  client_host_ = FindClientHost();
  if (!client_host_) {
    FormatErrorMessage(debugger_api_constants::kNotAttachedError);
    return false;
  }

  return true;
}

ExtensionDevToolsClientHost* DebuggerFunction::FindClientHost() {
  if (!agent_host_.get())
    return nullptr;

  const std::string& extension_id = extension()->id();
  DevToolsAgentHost* agent_host = agent_host_.get();
  AttachedClientHosts& hosts = g_attached_client_hosts.Get();
  auto it = std::find_if(
      hosts.begin(), hosts.end(),
      [&agent_host, &extension_id](ExtensionDevToolsClientHost* client_host) {
        return client_host->agent_host() == agent_host &&
               client_host->extension_id() == extension_id;
      });

  return it == hosts.end() ? nullptr : *it;
}

// DebuggerAttachFunction -----------------------------------------------------

DebuggerAttachFunction::DebuggerAttachFunction() {
}

DebuggerAttachFunction::~DebuggerAttachFunction() {
}

bool DebuggerAttachFunction::RunAsync() {
  std::unique_ptr<Attach::Params> params(Attach::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopyDebuggee(&debuggee_, params->target);
  if (!InitAgentHost())
    return false;

  if (!DevToolsAgentHost::IsSupportedProtocolVersion(
          params->required_version)) {
    error_ = ErrorUtils::FormatErrorMessage(
        debugger_api_constants::kProtocolVersionNotSupportedError,
        params->required_version);
    return false;
  }

  if (FindClientHost()) {
    FormatErrorMessage(debugger_api_constants::kAlreadyAttachedError);
    return false;
  }

  auto host = std::make_unique<ExtensionDevToolsClientHost>(
      GetProfile(), agent_host_.get(), extension(), debuggee_);

  if (!host->Attach()) {
    FormatErrorMessage(debugger_api_constants::kRestrictedError);
    return false;
  }

  host.release();  // An attached client host manages its own lifetime.
  SendResponse(true);
  return true;
}


// DebuggerDetachFunction -----------------------------------------------------

DebuggerDetachFunction::DebuggerDetachFunction() {
}

DebuggerDetachFunction::~DebuggerDetachFunction() {
}

bool DebuggerDetachFunction::RunAsync() {
  std::unique_ptr<Detach::Params> params(Detach::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopyDebuggee(&debuggee_, params->target);
  if (!InitClientHost())
    return false;

  client_host_->RespondDetachedToPendingRequests();
  client_host_->Close();
  SendResponse(true);
  return true;
}


// DebuggerSendCommandFunction ------------------------------------------------

DebuggerSendCommandFunction::DebuggerSendCommandFunction() {
}

DebuggerSendCommandFunction::~DebuggerSendCommandFunction() {
}

bool DebuggerSendCommandFunction::RunAsync() {
  std::unique_ptr<SendCommand::Params> params(
      SendCommand::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopyDebuggee(&debuggee_, params->target);
  if (!InitClientHost())
    return false;

  client_host_->SendMessageToBackend(this, params->method,
      params->command_params.get());
  return true;
}

void DebuggerSendCommandFunction::SendResponseBody(
    base::DictionaryValue* response) {
  base::Value* error_body;
  if (response->Get("error", &error_body)) {
    base::JSONWriter::Write(*error_body, &error_);
    SendResponse(false);
    return;
  }

  base::DictionaryValue* result_body;
  SendCommand::Results::Result result;
  if (response->GetDictionary("result", &result_body))
    result.additional_properties.Swap(result_body);

  results_ = SendCommand::Results::Create(result);
  SendResponse(true);
}

void DebuggerSendCommandFunction::SendDetachedError() {
  error_ = debugger_api_constants::kDetachedWhileHandlingError;
  SendResponse(false);
}

// DebuggerGetTargetsFunction -------------------------------------------------

namespace {

const char kTargetIdField[] = "id";
const char kTargetTypeField[] = "type";
const char kTargetTitleField[] = "title";
const char kTargetAttachedField[] = "attached";
const char kTargetUrlField[] = "url";
const char kTargetFaviconUrlField[] = "faviconUrl";
const char kTargetTabIdField[] = "tabId";
const char kTargetExtensionIdField[] = "extensionId";
const char kTargetTypePage[] = "page";
const char kTargetTypeBackgroundPage[] = "background_page";
const char kTargetTypeWorker[] = "worker";
const char kTargetTypeOther[] = "other";

std::unique_ptr<base::DictionaryValue> SerializeTarget(
    scoped_refptr<DevToolsAgentHost> host) {
  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());
  dictionary->SetString(kTargetIdField, host->GetId());
  dictionary->SetString(kTargetTitleField, host->GetTitle());
  dictionary->SetBoolean(kTargetAttachedField, host->IsAttached());
  dictionary->SetString(kTargetUrlField, host->GetURL().spec());

  std::string type = host->GetType();
  std::string target_type = kTargetTypeOther;
  if (type == DevToolsAgentHost::kTypePage) {
    int tab_id =
        extensions::ExtensionTabUtil::GetTabId(host->GetWebContents());
    dictionary->SetInteger(kTargetTabIdField, tab_id);
    target_type = kTargetTypePage;
  } else if (type == ChromeDevToolsManagerDelegate::kTypeBackgroundPage) {
    dictionary->SetString(kTargetExtensionIdField, host->GetURL().host());
    target_type = kTargetTypeBackgroundPage;
  } else if (type == DevToolsAgentHost::kTypeServiceWorker ||
             type == DevToolsAgentHost::kTypeSharedWorker) {
    target_type = kTargetTypeWorker;
  }

  dictionary->SetString(kTargetTypeField, target_type);

  GURL favicon_url = host->GetFaviconURL();
  if (favicon_url.is_valid())
    dictionary->SetString(kTargetFaviconUrlField, favicon_url.spec());

  return dictionary;
}

}  // namespace

DebuggerGetTargetsFunction::DebuggerGetTargetsFunction() {
}

DebuggerGetTargetsFunction::~DebuggerGetTargetsFunction() {
}

bool DebuggerGetTargetsFunction::RunAsync() {
  content::DevToolsAgentHost::List list = DevToolsAgentHost::GetOrCreateAll();
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&DebuggerGetTargetsFunction::SendTargetList, this, list));
  return true;
}

void DebuggerGetTargetsFunction::SendTargetList(
    const content::DevToolsAgentHost::List& target_list) {
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  for (size_t i = 0; i < target_list.size(); ++i)
    result->Append(SerializeTarget(target_list[i]));
  SetResult(std::move(result));
  SendResponse(true);
}

}  // namespace extensions
