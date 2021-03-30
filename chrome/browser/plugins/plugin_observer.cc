// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_observer.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_infobar_delegates.h"
#include "chrome/browser/plugins/plugin_installer.h"
#include "chrome/browser/plugins/plugin_installer_observer.h"
#include "chrome/browser/plugins/reload_plugin_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

using content::PluginService;

// PluginObserver -------------------------------------------------------------

class PluginObserver::PluginPlaceholderHost : public PluginInstallerObserver {
 public:
  PluginPlaceholderHost(
      PluginObserver* observer,
      std::u16string plugin_name,
      PluginInstaller* installer,
      mojo::PendingRemote<chrome::mojom::PluginRenderer> plugin_renderer_remote)
      : PluginInstallerObserver(installer),
        observer_(observer),
        plugin_renderer_remote_(std::move(plugin_renderer_remote)) {
    plugin_renderer_remote_.set_disconnect_handler(
        base::BindOnce(&PluginObserver::RemovePluginPlaceholderHost,
                       base::Unretained(observer_), this));
    DCHECK(installer);
  }

  void DownloadFinished() override {
    plugin_renderer_remote_->FinishedDownloading();
  }

 private:
  PluginObserver* observer_;
  mojo::Remote<chrome::mojom::PluginRenderer> plugin_renderer_remote_;
};

PluginObserver::PluginObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      plugin_host_receivers_(web_contents, this) {}

PluginObserver::~PluginObserver() {
}

void PluginObserver::PluginCrashed(const base::FilePath& plugin_path,
                                   base::ProcessId plugin_pid) {
  DCHECK(!plugin_path.value().empty());

  std::u16string plugin_name =
      PluginService::GetInstance()->GetPluginDisplayNameByPath(plugin_path);
  std::u16string infobar_text;
#if defined(OS_WIN)
  // Find out whether the plugin process is still alive.
  // Note: Although the chances are slim, it is possible that after the plugin
  // process died, |plugin_pid| has been reused by a new process. The
  // consequence is that we will display |IDS_PLUGIN_DISCONNECTED_PROMPT| rather
  // than |IDS_PLUGIN_CRASHED_PROMPT| to the user, which seems acceptable.
  base::Process plugin_process =
      base::Process::OpenWithAccess(plugin_pid,
                                    PROCESS_QUERY_INFORMATION | SYNCHRONIZE);
  bool is_running = false;
  if (plugin_process.IsValid()) {
    int unused_exit_code = 0;
    is_running = base::GetTerminationStatus(plugin_process.Handle(),
                                            &unused_exit_code) ==
                 base::TERMINATION_STATUS_STILL_RUNNING;
    plugin_process.Close();
  }

  if (is_running) {
    infobar_text = l10n_util::GetStringFUTF16(IDS_PLUGIN_DISCONNECTED_PROMPT,
                                              plugin_name);
    UMA_HISTOGRAM_COUNTS_1M("Plugin.ShowDisconnectedInfobar", 1);
  } else {
    infobar_text = l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT,
                                              plugin_name);
    UMA_HISTOGRAM_COUNTS_1M("Plugin.ShowCrashedInfobar", 1);
  }
#else
  // Calling the POSIX version of base::GetTerminationStatus() may affect other
  // code which is interested in the process termination status. (Please see the
  // comment of the function.) Therefore, a better way is needed to distinguish
  // disconnections from crashes.
  infobar_text = l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT,
                                            plugin_name);
  UMA_HISTOGRAM_COUNTS_1M("Plugin.ShowCrashedInfobar", 1);
#endif

  ReloadPluginInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents()),
      &web_contents()->GetController(),
      infobar_text);
}

// static
void PluginObserver::CreatePluginObserverInfoBar(
    InfoBarService* infobar_service,
    const std::u16string& plugin_name) {
  SimpleAlertInfoBarDelegate::Create(
      infobar_service,
      infobars::InfoBarDelegate::PLUGIN_OBSERVER_INFOBAR_DELEGATE,
      &kExtensionCrashedIcon,
      l10n_util::GetStringFUTF16(IDS_PLUGIN_INITIALIZATION_ERROR_PROMPT,
                                 plugin_name));
}

void PluginObserver::BlockedOutdatedPlugin(
    mojo::PendingRemote<chrome::mojom::PluginRenderer> plugin_renderer,
    const std::string& identifier) {
  PluginFinder* finder = PluginFinder::GetInstance();
  // Find plugin to update.
  PluginInstaller* installer = NULL;
  std::unique_ptr<PluginMetadata> plugin;
  if (finder->FindPluginWithIdentifier(identifier, &installer, &plugin)) {
    auto plugin_placeholder = std::make_unique<PluginPlaceholderHost>(
        this, plugin->name(), installer, std::move(plugin_renderer));
    plugin_placeholders_[plugin_placeholder.get()] =
        std::move(plugin_placeholder);

    OutdatedPluginInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents()), installer,
        std::move(plugin));
  } else {
    NOTREACHED();
  }
}

void PluginObserver::RemovePluginPlaceholderHost(
    PluginPlaceholderHost* placeholder) {
  plugin_placeholders_.erase(placeholder);
}

void PluginObserver::ShowFlashPermissionBubble() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(tommycli): This is a no-op now. Delete this method in a followup.
}

void PluginObserver::CouldNotLoadPlugin(const base::FilePath& plugin_path) {
  g_browser_process->GetMetricsServicesManager()->OnPluginLoadingError(
      plugin_path);
  std::u16string plugin_name =
      PluginService::GetInstance()->GetPluginDisplayNameByPath(plugin_path);
  CreatePluginObserverInfoBar(InfoBarService::FromWebContents(web_contents()),
                              plugin_name);
}

void PluginObserver::OpenPDF(const GURL& url) {
  // WebViews should never trigger PDF downloads.
  if (extensions::WebViewGuest::FromWebContents(web_contents()))
    return;

  content::RenderFrameHost* render_frame_host =
      plugin_host_receivers_.GetCurrentTargetFrame();

  if (!content::ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
          render_frame_host->GetRoutingID(), url)) {
    return;
  }

  content::Referrer referrer = content::Referrer::SanitizeForRequest(
      url, content::Referrer(web_contents()->GetURL(),
                             network::mojom::ReferrerPolicy::kDefault));

#if BUILDFLAG(ENABLE_PLUGINS)
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("pdf_plugin_placeholder", R"(
        semantics {
          sender: "PDF Plugin Placeholder"
          description:
            "When the PDF Viewer is unavailable, a placeholder is shown for "
            "embedded PDFs. This placeholder allows the user to download and "
            "open the PDF file via a button."
          trigger:
            "The user clicks the 'View PDF' button in the PDF placeholder."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be disabled via 'Download PDF files instead of "
            "automatically opening them in Chrome' in settings under content. "
            "The feature is disabled by default."
          chrome_policy {
            AlwaysOpenPdfExternally {
              AlwaysOpenPdfExternally: false
            }
          }
        })");
  std::unique_ptr<download::DownloadUrlParameters> params =
      std::make_unique<download::DownloadUrlParameters>(
          url, render_frame_host->GetRenderViewHost()->GetProcess()->GetID(),
          render_frame_host->GetRoutingID(), traffic_annotation);
  params->set_referrer(referrer.url);
  params->set_referrer_policy(
      content::Referrer::ReferrerPolicyForUrlRequest(referrer.policy));

  content::BrowserContext::GetDownloadManager(
      web_contents()->GetBrowserContext())
      ->DownloadUrl(std::move(params));

#else   // !BUILDFLAG(ENABLE_PLUGINS)
  content::OpenURLParams open_url_params(
      url, referrer, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  // On Android, PDFs downloaded with a user gesture are auto-opened.
  open_url_params.user_gesture = true;
  web_contents()->OpenURL(open_url_params);
#endif  // BUILDFLAG(ENABLE_PLUGINS)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PluginObserver)
