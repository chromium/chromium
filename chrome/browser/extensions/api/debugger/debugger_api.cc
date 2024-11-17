// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Debugger API.

#include "chrome/browser/extensions/api/debugger/debugger_api.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_entry.h"
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
#include "extensions/browser/extension_util.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "pdf/buildflags.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/common/pdf_util.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

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

constexpr char kAlreadyAttachedError[] =
    "Another debugger is already attached to the * with id: *.";
constexpr char kNoTargetError[] = "No * with given id *.";
constexpr char kInvalidTargetError[] =
    "Either tab id or extension id must be specified.";
constexpr char kNotAttachedError[] =
    "Debugger is not attached to the * with id: *.";
constexpr char kProtocolVersionNotSupportedError[] =
    "Requested protocol version is not supported: *.";
constexpr char kRestrictedError[] = "Cannot attach to this target.";
constexpr char kDetachedWhileHandlingError[] =
    "Detached while handling command.";

constexpr char kTabTargetType[] = "tab";
constexpr char kBackgroundPageTargetType[] = "background page";
constexpr char kOpaqueTargetType[] = "target";

// Helpers --------------------------------------------------------------------

void CopyDebuggee(Debuggee* dst, const Debuggee& src) {
  dst->tab_id = src.tab_id;
  dst->extension_id = src.extension_id;
  dst->target_id = src.target_id;
}

void DebuggerSessionFromDebugee(DebuggerSession& dst,
                                const Debuggee& src,
                                std::string* maybe_session_id) {
  dst.tab_id = src.tab_id;
  dst.extension_id = src.extension_id;
  dst.target_id = src.target_id;
  if (maybe_session_id) {
    dst.session_id = *maybe_session_id;
  }
}

void DebuggeeFromDebuggerSession(Debuggee& dst, const DebuggerSession& src) {
  dst.tab_id = src.tab_id;
  dst.extension_id = src.extension_id;
  dst.target_id = src.target_id;
}

#if BUILDFLAG(ENABLE_PDF)
// Returns whether `url` is the URL for the built-in PDF extension.
bool IsPdfExtensionUrl(const GURL& url) {
  return url.scheme() == kExtensionScheme &&
         url.host() == extension_misc::kPdfExtensionId;
}
#endif  // BUILDFLAG(ENABLE_PDF)

bool ExtensionMayAttachToTargetProfile(Profile* extension_profile,
                                       bool allow_incognito_access,
                                       DevToolsAgentHost& agent_host) {
  Profile* profile =
      Profile::FromBrowserContext(agent_host.GetBrowserContext());
  if (!profile)
    return false;
  if (!extension_profile->IsSameOrParent(profile))
    return false;
  return profile == extension_profile || allow_incognito_access;
}

// Returns true if the given |Extension| is allowed to attach to the specified
// |url|.
bool ExtensionMayAttachToURL(const Extension& extension,
                             Profile* extension_profile,
                             const GURL& url,
                             std::string* error) {
  // Allow the extension to attach to about:blank and empty URLs.
  if (url.is_empty() || url == "about:")
    return true;

  if (url == content::kUnreachableWebDataURL)
    return true;

  // NOTE: The `debugger` permission implies all URLs access (and indicates
  // such to the user), so we don't check explicit page access. However, we
  // still need to check if it's an otherwise-restricted URL.
  // NOTE: blob URLs are generally restricted but debugger should be able to
  // attach if it has access to the origin that created the blob.
  // See https://crbug.com/1492134.
  const GURL& url_for_restriction_check =
      url.SchemeIsBlob() ? url::Origin::Create(url).GetURL() : url;
  if (extension.permissions_data()->IsRestrictedUrl(url_for_restriction_check,
                                                    error)) {
    return false;
  }

  // Policy blocked hosts supersede the `debugger` permission.
  if (extension.permissions_data()->IsPolicyBlockedHost(url) ||
      extension.permissions_data()->IsPolicyBlockedHost(
          url_for_restriction_check)) {
    *error = kRestrictedError;
    return false;
  }

  if (url.SchemeIsFile() &&
      !util::AllowFileAccess(extension.id(), extension_profile)) {
    *error = kRestrictedError;
    return false;
  }

  return true;
}

// Returns whether the extension may attach to a frame. `frame_url` is the URL
// of the frame, If querying about a frame, `page_url` is non-null, and
// identifies the URL of the outermost frame.
bool ExtensionMayAttachToURLOrInnerURL(const Extension& extension,
                                       Profile* extension_profile,
                                       const GURL& frame_url,
                                       const GURL* page_url,
                                       std::string* error) {
  // cid: URLs within file: urls are just parts of the top level page, and don't
  // represent new origins. We skip over these and instead test against the
  // top-level page URL.
  if (page_url && page_url->SchemeIsFile() &&
      frame_url.SchemeIs(url::kContentIDScheme)) {
    return ExtensionMayAttachToURLOrInnerURL(extension, extension_profile,
                                             *page_url, nullptr, error);
  }

  if (!ExtensionMayAttachToURL(extension, extension_profile, frame_url,
                               error)) {
    return false;
  }
  // For nested URLs, make sure ExtensionMayAttachToURL() allows both
  // the outer and the inner URLs.
  if (frame_url.inner_url() &&
      !ExtensionMayAttachToURL(extension, extension_profile,
                               *frame_url.inner_url(), error)) {
    return false;
  }
  return true;
}

constexpr char kBrowserTargetId[] = "browser";

constexpr char kPerfettoUIExtensionId[] = "lfmkphfpdbjijhpomgecfikhfohaoine";

bool ExtensionIsTrusted(const Extension& extension) {
  return extension.id() == kPerfettoUIExtensionId;
}

bool ExtensionMayAttachToRenderFrameHost(
    const Extension& extension,
    Profile* extension_profile,
    content::RenderFrameHost* render_frame_host,
    std::string* error) {
  bool result = true;
  const GURL& page_url = render_frame_host->GetLastCommittedURL();
  render_frame_host->ForEachRenderFrameHostWithAction(
      [&page_url, &extension, extension_profile, error,
       &result](content::RenderFrameHost* render_frame_host) {
        // If |render_frame_host| is attached to an inner MimeHandlerViewGuest
        // skip it. This is done to fix crbug.com/1293856 because an extension
        // cannot inspect another extension.
        if (MimeHandlerViewGuest::FromRenderFrameHost(render_frame_host)) {
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        }

#if BUILDFLAG(ENABLE_PDF)
        // The PDF extension frame would normally prevent all other frames in
        // the frame tree from being attachable. Skip it so this doesn't occur.
        // This should be okay, since the PDF extension frame and PDF content
        // frame aren't listed in chrome.debugger.getTargets(). Check both the
        // last committed origin and the SiteURL for the PDF extension frame,
        // because this method may be called in the middle of a navigation where
        // the SiteURL has been updated but navigation hasn't committed yet.
        if (chrome_pdf::features::IsOopifPdfEnabled() &&
            (IsPdfExtensionOrigin(
                 render_frame_host->GetLastCommittedOrigin()) ||
             IsPdfExtensionUrl(
                 render_frame_host->GetSiteInstance()->GetSiteURL()))) {
          return content::RenderFrameHost::FrameIterationAction::kContinue;
        }
#endif  // BUILDFLAG(ENABLE_PDF)

        if (render_frame_host->GetWebUI()) {
          *error = kRestrictedError;
          result = false;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }

        // We check both the last committed URL and the SiteURL because this
        // method may be called in the middle of a navigation where the SiteURL
        // has been updated but navigation hasn't committed yet.
        if (!ExtensionMayAttachToURLOrInnerURL(
                extension, extension_profile,
                render_frame_host->GetLastCommittedURL(), &page_url, error) ||
            !ExtensionMayAttachToURLOrInnerURL(
                extension, extension_profile,
                render_frame_host->GetSiteInstance()->GetSiteURL(), &page_url,
                error)) {
          result = false;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }

        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return result;
}

bool ExtensionMayAttachToWebContents(const Extension& extension,
                                     Profile* extension_profile,
                                     WebContents& web_contents,
                                     std::string* error) {
  security_interstitials::SecurityInterstitialTabHelper*
      security_interstitial_tab_helper = security_interstitials::
          SecurityInterstitialTabHelper::FromWebContents(&web_contents);
  if (security_interstitial_tab_helper &&
      security_interstitial_tab_helper->IsDisplayingInterstitial()) {
    *error = kRestrictedError;
    return false;
  }
  // This is *not* redundant to the checks below, as
  // web_contents.GetLastCommittedURL() may be different from
  // web_contents.GetPrimaryMainFrame()->GetLastCommittedURL(), with the
  // former being a 'virtual' URL as obtained from NavigationEntry.
  if (!ExtensionMayAttachToURL(extension, extension_profile,
                               web_contents.GetLastCommittedURL(), error)) {
    return false;
  }
  if (web_contents.GetController().GetPendingEntry() &&
      !ExtensionMayAttachToURL(
          extension, extension_profile,
          web_contents.GetController().GetPendingEntry()->GetURL(), error)) {
    return false;
  }

  return ExtensionMayAttachToRenderFrameHost(
      extension, extension_profile, web_contents.GetPrimaryMainFrame(), error);
}

bool ExtensionMayAttachToAgentHost(const Extension& extension,
                                   bool allow_incognito_access,
                                   Profile* extension_profile,
                                   DevToolsAgentHost& agent_host,
                                   std::string* error) {
  if (!ExtensionMayAttachToTargetProfile(extension_profile,
                                         allow_incognito_access, agent_host)) {
    *error = kRestrictedError;
    return false;
  }
  if (WebContents* wc = agent_host.GetWebContents()) {
    return ExtensionMayAttachToWebContents(extension, extension_profile, *wc,
                                           error);
  }

  return ExtensionMayAttachToURL(extension, extension_profile,
                                 agent_host.GetURL(), error);
}

}  // namespace

// ExtensionDevToolsClientHost ------------------------------------------------

using AttachedClientHosts = std::set<ExtensionDevToolsClientHost*>;
base::LazyInstance<AttachedClientHosts>::Leaky g_attached_client_hosts =
    LAZY_INSTANCE_INITIALIZER;

class ExtensionDevToolsClientHost : public content::DevToolsAgentHostClient,
                                    public ExtensionRegistryObserver,
                                    public ProfileObserver {
 public:
  ExtensionDevToolsClientHost(
      Profile* profile,
      DevToolsAgentHost* agent_host,
      scoped_refptr<const Extension> extension,
      std::optional<WorkerId> extension_service_worker_id,
      const Debuggee& debuggee);

  ExtensionDevToolsClientHost(const ExtensionDevToolsClientHost&) = delete;
  ExtensionDevToolsClientHost& operator=(const ExtensionDevToolsClientHost&) =
      delete;

  ~ExtensionDevToolsClientHost() override;

  std::string GetTypeForMetrics() override { return "Extension"; }

  bool Attach();
  const ExtensionId& extension_id() { return extension_->id(); }
  DevToolsAgentHost* agent_host() { return agent_host_.get(); }
  void RespondDetachedToPendingRequests();
  void Close();
  void SendMessageToBackend(DebuggerSendCommandFunction* function,
                            const std::string& method,
                            SendCommand::Params::CommandParams* command_params,
                            std::optional<std::string> session_id);

  // Closes connection as terminated by the user.
  void InfoBarDestroyed();

  // DevToolsAgentHostClient interface.
  void AgentHostClosed(DevToolsAgentHost* agent_host) override;
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;
  bool MayAttachToRenderFrameHost(
      content::RenderFrameHost* render_frame_host) override;
  bool MayAttachToURL(const GURL& url, bool is_webui) override;
  bool IsTrusted() override;
  bool MayReadLocalFiles() override;
  bool MayWriteLocalFiles() override;
  std::optional<url::Origin> GetNavigationInitiatorOrigin() override;

 private:
  using PendingRequests =
      std::map<int, scoped_refptr<DebuggerSendCommandFunction>>;

  void SendDetachedEvent();

  void OnAppTerminating();

  // ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  // ProfileObserver implementation
  void OnProfileWillBeDestroyed(Profile* profile) override;

  raw_ptr<Profile> profile_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  scoped_refptr<const Extension> extension_;
  // The WorkerId of the extension service worker that called attach() for this
  // client host, if any.
  const std::optional<WorkerId> extension_service_worker_id_;

  Debuggee debuggee_;
  base::CallbackListSubscription on_app_terminating_subscription_;
  int last_request_id_ = 0;
  PendingRequests pending_requests_;
  base::CallbackListSubscription subscription_;
  api::debugger::DetachReason detach_reason_ =
      api::debugger::DetachReason::kTargetClosed;

  // A service worker keepalive used to keep the associated worker alive while
  // this client is attached. Only used if `extension_service_worker_id_` has a
  // value.
  std::optional<base::Uuid> service_worker_keepalive_;

  // Listen to extension unloaded notification.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

ExtensionDevToolsClientHost::ExtensionDevToolsClientHost(
    Profile* profile,
    DevToolsAgentHost* agent_host,
    scoped_refptr<const Extension> extension,
    std::optional<WorkerId> extension_service_worker_id,
    const Debuggee& debuggee)
    : profile_(profile),
      agent_host_(agent_host),
      extension_(std::move(extension)),
      extension_service_worker_id_(std::move(extension_service_worker_id)) {
  CopyDebuggee(&debuggee_, debuggee);

  g_attached_client_hosts.Get().insert(this);

  // ExtensionRegistryObserver listen extension unloaded and detach debugger
  // from there.
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
  profile_observation_.Observe(profile_);

  // RVH-based agents disconnect from their clients when the app is terminating
  // but shared worker-based agents do not.
  // Disconnect explicitly to make sure that |this| observer is not leaked.
  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(
          base::BindOnce(&ExtensionDevToolsClientHost::OnAppTerminating,
                         base::Unretained(this)));
}

bool ExtensionDevToolsClientHost::Attach() {
  // Attach to debugger and tell it we are ready.
  if (!agent_host_->AttachClient(this))
    return false;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kSilentDebuggerExtensionAPI)) {
    return true;
  }

  if (base::FeatureList::IsEnabled(
          extensions_features::kSilentDebuggerExtensionAPI)) {
    return true;
  }

  // We allow policy-installed extensions to circumvent the normal
  // infobar warning. See crbug.com/693621.
  if (Manifest::IsPolicyLocation(extension_->location()))
    return true;

  subscription_ = ExtensionDevToolsInfoBarDelegate::Create(
      extension_id(), extension_->name(),
      base::BindOnce(&ExtensionDevToolsClientHost::InfoBarDestroyed,
                     base::Unretained(this)));
  if (extension_service_worker_id_) {
    ProcessManager* process_manager = ProcessManager::Get(profile_);
    CHECK(process_manager);
    // The service worker should definitely be registered at this point.
    CHECK(process_manager->HasServiceWorker(*extension_service_worker_id_));
    service_worker_keepalive_ =
        process_manager->IncrementServiceWorkerKeepaliveCount(
            *extension_service_worker_id_,
            content::ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout,
            Activity::DEBUGGER, /*extra_data=*/std::string());
  }

  return true;
}

ExtensionDevToolsClientHost::~ExtensionDevToolsClientHost() {
  g_attached_client_hosts.Get().erase(this);

  // Decrement the associated worker keepalive, if any.
  if (service_worker_keepalive_) {
    CHECK(extension_service_worker_id_);
    ProcessManager* process_manager = ProcessManager::Get(profile_);
    CHECK(process_manager);
    // The worker may have terminated for other reasons. Only decrement the
    // keepalive if it's still around.
    if (process_manager->HasServiceWorker(*extension_service_worker_id_)) {
      process_manager->DecrementServiceWorkerKeepaliveCount(
          *extension_service_worker_id_, *service_worker_keepalive_,
          Activity::DEBUGGER, /*extra_data=*/std::string());
    }
  }
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
    SendCommand::Params::CommandParams* command_params,
    std::optional<std::string> session_id) {
  base::Value::Dict protocol_request;
  int request_id = ++last_request_id_;
  pending_requests_[request_id] = function;
  protocol_request.Set("id", request_id);
  protocol_request.Set("method", method);
  if (command_params) {
    protocol_request.Set("params",
                         command_params->additional_properties.Clone());
  }
  if (session_id.has_value()) {
    protocol_request.Set("sessionId", session_id.value());
  }

  std::string json;
  base::JSONWriter::Write(protocol_request, &json);

  agent_host_->DispatchProtocolMessage(this,
                                       base::as_bytes(base::make_span(json)));
}

void ExtensionDevToolsClientHost::InfoBarDestroyed() {
  detach_reason_ = api::debugger::DetachReason::kCanceledByUser;
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

  auto args(OnDetach::Create(debuggee_, detach_reason_));
  auto event =
      std::make_unique<Event>(events::DEBUGGER_ON_DETACH, OnDetach::kEventName,
                              std::move(args), profile_);
  EventRouter::Get(profile_)->DispatchEventToExtension(extension_id(),
                                                       std::move(event));
}

void ExtensionDevToolsClientHost::OnProfileWillBeDestroyed(Profile* profile) {
  if (profile == profile_) {
    Close();
  }
}

void ExtensionDevToolsClientHost::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  if (extension->id() == extension_id())
    Close();
}

void ExtensionDevToolsClientHost::OnAppTerminating() {
  Close();
}

void ExtensionDevToolsClientHost::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  DCHECK(agent_host == agent_host_.get());
  if (!EventRouter::Get(profile_))
    return;

  std::string_view message_str = base::as_string_view(message);
  std::optional<base::Value> result = base::JSONReader::Read(
      message_str, base::JSON_REPLACE_INVALID_CHARACTERS);
  if (!result || !result->is_dict()) {
    LOG(ERROR) << "Tried to send invalid message to extension: " << message_str;
    return;
  }
  base::Value::Dict& dictionary = result->GetDict();

  std::optional<int> id = dictionary.FindInt("id");
  if (!id) {
    std::string* method_name = dictionary.FindString("method");
    if (!method_name)
      return;

    OnEvent::Params params;
    if (base::Value::Dict* params_value = dictionary.FindDict("params")) {
      params.additional_properties = std::move(*params_value);
    }

    DebuggerSession session;
    DebuggerSessionFromDebugee(session, debuggee_,
                               dictionary.FindString("sessionId"));

    auto args(OnEvent::Create(session, *method_name, params));
    auto event =
        std::make_unique<Event>(events::DEBUGGER_ON_EVENT, OnEvent::kEventName,
                                std::move(args), profile_);
    EventRouter::Get(profile_)->DispatchEventToExtension(extension_id(),
                                                         std::move(event));
  } else {
    auto it = pending_requests_.find(*id);
    if (it == pending_requests_.end())
      return;

    it->second->SendResponseBody(base::Value(std::move(dictionary)));
    pending_requests_.erase(it);
  }
}

bool ExtensionDevToolsClientHost::MayAttachToRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  std::string error;
  return ExtensionMayAttachToRenderFrameHost(*extension_, profile_,
                                             render_frame_host, &error);
}

bool ExtensionDevToolsClientHost::MayAttachToURL(const GURL& url,
                                                 bool is_webui) {
  if (is_webui)
    return false;
  std::string error;
  return ExtensionMayAttachToURLOrInnerURL(*extension_, profile_, url, nullptr,
                                           &error);
}

bool ExtensionDevToolsClientHost::IsTrusted() {
  return ExtensionIsTrusted(*extension_);
}

bool ExtensionDevToolsClientHost::MayReadLocalFiles() {
  return util::AllowFileAccess(extension_->id(), profile_);
}

bool ExtensionDevToolsClientHost::MayWriteLocalFiles() {
  return false;
}

std::optional<url::Origin>
ExtensionDevToolsClientHost::GetNavigationInitiatorOrigin() {
  // Ensure that navigations started by debugger API are treated as
  // renderer-initiated by this extension, so that URL spoof defenses are in
  // effect.
  return extension_->origin();
}

// DebuggerFunction -----------------------------------------------------------

DebuggerFunction::DebuggerFunction() : client_host_(nullptr) {}

DebuggerFunction::~DebuggerFunction() = default;

std::string DebuggerFunction::FormatErrorMessage(const std::string& format) {
  if (debuggee_.tab_id) {
    return ErrorUtils::FormatErrorMessage(
        format, kTabTargetType, base::NumberToString(*debuggee_.tab_id));
  }
  if (debuggee_.extension_id) {
    return ErrorUtils::FormatErrorMessage(format, kBackgroundPageTargetType,
                                          *debuggee_.extension_id);
  }

  return ErrorUtils::FormatErrorMessage(format, kOpaqueTargetType,
                                        *debuggee_.target_id);
}

bool DebuggerFunction::InitAgentHost(std::string* error) {
  if (debuggee_.tab_id) {
    WebContents* web_contents = nullptr;
    bool result = ExtensionTabUtil::GetTabById(
        *debuggee_.tab_id, browser_context(), include_incognito_information(),
        &web_contents);
    if (result && web_contents) {
      if (!ExtensionMayAttachToWebContents(
              *extension(), Profile::FromBrowserContext(browser_context()),
              *web_contents, error)) {
        return false;
      }

      agent_host_ = DevToolsAgentHost::GetOrCreateFor(web_contents);
    }
  } else if (debuggee_.extension_id) {
    ExtensionHost* extension_host =
        ProcessManager::Get(browser_context())
            ->GetBackgroundHostForExtension(*debuggee_.extension_id);
    if (extension_host) {
      const GURL& url = extension_host->GetLastCommittedURL();
      if (extension()->permissions_data()->IsRestrictedUrl(url, error) ||
          extension()->permissions_data()->IsPolicyBlockedHost(url)) {
        return false;
      }
      agent_host_ =
          DevToolsAgentHost::GetOrCreateFor(extension_host->host_contents());
    }
  } else if (debuggee_.target_id) {
    scoped_refptr<DevToolsAgentHost> agent_host =
        DevToolsAgentHost::GetForId(*debuggee_.target_id);
    if (agent_host) {
      if (!ExtensionMayAttachToAgentHost(
              *extension(), include_incognito_information(),
              Profile::FromBrowserContext(browser_context()), *agent_host,
              error)) {
        return false;
      }
      agent_host_ = std::move(agent_host);
    } else if (*debuggee_.target_id == kBrowserTargetId &&
               ExtensionIsTrusted(*extension())) {
      // TODO(caseq): get rid of the below code, browser agent host should
      // really be a singleton.
      // Re-use existing browser agent hosts.
      const ExtensionId& extension_id = extension()->id();
      AttachedClientHosts& hosts = g_attached_client_hosts.Get();
      auto it = base::ranges::find_if(
          hosts, [&extension_id](ExtensionDevToolsClientHost* client_host) {
            return client_host->extension_id() == extension_id &&
                   client_host->agent_host() &&
                   client_host->agent_host()->GetType() ==
                       DevToolsAgentHost::kTypeBrowser;
          });
      agent_host_ = it != hosts.end()
                        ? (*it)->agent_host()
                        : DevToolsAgentHost::CreateForBrowser(
                              nullptr /* tethering_task_runner */,
                              DevToolsAgentHost::CreateServerSocketCallback());
    }
  } else {
    *error = kInvalidTargetError;
    return false;
  }

  if (!agent_host_.get()) {
    *error = FormatErrorMessage(kNoTargetError);
    return false;
  }
  return true;
}

bool DebuggerFunction::InitClientHost(std::string* error) {
  if (!InitAgentHost(error))
    return false;

  client_host_ = FindClientHost();
  if (!client_host_) {
    *error = FormatErrorMessage(kNotAttachedError);
    return false;
  }

  return true;
}

ExtensionDevToolsClientHost* DebuggerFunction::FindClientHost() {
  if (!agent_host_.get())
    return nullptr;

  const ExtensionId& extension_id = extension()->id();
  DevToolsAgentHost* agent_host = agent_host_.get();
  AttachedClientHosts& hosts = g_attached_client_hosts.Get();
  auto it = base::ranges::find_if(
      hosts,
      [&agent_host, &extension_id](ExtensionDevToolsClientHost* client_host) {
        return client_host->agent_host() == agent_host &&
               client_host->extension_id() == extension_id;
      });

  return it == hosts.end() ? nullptr : *it;
}

// DebuggerAttachFunction -----------------------------------------------------

DebuggerAttachFunction::DebuggerAttachFunction() = default;

DebuggerAttachFunction::~DebuggerAttachFunction() = default;

ExtensionFunction::ResponseAction DebuggerAttachFunction::Run() {
  std::optional<Attach::Params> params = Attach::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  CopyDebuggee(&debuggee_, params->target);
  std::string error;
  if (!InitAgentHost(&error))
    return RespondNow(Error(std::move(error)));

  if (!DevToolsAgentHost::IsSupportedProtocolVersion(
          params->required_version)) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        kProtocolVersionNotSupportedError, params->required_version)));
  }

  if (FindClientHost()) {
    return RespondNow(Error(FormatErrorMessage(kAlreadyAttachedError)));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  auto host = std::make_unique<ExtensionDevToolsClientHost>(
      profile, agent_host_.get(), extension(), worker_id(), debuggee_);

  if (!host->Attach()) {
    return RespondNow(Error(kRestrictedError));
  }

  host.release();  // An attached client host manages its own lifetime.

  if (!(Manifest::IsPolicyLocation(extension()->location()) ||
        Manifest::IsComponentLocation(extension()->location()))) {
    bool is_developer_mode =
        profile->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode);
    base::UmaHistogramBoolean("Extensions.Debugger.UserIsInDeveloperMode",
                              is_developer_mode);
  }

  return RespondNow(NoArguments());
}

// DebuggerDetachFunction -----------------------------------------------------

DebuggerDetachFunction::DebuggerDetachFunction() = default;

DebuggerDetachFunction::~DebuggerDetachFunction() = default;

ExtensionFunction::ResponseAction DebuggerDetachFunction::Run() {
  std::optional<Detach::Params> params = Detach::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  CopyDebuggee(&debuggee_, params->target);
  std::string error;
  if (!InitClientHost(&error))
    return RespondNow(Error(std::move(error)));

  client_host_->RespondDetachedToPendingRequests();
  client_host_->Close();
  return RespondNow(NoArguments());
}

// DebuggerSendCommandFunction ------------------------------------------------

DebuggerSendCommandFunction::DebuggerSendCommandFunction() = default;

DebuggerSendCommandFunction::~DebuggerSendCommandFunction() = default;

ExtensionFunction::ResponseAction DebuggerSendCommandFunction::Run() {
  std::optional<SendCommand::Params> params =
      SendCommand::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  DebuggeeFromDebuggerSession(debuggee_, params->target);
  std::string error;
  if (!InitClientHost(&error))
    return RespondNow(Error(std::move(error)));

  client_host_->SendMessageToBackend(
      this, params->method, base::OptionalToPtr(params->command_params),
      params->target.session_id);
  if (did_respond())
    return AlreadyResponded();
  return RespondLater();
}

void DebuggerSendCommandFunction::SendResponseBody(base::Value response) {
  if (base::Value* error_body = response.GetDict().Find("error")) {
    std::string error;
    base::JSONWriter::Write(*error_body, &error);
    Respond(Error(std::move(error)));
    return;
  }

  SendCommand::Results::Result result;
  if (base::Value::Dict* result_body = response.GetDict().FindDict("result")) {
    result.additional_properties = std::move(*result_body);
  }

  Respond(ArgumentList(SendCommand::Results::Create(result)));
}

void DebuggerSendCommandFunction::SendDetachedError() {
  Respond(Error(kDetachedWhileHandlingError));
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

base::Value::Dict SerializeTarget(scoped_refptr<DevToolsAgentHost> host) {
  base::Value::Dict dictionary;
  dictionary.Set(kTargetIdField, host->GetId());
  dictionary.Set(kTargetTitleField, host->GetTitle());
  dictionary.Set(kTargetAttachedField, host->IsAttached());
  dictionary.Set(kTargetUrlField, host->GetURL().spec());

  std::string type = host->GetType();
  std::string target_type = kTargetTypeOther;
  if (type == DevToolsAgentHost::kTypePage) {
    int tab_id =
        extensions::ExtensionTabUtil::GetTabId(host->GetWebContents());
    if (tab_id != api::tabs::TAB_ID_NONE) {
      dictionary.Set(kTargetTabIdField, tab_id);
    } else {
      dictionary.Set(kTargetExtensionIdField, host->GetURL().host());
    }
    target_type = kTargetTypePage;
  } else if (type == ChromeDevToolsManagerDelegate::kTypeBackgroundPage) {
    dictionary.Set(kTargetExtensionIdField, host->GetURL().host());
    target_type = kTargetTypeBackgroundPage;
  } else if (type == DevToolsAgentHost::kTypeServiceWorker ||
             type == DevToolsAgentHost::kTypeSharedWorker) {
    target_type = kTargetTypeWorker;
  }

  dictionary.Set(kTargetTypeField, target_type);

  GURL favicon_url = host->GetFaviconURL();
  if (favicon_url.is_valid())
    dictionary.Set(kTargetFaviconUrlField, favicon_url.spec());

  return dictionary;
}

}  // namespace

DebuggerGetTargetsFunction::DebuggerGetTargetsFunction() = default;

DebuggerGetTargetsFunction::~DebuggerGetTargetsFunction() = default;

ExtensionFunction::ResponseAction DebuggerGetTargetsFunction::Run() {
  content::DevToolsAgentHost::List list = DevToolsAgentHost::GetOrCreateAll();
  base::Value::List result;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  for (auto& host : list) {
    // TODO(crbug.com/40233332): hide all Tab targets for now to avoid
    // compatibility problems. Consider exposing them later when they're fully
    // supported, and compatibility considerations are better understood.
    if (host->GetType() == DevToolsAgentHost::kTypeTab)
      continue;
    if (!ExtensionMayAttachToTargetProfile(
            profile, include_incognito_information(), *host)) {
      continue;
    }
#if BUILDFLAG(ENABLE_PDF)
    // OOPIF PDF viewer only. Don't list the `content::DevToolsAgentHost`s for
    // inner PDF frames. PDF extension frames and PDF content frames shouldn't
    // be exposed to chrome.debugger clients.
    auto* process_host = host->GetProcessHost();
    if (chrome_pdf::features::IsOopifPdfEnabled() &&
        (IsPdfExtensionUrl(host->GetURL()) ||
         (process_host && process_host->IsPdf()))) {
      continue;
    }
#endif  // BUILDFLAG(ENABLE_PDF)
    result.Append(SerializeTarget(host));
  }

  return RespondNow(WithArguments(std::move(result)));
}

}  // namespace extensions
