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
#include "chrome/browser/plugins/flash_download_interception.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_infobar_delegates.h"
#include "chrome/browser/plugins/plugin_installer.h"
#include "chrome/browser/plugins/plugin_installer_observer.h"
#include "chrome/browser/plugins/reload_plugin_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "components/component_updater/component_updater_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/webplugininfo.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

using content::PluginService;

// PluginObserver -------------------------------------------------------------

class PluginObserver::PluginPlaceholderHost : public PluginInstallerObserver {
 public:
  PluginPlaceholderHost(
      PluginObserver* observer,
      base::string16 plugin_name,
      PluginInstaller* installer,
      mojo::PendingRemote<chrome::mojom::PluginRenderer> plugin_renderer_remote)
      : PluginInstallerObserver(installer),
        observer_(observer),
        plugin_renderer_remote_(std::move(plugin_renderer_remote)) {
    plugin_renderer_remote_.set_disconnect_handler(
        base::Bind(&PluginObserver::RemovePluginPlaceholderHost,
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

class PluginObserver::ComponentObserver
    : public update_client::UpdateClient::Observer {
 public:
  using Events = update_client::UpdateClient::Observer::Events;
  ComponentObserver(
      PluginObserver* observer,
      const std::string& component_id,
      mojo::PendingRemote<chrome::mojom::PluginRenderer> plugin_renderer_remote)
      : observer_(observer),
        component_id_(component_id),
        plugin_renderer_remote_(std::move(plugin_renderer_remote)) {
    plugin_renderer_remote_.set_disconnect_handler(
        base::Bind(&PluginObserver::RemoveComponentObserver,
                   base::Unretained(observer_), this));
    g_browser_process->component_updater()->AddObserver(this);
  }

  ~ComponentObserver() override {
    g_browser_process->component_updater()->RemoveObserver(this);
  }

  void OnEvent(Events event, const std::string& id) override {
    // TODO(lukasza): https://crbug.com/760637: |routing_id_| might live in a
    // different process than the RenderViewHost - need to track and use
    // placeholder's process when calling Send below.

    if (id != component_id_)
      return;
    switch (event) {
      case Events::COMPONENT_UPDATED:
        plugin_renderer_remote_->UpdateSuccess();
        observer_->RemoveComponentObserver(this);
        break;
      case Events::COMPONENT_UPDATE_FOUND:
        plugin_renderer_remote_->UpdateDownloading();
        break;
      case Events::COMPONENT_NOT_UPDATED:
      case Events::COMPONENT_UPDATE_ERROR:
        plugin_renderer_remote_->UpdateFailure();
        observer_->RemoveComponentObserver(this);
        break;
      default:
        // No message to send.
        break;
    }
  }

 private:
  PluginObserver* observer_;
  std::string component_id_;
  mojo::Remote<chrome::mojom::PluginRenderer> plugin_renderer_remote_;
  DISALLOW_COPY_AND_ASSIGN(ComponentObserver);
};

PluginObserver::PluginObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      plugin_host_bindings_(web_contents, this) {}

PluginObserver::~PluginObserver() {
}

void PluginObserver::PluginCrashed(const base::FilePath& plugin_path,
                                   base::ProcessId plugin_pid) {
  DCHECK(!plugin_path.value().empty());

  base::string16 plugin_name =
      PluginService::GetInstance()->GetPluginDisplayNameByPath(plugin_path);
  base::string16 infobar_text;
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
    const base::string16& plugin_name) {
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

void PluginObserver::BlockedComponentUpdatedPlugin(
    mojo::PendingRemote<chrome::mojom::PluginRenderer> plugin_renderer,
    const std::string& identifier) {
  auto component_observer = std::make_unique<ComponentObserver>(
      this, identifier, std::move(plugin_renderer));
  component_observers_[component_observer.get()] =
      std::move(component_observer);
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      identifier, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      component_updater::Callback());
}

void PluginObserver::RemoveComponentObserver(
    ComponentObserver* component_observer) {
  component_observers_.erase(component_observer);
}

void PluginObserver::RemovePluginPlaceholderHost(
    PluginPlaceholderHost* placeholder) {
  plugin_placeholders_.erase(placeholder);
}

void PluginObserver::ShowFlashPermissionBubble() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  FlashDownloadInterception::InterceptFlashDownloadNavigation(
      web_contents(), web_contents()->GetLastCommittedURL());
}

void PluginObserver::CouldNotLoadPlugin(const base::FilePath& plugin_path) {
  g_browser_process->GetMetricsServicesManager()->OnPluginLoadingError(
      plugin_path);
  base::string16 plugin_name =
      PluginService::GetInstance()->GetPluginDisplayNameByPath(plugin_path);
  CreatePluginObserverInfoBar(InfoBarService::FromWebContents(web_contents()),
                              plugin_name);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PluginObserver)
