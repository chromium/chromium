// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_service.h"

#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/common/constants.mojom.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/startup_metric_utils/browser/startup_metric_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/launchable.h"
#if defined(USE_OZONE)
#include "services/ws/public/cpp/input_devices/input_device_controller.h"
#endif
#endif
#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"
#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
#include "chrome/browser/spellchecker/spell_check_panel_host_impl.h"
#endif
#endif

class ChromeService::IOThreadContext : public service_manager::Service {
 public:
  IOThreadContext() {
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::UI});

#if defined(OS_CHROMEOS)
#if defined(USE_OZONE)
    input_device_controller_.AddInterface(&registry_, ui_task_runner);
#endif
    registry_.AddInterface(base::BindRepeating(&chromeos::Launchable::Bind,
                                               base::Unretained(&launchable_)),
                           ui_task_runner);
#endif
    registry_.AddInterface(base::BindRepeating(
        &startup_metric_utils::StartupMetricHostImpl::Create));
#if BUILDFLAG(ENABLE_SPELLCHECK)
    registry_with_source_info_.AddInterface(
        base::BindRepeating(&SpellCheckHostChromeImpl::Create), ui_task_runner);
#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
    registry_.AddInterface(
        base::BindRepeating(&SpellCheckPanelHostImpl::Create), ui_task_runner);
#endif
#endif
  }
  ~IOThreadContext() override = default;

  void BindConnector(
      service_manager::mojom::ConnectorRequest connector_request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // NOTE: It's not safe to modify |connector_request_| here since it's read
    // on the IO thread. Post a task instead. As long as this task is posted
    // before any code attempts to connect to the chrome service, there's no
    // race.
    base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::IO})
        ->PostTask(FROM_HERE,
                   base::BindOnce(&IOThreadContext::BindConnectorOnIOThread,
                                  base::Unretained(this),
                                  std::move(connector_request)));
  }

 private:
  void BindConnectorOnIOThread(
      service_manager::mojom::ConnectorRequest connector_request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    connector_request_ = std::move(connector_request);
  }

  // service_manager::Service:
  void OnStart() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    DCHECK(connector_request_.is_pending());
    context()->connector()->BindConnectorRequest(std::move(connector_request_));
  }

  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& name,
                       mojo::ScopedMessagePipeHandle handle) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::OverrideOnBindInterface(remote_info, name, &handle);
    if (!handle.is_valid())
      return;

    if (!registry_.TryBindInterface(name, &handle))
      registry_with_source_info_.TryBindInterface(name, &handle, remote_info);
  }

  service_manager::mojom::ConnectorRequest connector_request_;

  service_manager::BinderRegistry registry_;
  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_with_source_info_;

#if defined(OS_CHROMEOS)
  chromeos::Launchable launchable_;
#if defined(USE_OZONE)
  ws::InputDeviceController input_device_controller_;
#endif
#endif

  DISALLOW_COPY_AND_ASSIGN(IOThreadContext);
};

class ChromeService::ExtraParts : public ChromeBrowserMainExtraParts {
 public:
  ExtraParts() = default;
  ~ExtraParts() override = default;

 private:
  void ServiceManagerConnectionStarted(
      content::ServiceManagerConnection* connection) override {
    // Initializing the connector asynchronously configures the Connector on the
    // IO thread. This needs to be done before StartService() is called or
    // ChromeService::BindConnector() can race with ChromeService::OnStart().
    ChromeService::GetInstance()->InitConnector();

    connection->GetConnector()->StartService(
        service_manager::Identity(chrome::mojom::kServiceName));
  }

  DISALLOW_COPY_AND_ASSIGN(ExtraParts);
};

// static
ChromeService* ChromeService::GetInstance() {
  static base::NoDestructor<ChromeService> service;
  return service.get();
}

ChromeBrowserMainExtraParts* ChromeService::CreateExtraParts() {
  return new ExtraParts;
}

service_manager::EmbeddedServiceInfo::ServiceFactory
ChromeService::CreateChromeServiceFactory() {
  return base::BindRepeating(&ChromeService::CreateChromeServiceWrapper,
                             base::Unretained(this));
}

ChromeService::ChromeService()
    : io_thread_context_(std::make_unique<IOThreadContext>()) {}

ChromeService::~ChromeService() = default;

void ChromeService::InitConnector() {
  service_manager::mojom::ConnectorRequest request;
  connector_ = service_manager::Connector::Create(&request);
  io_thread_context_->BindConnector(std::move(request));
}

std::unique_ptr<service_manager::Service>
ChromeService::CreateChromeServiceWrapper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return std::make_unique<service_manager::ForwardingService>(
      io_thread_context_.get());
}
