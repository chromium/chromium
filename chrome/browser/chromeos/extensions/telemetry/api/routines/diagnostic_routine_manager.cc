// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_manager.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/containers/flat_tree.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/app_ui_observer.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/util.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/remote_diagnostic_routines_service_strategy.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

}  // namespace

// static
extensions::BrowserContextKeyedAPIFactory<DiagnosticRoutineManager>*
DiagnosticRoutineManager::GetFactoryInstance() {
  static base::NoDestructor<
      extensions::BrowserContextKeyedAPIFactory<DiagnosticRoutineManager>>
      instance;
  return instance.get();
}

// static
DiagnosticRoutineManager* DiagnosticRoutineManager::Get(
    content::BrowserContext* browser_context) {
  return extensions::BrowserContextKeyedAPIFactory<
      DiagnosticRoutineManager>::Get(browser_context);
}

DiagnosticRoutineManager::DiagnosticRoutineManager(
    content::BrowserContext* context)
    : browser_context_(context) {
  extensions::ExtensionRegistry::Get(context)->AddObserver(this);
}

DiagnosticRoutineManager::~DiagnosticRoutineManager() = default;

base::expected<base::Uuid, DiagnosticRoutineManager::Error>
DiagnosticRoutineManager::CreateRoutine(
    extensions::ExtensionId extension_id,
    crosapi::TelemetryDiagnosticRoutineArgumentPtr routine_argument) {
  if (app_ui_observers_.find(extension_id) == app_ui_observers_.end()) {
    auto observer = CreateAppUiObserver(extension_id);
    if (!observer.has_value()) {
      return base::unexpected(observer.error());
    }

    app_ui_observers_.emplace(extension_id, std::move(observer.value()));
  }

  mojo::PendingRemote<crosapi::TelemetryDiagnosticRoutineControl>
      control_remote;
  mojo::PendingReceiver<crosapi::TelemetryDiagnosticRoutineObserver>
      observer_receiver;

  GetRemoteService()->CreateRoutine(
      std::move(routine_argument),
      control_remote.InitWithNewPipeAndPassReceiver(),
      observer_receiver.InitWithNewPipeAndPassRemote());

  auto uuid = base::Uuid::GenerateRandomV4();
  DiagnosticRoutine::RoutineInfo routine_info(extension_id, uuid,
                                              browser_context_);

  auto it = routines_per_extension_.find(extension_id);
  if (it == routines_per_extension_.end()) {
    std::tie(it, std::ignore) = routines_per_extension_.emplace(
        std::piecewise_construct, std::forward_as_tuple(extension_id),
        std::forward_as_tuple());
  }
  // SAFETY: We can use `Unretained` here since `DiagnosticRoutine` is a member
  // of `this`.
  it->second.push_back(std::make_unique<DiagnosticRoutine>(
      std::move(control_remote), std::move(observer_receiver), routine_info,
      base::BindOnce(&DiagnosticRoutineManager::OnDiagnosticRoutineFinished,
                     base::Unretained(this))));

  return base::ok(uuid);
}

void DiagnosticRoutineManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  routines_per_extension_.erase(extension->id());
  app_ui_observers_.erase(extension->id());
}

mojo::Remote<crosapi::TelemetryDiagnosticRoutinesService>&
DiagnosticRoutineManager::GetRemoteService() {
  if (!remote_strategy_) {
    remote_strategy_ = RemoteDiagnosticRoutineServiceStrategy::Create();
  }
  return remote_strategy_->GetRemoteService();
}

void DiagnosticRoutineManager::OnAppUiClosed(
    extensions::ExtensionId extension_id) {
  // Try to find another open UI.
  auto observer = CreateAppUiObserver(extension_id);
  if (observer.has_value()) {
    app_ui_observers_.insert_or_assign(extension_id,
                                       std::move(observer.value()));
    return;
  }

  app_ui_observers_.erase(extension_id);
  routines_per_extension_.erase(extension_id);
}

base::expected<std::unique_ptr<AppUiObserver>, DiagnosticRoutineManager::Error>
DiagnosticRoutineManager::CreateAppUiObserver(
    extensions::ExtensionId extension_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context_)
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::EVERYTHING);
  if (!extension) {
    // If the extension has been unloaded from the registry, there
    // won't be any related app UI.
    return base::unexpected(kExtensionUnloaded);
  }
  content::WebContents* contents =
      FindTelemetryExtensionOpenAndSecureAppUi(browser_context_, extension);
  if (!contents) {
    return base::unexpected(kAppUiClosed);
  }

  return std::make_unique<AppUiObserver>(
      contents,
      extensions::ExternallyConnectableInfo::Get(extension)->matches.Clone(),
      // Unretained is safe here because `this` will own the observer.
      base::BindOnce(&DiagnosticRoutineManager::OnAppUiClosed,
                     base::Unretained(this), extension_id),
      base::NullCallback());
}

void DiagnosticRoutineManager::OnDiagnosticRoutineFinished(
    DiagnosticRoutine* routine) {
  for (auto& [_, routines] : routines_per_extension_) {
    base::EraseIf(routines,
                  [routine](const std::unique_ptr<DiagnosticRoutine>& ptr) {
                    return ptr.get() == routine;
                  });
  }
}

}  // namespace chromeos
