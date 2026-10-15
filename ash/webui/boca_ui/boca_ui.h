// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_BOCA_UI_H_
#define ASH_WEBUI_BOCA_UI_BOCA_UI_H_

#include <memory>
#include <string>

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUIDataSource;
}  // namespace content

class HostContentSettingsMap;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::boca {
class BocaUI;
class BocaAppHandler;
class BocaSessionManager;
class OnTaskSystemWebAppManager;

// A delegate used during data source creation to expose some //chrome
// functionality to the data source
class BocaUIDelegate {
 public:
  virtual ~BocaUIDelegate() = default;
  // Takes a WebUIDataSource, and populates its load-time data.
  virtual void PopulateLoadTimeData(content::WebUIDataSource* source) = 0;
};

// The WebUI for chrome-untrusted://boca-app/. Boca app is directly served in
// main frame.
class BocaUI : public ui::UntrustedWebUIController,
               public boca::mojom::BocaPageHandlerFactory {
 public:
  // `delegate` is only used during construction, to populate the data
  // source's load-time data; it is not retained.
  // `host_content_settings_map` is the one belonging to the user's profile,
  // resolved by the caller. It must be non-null and must outlive `this`.
  // `on_task_system_web_app_manager` is null when there is no on-task session.
  // The remaining four are what a GeminiStatusFetcher is built from; Create()
  // builds a fresh one per page handler.
  BocaUI(content::WebUI* web_ui,
         std::unique_ptr<BocaUIDelegate> delegate,
         BocaSessionManager* boca_session_manager,
         bool is_producer,
         HostContentSettingsMap* host_content_settings_map,
         OnTaskSystemWebAppManager* on_task_system_web_app_manager,
         std::string gaia_id,
         signin::IdentityManager* identity_manager,
         scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
         PrefService* pref_service);
  BocaUI(const BocaUI&) = delete;
  BocaUI& operator=(const BocaUI&) = delete;
  ~BocaUI() override;
  void BindInterface(
      mojo::PendingReceiver<boca::mojom::BocaPageHandlerFactory> factory);

  // ash::boca::mojom ::BocaPageHandlerFactory:
  void Create(mojo::PendingReceiver<boca::mojom::PageHandler> page_handler,
              mojo::PendingRemote<boca::mojom::Page> page) override;

 private:
  const raw_ref<BocaSessionManager> boca_session_manager_;
  const bool is_producer_;
  // Create() runs when the page binds, which is after construction, so these
  // are kept until then: the map to build the ContentSettingsHandler with, the
  // rest to build the page handler and its GeminiStatusFetcher.
  const raw_ref<HostContentSettingsMap> host_content_settings_map_;
  const raw_ptr<OnTaskSystemWebAppManager> on_task_system_web_app_manager_;
  const std::string gaia_id_;
  const raw_ref<signin::IdentityManager> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ref<PrefService> pref_service_;
  mojo::Receiver<boca::mojom::BocaPageHandlerFactory> receiver_{this};
  std::unique_ptr<BocaAppHandler> page_handler_impl_;
  SpotlightService spotlight_service_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::boca

#endif  // ASH_WEBUI_BOCA_UI_BOCA_UI_H_
