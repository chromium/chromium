// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/shimless_rma/diagnostics_app_profile_helper.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/shimless_rma/diagnostics_app_profile_helper_constants.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/service_worker_context.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/verifier_formats.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace context {
class BrowserContext;
};

namespace ash::shimless_rma {

namespace {

// Polling interval and the timeout to wait for the extension being ready.
constexpr base::TimeDelta kExtensionReadyPollingInterval =
    base::Milliseconds(50);
constexpr base::TimeDelta kExtensionReadyPollingTimeout = base::Seconds(3);

extensions::ExtensionService* GetExtensionService(
    content::BrowserContext* context) {
  CHECK(context);
  auto* system = extensions::ExtensionSystem::Get(context);
  CHECK(system);
  auto* service = system->extension_service();
  CHECK(service);
  return service;
}

void DisableAllExtensions(content::BrowserContext* context) {
  auto* registry = extensions::ExtensionRegistry::Get(context);
  CHECK(registry);
  auto* service = GetExtensionService(context);

  std::vector<std::string> ids;
  for (const auto& extension : registry->enabled_extensions()) {
    ids.push_back(extension->id());
  }
  for (const auto& extension : registry->terminated_extensions()) {
    ids.push_back(extension->id());
  }

  for (const auto& id : ids) {
    service->DisableExtension(id,
                              extensions::disable_reason::DISABLE_USER_ACTION);
  }
}

struct PrepareDiagnosticsAppProfileState {
  PrepareDiagnosticsAppProfileState();
  PrepareDiagnosticsAppProfileState(PrepareDiagnosticsAppProfileState&) =
      delete;
  PrepareDiagnosticsAppProfileState& operator=(
      PrepareDiagnosticsAppProfileState&) = delete;
  ~PrepareDiagnosticsAppProfileState();

  // Arguments
  raw_ptr<DiagnosticsAppProfileHelperDelegate> delegate;
  base::FilePath crx_path;
  base::FilePath swbn_path;
  ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextCallback callback;
  // Reference to the crx_installer.
  scoped_refptr<extensions::CrxInstaller> crx_installer = nullptr;
  // Result.
  raw_ptr<content::BrowserContext> context;
  absl::optional<std::string> extension_id;
  absl::optional<web_package::SignedWebBundleId> iwa_id;
  absl::optional<std::string> name;
  absl::optional<std::string> permission_message;
};

PrepareDiagnosticsAppProfileState::PrepareDiagnosticsAppProfileState() =
    default;

PrepareDiagnosticsAppProfileState::~PrepareDiagnosticsAppProfileState() =
    default;

void ReportError(std::unique_ptr<PrepareDiagnosticsAppProfileState> state,
                 const std::string& message) {
  std::move(state->callback).Run(base::unexpected(message));
}

void ReportSuccess(std::unique_ptr<PrepareDiagnosticsAppProfileState> state) {
  CHECK(state->context);
  CHECK(state->extension_id);
  CHECK(state->iwa_id);

  std::move(state->callback)
      .Run(base::ok(
          ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult(
              state->context, state->extension_id.value(),
              state->iwa_id.value(), state->name.value(),
              state->permission_message)));
}

void OnIsolatedWebAppInstalled(
    std::unique_ptr<PrepareDiagnosticsAppProfileState> state,
    base::expected<web_app::InstallIsolatedWebAppCommandSuccess,
                   web_app::InstallIsolatedWebAppCommandError> result) {
  CHECK(state->context);
  CHECK(state->extension_id);
  CHECK(state->iwa_id);

  if (!result.has_value()) {
    ReportError(std::move(state), "Failed to install Isolated web app: " +
                                      result.error().message);
    return;
  }

  const web_app::WebApp* web_app = state->delegate->GetWebAppById(
      web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          *state->iwa_id)
          .app_id(),
      state->context);
  // TODO(b/294815884): Check this when installing the IWA after we can add
  // custom checker. For now, we just install the IWA. Because we won't return
  // the profile and won't launch the IWA it should be fine.
  if (!web_app->permissions_policy().empty()) {
    ReportError(std::move(state), k3pDiagErrorIWACannotHasPermissionPolicy);
    return;
  }
  state->name = web_app->untranslated_name();

  ReportSuccess(std::move(state));
}

void InstallIsolatedWebApp(
    std::unique_ptr<PrepareDiagnosticsAppProfileState> state) {
  CHECK(state->context);
  CHECK(state->extension_id);

  auto info =
      chromeos::GetChromeOSExtensionInfoById(state->extension_id.value());
  if (!info.iwa_id) {
    ReportError(std::move(state), "Extension " + state->extension_id.value() +
                                      " doesn't have a connected IWA.");
    return;
  }
  state->iwa_id = info.iwa_id;

  auto url_info = web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      state->iwa_id.value());
  web_app::IsolatedWebAppLocation location =
      web_app::InstalledBundle{.path = state->swbn_path};
  state->delegate->GetWebAppCommandScheduler(state->context)
      ->InstallIsolatedWebApp(
          url_info, location,
          /*expected_version=*/absl::nullopt, /*optional_keep_alive=*/nullptr,
          /*optional_profile_keep_alive=*/nullptr,
          base::BindOnce(&OnIsolatedWebAppInstalled, std::move(state)));
}

void CheckExtensionIsReady(
    std::unique_ptr<PrepareDiagnosticsAppProfileState> state,
    GURL script_url,
    blink::StorageKey storage_key,
    base::Time start_time);

void OnCheckExtensionIsReadyResponse(
    std::unique_ptr<PrepareDiagnosticsAppProfileState> state,
    GURL script_url,
    blink::StorageKey storage_key,
    base::Time start_time,
    content::ServiceWorkerCapability capability) {
  if (capability == content::ServiceWorkerCapability::NO_SERVICE_WORKER) {
    // The service worker could still be registering, or the extension is failed
    // to be activated.
    if (base::Time::Now() - start_time <= kExtensionReadyPollingTimeout) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&CheckExtensionIsReady, std::move(state), script_url,
                         storage_key, start_time),
          kExtensionReadyPollingInterval);
      return;
    }
    ReportError(std::move(state), k3pDiagErrorCannotActivateExtension);
    return;
  }

  InstallIsolatedWebApp(std::move(state));
}

void CheckExtensionIsReady(
    std::unique_ptr<PrepareDiagnosticsAppProfileState> state,
    GURL script_url,
    blink::StorageKey storage_key,
    base::Time start_time) {
  content::ServiceWorkerContext* service_worker_context =
      state->delegate->GetServiceWorkerContextForExtensionId(
          state->extension_id.value(), state->context);
  // Extensions register a service worker. Diagnostics app IWAs need to be
  // started after the service worker is up to communicate with the extensions.
  service_worker_context->CheckHasServiceWorker(
      script_url, storage_key,
      base::BindOnce(&OnCheckExtensionIsReadyResponse, std::move(state),
                     script_url, storage_key, start_time));
}

void OnExtensionInstalled(
    std::unique_ptr<PrepareDiagnosticsAppProfileState> state,
    const absl::optional<extensions::CrxInstallError>& error) {
  CHECK(state->context);
  CHECK(state->crx_installer);

  if (error) {
    ReportError(std::move(state),
                "Failed to install 3p diagnostics extension: " +
                    base::UTF16ToUTF8(error->message()));
    return;
  }
  const extensions::Extension* extension = state->crx_installer->extension();
  CHECK(extension);
  state->extension_id = extension->id();
  state->crx_installer.reset();

  if (!chromeos::IsChromeOSSystemExtension(extension->id())) {
    ReportError(std::move(state),
                base::StringPrintf(k3pDiagErrorNotChromeOSSystemExtension,
                                   extension->id().c_str()));
    return;
  }

  if (!extension->install_warnings().empty()) {
    LOG(ERROR)
        << "Extension " << extension->id()
        << " may not work as expected because of these install warnings:";
    for (const auto& warning : extension->install_warnings()) {
      LOG(ERROR) << warning.message;
    }
  }

  extensions::PermissionMessages permission_messages =
      extension->permissions_data()->GetPermissionMessages();
  if (permission_messages.empty()) {
    state->permission_message = absl::nullopt;
  } else {
    std::u16string message;
    for (const auto& permission_message : permission_messages) {
      base::StrAppend(&message, {permission_message.message(), u"\n"});
      for (const auto& submessage : permission_message.submessages()) {
        base::StrAppend(&message, {u"- ", submessage, u"\n"});
      }
    }
    state->permission_message = base::UTF16ToUTF8(message);
  }

  GetExtensionService(state->context)->EnableExtension(extension->id());

  GURL script_url = extension->GetResourceURL(
      extensions::BackgroundInfo::GetBackgroundServiceWorkerScript(extension));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CheckExtensionIsReady, std::move(state), script_url,
                     blink::StorageKey::CreateFirstParty(
                         url::Origin::Create(extension->url())),
                     base::Time::Now()),
      kExtensionReadyPollingInterval);
}

void InstallExtension(
    std::unique_ptr<PrepareDiagnosticsAppProfileState> state) {
  CHECK(state->context);

  auto crx_installer = extensions::CrxInstaller::CreateSilent(
      GetExtensionService(state->context));
  state->crx_installer = crx_installer;
  const base::FilePath& crx_path = state->crx_path;
  crx_installer->AddInstallerCallback(
      base::BindOnce(&OnExtensionInstalled, std::move(state)));
  const crx_file::VerifierFormat verifier_format =
      ash::features::IsShimlessRMA3pDiagnosticsDevModeEnabled()
          ? extensions::GetTestVerifierFormat()
          : extensions::GetWebstoreVerifierFormat(
                /*test_publisher_enabled=*/false);
  crx_installer->InstallCrxFile(
      extensions::CRXFileInfo{crx_path, verifier_format});
}

void OnExtensionSystemReady(
    std::unique_ptr<PrepareDiagnosticsAppProfileState> state) {
  CHECK(state->context);

  DisableAllExtensions(state->context);
  InstallExtension(std::move(state));
}

void OnProfileLoaded(std::unique_ptr<PrepareDiagnosticsAppProfileState> state,
                     Profile* profile) {
  if (!profile) {
    ReportError(std::move(state),
                "Failed to load shimless diagnostics app profile.");
    return;
  }
  // Extensions and IWAs should be installed to the original profile.
  if (profile->IsOffTheRecord()) {
    profile = profile->GetOriginalProfile();
  }
  profile->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles, true);

  state->context = profile;
  auto* system = extensions::ExtensionSystem::Get(state->context);
  CHECK(system);
  system->ready().Post(
      FROM_HERE, base::BindOnce(&OnExtensionSystemReady, std::move(state)));
}

void PrepareDiagnosticsAppProfileImpl(
    std::unique_ptr<PrepareDiagnosticsAppProfileState> state) {
  CHECK(g_browser_process);
  CHECK(g_browser_process->profile_manager());
  CHECK(BrowserContextHelper::Get());
  // TODO(b/292227137): Use ScopedProfileKeepAlive before migrate this to
  // LaCrOS.
  g_browser_process->profile_manager()->CreateProfileAsync(
      BrowserContextHelper::Get()->GetShimlessRmaAppBrowserContextPath(),
      base::BindOnce(&OnProfileLoaded, std::move(state)));
}

}  // namespace

DiagnosticsAppProfileHelperDelegate::DiagnosticsAppProfileHelperDelegate() =
    default;

DiagnosticsAppProfileHelperDelegate::~DiagnosticsAppProfileHelperDelegate() =
    default;

content::ServiceWorkerContext*
DiagnosticsAppProfileHelperDelegate::GetServiceWorkerContextForExtensionId(
    const extensions::ExtensionId& extension_id,
    content::BrowserContext* browser_context) {
  return extensions::util::GetServiceWorkerContextForExtensionId(
      extension_id, browser_context);
}

web_app::WebAppCommandScheduler*
DiagnosticsAppProfileHelperDelegate::GetWebAppCommandScheduler(
    content::BrowserContext* browser_context) {
  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(browser_context));
  CHECK(web_app_provider);
  return &web_app_provider->scheduler();
}

const web_app::WebApp* DiagnosticsAppProfileHelperDelegate::GetWebAppById(
    const webapps::AppId& app_id,
    content::BrowserContext* browser_context) {
  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(browser_context));
  const web_app::WebAppRegistrar& registrar =
      web_app_provider->registrar_unsafe();
  return registrar.GetAppById(app_id);
}

void PrepareDiagnosticsAppProfile(
    DiagnosticsAppProfileHelperDelegate* delegate,
    const base::FilePath& crx_path,
    const base::FilePath& swbn_path,
    ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextCallback callback) {
  CHECK(::ash::features::IsShimlessRMA3pDiagnosticsEnabled());
  auto state = std::make_unique<PrepareDiagnosticsAppProfileState>();
  state->delegate = delegate;
  state->crx_path = crx_path;
  state->swbn_path = swbn_path;
  state->callback = std::move(callback);
  PrepareDiagnosticsAppProfileImpl(std::move(state));
}

}  // namespace ash::shimless_rma
