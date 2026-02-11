// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/views/devtools_floaty.h"

#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

// A WebContentsDelegate for the DevTools GreenDev Floaty dialog.
class DevToolsFloatyWebContentsDelegate : public content::WebContentsDelegate {
 public:
  DevToolsFloatyWebContentsDelegate() = default;
  ~DevToolsFloatyWebContentsDelegate() override = default;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override {}

  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override {
    DevToolsWindow::InspectElement(content::RenderFrameHost::FromID(
                                       render_frame_host.GetProcess()->GetID(),
                                       render_frame_host.GetRoutingID()),
                                   params.x, params.y);
    return true;
  }
};

class DevToolsFloatyDialogDelegate;
std::map<int, DevToolsFloatyDialogDelegate*>& GetFloatyRegistry() {
  static base::NoDestructor<std::map<int, DevToolsFloatyDialogDelegate*>>
      registry;
  return *registry;
}

// A simple delegate to handle requests for opening the DevTools panel for the
// DevToolsUIBindings object.
class FloatyBindingsDelegate : public DevToolsUIBindings::Delegate {
 public:
  FloatyBindingsDelegate(views::Widget* widget, int process_id, int routing_id)
      : widget_(widget), process_id_(process_id), routing_id_(routing_id) {}
  ~FloatyBindingsDelegate() override = default;

  void OpenInNewTab(const std::string& url) override {
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(process_id_, routing_id_);
    if (!rfh) {
      return;
    }
    content::WebContents* inspected_web_contents =
        content::WebContents::FromRenderFrameHost(rfh);

    if (url == "magic:open-devtools") {
      Profile* profile = Profile::FromBrowserContext(
          inspected_web_contents->GetBrowserContext());
      content::DevToolsManagerDelegate::DevToolsOptions options("greendev");
      DevToolsWindow::OpenDevToolsWindow(
          inspected_web_contents, profile,
          DevToolsOpenedByAction::kContextMenuInspect, options);
    } else {
      // Open regular URLs in a new tab.
      content::OpenURLParams params(GURL(url), content::Referrer(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                    ui::PAGE_TRANSITION_LINK, false);
      // We use the inspected contents to open the URL, ensuring it opens in the
      // correct browser window/context.
      inspected_web_contents->OpenURL(params, base::DoNothing());
    }
  }

  void ActivateWindow() override {
    if (widget_) {
      widget_->Restore();
      widget_->Show();
    }
  }

  content::WebContents* GetInspectedWebContents() override { return nullptr; }
  void CloseWindow() override {}
  void Inspect(scoped_refptr<content::DevToolsAgentHost> host) override {}
  void SetInspectedPageBounds(const gfx::Rect& rect) override {}
  void InspectElementCompleted() override {}
  void SetIsDocked(bool is_docked) override {}
  void OpenSearchResultsInNewTab(const std::string& query) override {}
  void SetWhitelistedShortcuts(const std::string& message) override {}
  void SetEyeDropperActive(bool active) override {}
  void OpenNodeFrontend() override {}
  void InspectedContentsClosing() override {}
  void OnLoadCompleted() override {}
  void ReadyForTest() override {}
  void ConnectionReady() override {}
  void SetOpenNewWindowForPopups(bool value) override {}
  infobars::ContentInfoBarManager* GetInfoBarManager() override {
    return nullptr;
  }
  void RenderProcessGone(bool crashed) override {}
  void ShowCertificateViewer(const std::string& cert_chain) override {}
  int GetDockStateForLogging() override { return 0; }
  int GetOpenedByForLogging() override { return 0; }
  int GetClosedByForLogging() override { return 0; }

 private:
  raw_ptr<views::Widget> widget_;
  int process_id_;
  int routing_id_;
};

// The DialogDelegate that handles showing the GreenDev Floaty window, by
// creating a WebView and loading the devtools:// entrypoint within. Reacts to
// external state changes and handles messaging and cleanup for the delegate.
class DevToolsFloatyDialogDelegate : public views::DialogDelegate,
                                     public ProfileObserver,
                                     public content::WebContentsObserver,
                                     public views::WidgetObserver,
                                     public content::DevToolsAgentHostClient {
 public:
  explicit DevToolsFloatyDialogDelegate(
      Profile* profile,
      std::unique_ptr<content::WebContents> web_contents,
      std::unique_ptr<DevToolsFloatyWebContentsDelegate> web_contents_delegate,
      int process_id,
      int routing_id,
      int backend_node_id)
      : WebContentsObserver(web_contents.get()),
        web_contents_(std::move(web_contents)),
        web_contents_delegate_(std::move(web_contents_delegate)),
        target_process_id_(process_id),
        target_routing_id_(routing_id),
        backend_node_id_(backend_node_id) {
    profile_observation_.Observe(profile);
    SetModalType(ui::mojom::ModalType::kNone);
    SetTitle(u"DevTools Floaty");
    SetButtons(0);

    auto webview = std::make_unique<views::WebView>(profile);
    webview->SetWebContents(web_contents_.get());
    web_contents_->SetDelegate(web_contents_delegate_.get());
    SetContentsView(std::move(webview));

    if (backend_node_id_) {
      GetFloatyRegistry()[backend_node_id_] = this;
    }
  }

  ~DevToolsFloatyDialogDelegate() override {
    if (backend_node_id_) {
      GetFloatyRegistry().erase(backend_node_id_);
    }

    if (GetWidget()) {
      GetWidget()->RemoveObserver(this);
    }
  }

  // content::DevToolsAgentHostClient:
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    std::string_view message_sp(reinterpret_cast<const char*>(message.data()),
                                message.size());

    std::optional<base::Value> value =
        base::JSONReader::Read(message_sp, base::JSON_PARSE_RFC);
    if (!value || !value->is_dict()) {
      return;
    }

    const base::DictValue& dict = value->GetDict();
    const std::string* method = dict.FindString("method");

    if (method && *method == "Overlay.inspectedElementWindowRestored") {
      const base::DictValue* params = dict.FindDict("params");
      if (params) {
        std::optional<int> backend_node_id = params->FindInt("backendNodeId");
        if (backend_node_id && *backend_node_id == backend_node_id_) {
          DevToolsFloaty::Restore(*backend_node_id);
        }
      }
    } else if (method && *method == "Overlay.inspectPanelShowRequested") {
      content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
          target_process_id_, target_routing_id_);
      if (rfh) {
        content::WebContents* inspected_web_contents =
            content::WebContents::FromRenderFrameHost(rfh);
        Profile* profile = Profile::FromBrowserContext(
            inspected_web_contents->GetBrowserContext());
        content::DevToolsManagerDelegate::DevToolsOptions options("greendev");
        DevToolsWindow::OpenDevToolsWindow(
            inspected_web_contents, profile,
            DevToolsOpenedByAction::kContextMenuInspect, options);
      }
    }
  }

  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {
    agent_host_ = nullptr;
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    if (agent_host_ && backend_node_id_) {
      content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
          target_process_id_, target_routing_id_);
      if (rfh) {
        content::WebContents* inspected_contents =
            content::WebContents::FromRenderFrameHost(rfh);
        DevToolsWindow* window =
            DevToolsWindow::GetInstanceForInspectedWebContents(
                inspected_contents);
        if (window) {
          content::WebContents* devtools_contents =
              window->GetDevToolsWebContents();
          if (devtools_contents) {
            DevToolsUIBindings* bindings =
                DevToolsUIBindings::ForWebContents(devtools_contents);
            if (bindings) {
              bindings->CallClientMethod("GreenDevPanel", "closeSession",
                                         base::Value(backend_node_id_));
            }
          }
        }
      }
    }
    // Explicitly destroy WebContents to ensure bindings are detached and
    // session is closed.
    web_contents_.reset();
    if (agent_host_) {
      agent_host_->DetachClient(this);
    }
    agent_host_ = nullptr;
  }

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override {
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
        target_process_id_, target_routing_id_);
    if (rfh) {
      DevToolsUIBindings* bindings =
          DevToolsUIBindings::ForWebContents(web_contents_.get());
      if (bindings) {
        bindings->SetDelegate(new FloatyBindingsDelegate(
            GetWidget(), target_process_id_, target_routing_id_));
        agent_host_ = content::DevToolsAgentHost::GetOrCreateFor(
            content::WebContents::FromRenderFrameHost(rfh));
        bindings->AttachTo(agent_host_);
        agent_host_->AttachClient(this);
        const char* enable_overlay_message =
            "{\"id\":102,\"method\":\"Overlay.enable\"}";
        agent_host_->DispatchProtocolMessage(
            this, base::as_byte_span(std::string(enable_overlay_message)));

        const std::string message = base::StringPrintf(
            "{\"id\":1003,\"method\":\"Overlay.setShowInspectedElementAnchor\","
            "\"params\":{\"InspectedElementAnchorConfig\":{\"backendNodeId\":%"
            "d}"
            "}}",
            backend_node_id_);
        agent_host_->DispatchProtocolMessage(this, base::as_byte_span(message));
      }
    }
  }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    profile_observation_.Reset();

    views::Widget* widget = GetWidget();
    if (!widget) {
      web_contents_.reset();
      return;
    }

    auto* web_view = static_cast<views::WebView*>(GetContentsView());
    if (web_view) {
      web_view->SetWebContents(nullptr);
    }

    web_contents_.reset();

    widget->CloseNow();
  }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<DevToolsFloatyWebContentsDelegate> web_contents_delegate_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  int target_process_id_;
  int target_routing_id_;
  int backend_node_id_;
};

void ShowWindow(Profile* profile,
                int process_id,
                int routing_id,
                gfx::Point position,
                int backend_node_id) {
  CHECK(base::FeatureList::IsEnabled(features::kDevToolsGreenDevUi));

  auto web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  content::WebContents* raw_web_contents = web_contents.get();
  auto delegate_ptr = std::make_unique<DevToolsFloatyDialogDelegate>(
      profile, std::move(web_contents),
      std::make_unique<DevToolsFloatyWebContentsDelegate>(), process_id,
      routing_id, backend_node_id);
  DevToolsFloatyDialogDelegate* delegate = delegate_ptr.get();

  GURL url = GURL(base::StringPrintf(
      "devtools://devtools/bundled/entrypoints/greendev_floaty/"
      "floaty.html#processId=%d&routingId=%d&x=%d&y=%d&backendNodeId=%d",
      process_id, routing_id, position.x(), position.y(), backend_node_id));
  raw_web_contents->GetController().LoadURL(url, content::Referrer(),
                                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                            std::string());

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(process_id, routing_id);
  if (!rfh) {
    return;
  }
  content::WebContents* inspected_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!inspected_contents) {
    return;
  }

  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      std::move(delegate_ptr), inspected_contents->GetTopLevelNativeWindow(),
      gfx::NativeView());
  if (!widget) {
    return;
  }
  widget->AddObserver(delegate);
  // TODO(https://crbug.com/482553156): Remove these.
  constexpr int offsetX = 30;
  constexpr int offsetY = 175;
  widget->SetBounds(
      gfx::Rect(position.x() + offsetX, position.y() + offsetY, 400, 400));
  widget->Show();
}

// An AgentHostClient for the DevTools Floaty object, responsible for
// dispatching overlay protocol messages.
class InspectElementGeminiClient : public content::DevToolsAgentHostClient {
 public:
  InspectElementGeminiClient(content::BrowserContext* browser_context,
                             int render_process_id,
                             int render_frame_id,
                             int x,
                             int y)
      : browser_context_(browser_context),
        render_process_id_(render_process_id),
        render_frame_id_(render_frame_id),
        x_(x),
        y_(y) {}
  ~InspectElementGeminiClient() override = default;

  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    std::string_view message_sp(reinterpret_cast<const char*>(message.data()),
                                message.size());

    std::optional<base::Value> value =
        base::JSONReader::Read(message_sp, base::JSON_PARSE_RFC);
    if (!value || !value->is_dict()) {
      return;
    }

    const base::DictValue& dict = value->GetDict();

    // Handle response to DOM.getNodeForLocation (id: 103)
    // This is no longer used but kept if we switch back to manual calls.
    // ...

    const std::string* method = dict.FindString("method");
    if (method && *method == "Overlay.inspectNodeRequested") {
      const base::DictValue* params = dict.FindDict("params");
      if (params) {
        std::optional<int> backend_node_id = params->FindInt("backendNodeId");
        if (backend_node_id) {
          if (!triggered_) {
            triggered_ = true;
            ShowWindow(Profile::FromBrowserContext(browser_context_),
                       render_process_id_, render_frame_id_, gfx::Point(x_, y_),
                       *backend_node_id);
            keep_alive_host_ = agent_host;
          } else {
            DevToolsFloaty::Restore(*backend_node_id);
          }
        }
      }
    } else if (method && *method == "Overlay.inspectPanelShowRequested") {
      content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
          render_process_id_, render_frame_id_);
      if (rfh) {
        content::WebContents* inspected_web_contents =
            content::WebContents::FromRenderFrameHost(rfh);
        Profile* profile = Profile::FromBrowserContext(
            inspected_web_contents->GetBrowserContext());
        content::DevToolsManagerDelegate::DevToolsOptions options("greendev");
        DevToolsWindow::OpenDevToolsWindow(
            inspected_web_contents, profile,
            DevToolsOpenedByAction::kContextMenuInspect, options);
      }
    } else {
    }
  }

  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {
    delete this;
  }

 private:
  raw_ptr<content::BrowserContext> browser_context_;
  int render_process_id_;
  int render_frame_id_;
  int x_;
  int y_;
  bool triggered_ = false;
  scoped_refptr<content::DevToolsAgentHost> keep_alive_host_;
};

}  // namespace

namespace DevToolsFloaty {

void Show(Profile* profile,
          int process_id,
          int routing_id,
          gfx::Point position,
          int backend_node_id) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(process_id, routing_id);
  if (!rfh) {
    return;
  }

  scoped_refptr<content::DevToolsAgentHost> agent(
      content::DevToolsAgentHost::GetOrCreateFor(
          content::WebContents::FromRenderFrameHost(rfh)));
  InspectElementGeminiClient* client = new InspectElementGeminiClient(
      rfh->GetBrowserContext(), rfh->GetProcess()->GetDeprecatedID(),
      rfh->GetRoutingID(), position.x(), position.y());
  agent->AttachClient(client);
  const char* enable_dom_message = "{\"id\":100,\"method\":\"DOM.enable\"}";
  agent->DispatchProtocolMessage(
      client, base::as_byte_span(std::string(enable_dom_message)));
  const char* enable_overlay_message =
      "{\"id\":101,\"method\":\"Overlay.enable\"}";
  agent->DispatchProtocolMessage(
      client, base::as_byte_span(std::string(enable_overlay_message)));
  agent->InspectElement(rfh, position.x(), position.y());
}

void Restore(int backend_node_id) {
  CHECK(base::FeatureList::IsEnabled(features::kDevToolsGreenDevUi));
  CHECK(backend_node_id > 0);

  auto it = GetFloatyRegistry().find(backend_node_id);
  if (it == GetFloatyRegistry().end()) {
    return;
  }

  views::Widget* widget = it->second->GetWidget();
  if (widget) {
    widget->Restore();
    widget->Show();
  }
}

}  // namespace DevToolsFloaty
