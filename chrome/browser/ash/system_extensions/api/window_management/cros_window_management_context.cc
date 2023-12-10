// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context.h"

#include <memory>

#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context_factory.h"
#include "chrome/browser/ash/system_extensions/api/window_management/window_management_impl.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"

namespace ash {

namespace {

bool IsTilingWindowManagerAccelerator(base::StringPiece accelerator_name) {
  // List of tiling window manager accelerator names.
  static const base::NoDestructor<base::flat_set<std::string>>
      kTilingWMaccelerators({
          // Move window
          "Alt Shift ArrowLeft",
          "Alt Shift ArrowRight",
          "Alt Shift ArrowUp",
          "Alt Shift ArrowDown",
          "Alt Shift KeyJ",
          "Alt Shift KeyK",
          "Alt Shift KeyL",
          "Alt Shift Semicolon",
          // Move focus
          "Alt ArrowLeft",
          "Alt ArrowRight",
          "Alt ArrowUp",
          "Alt ArrowDown",
          "Alt KeyJ",
          "Alt KeyK",
          "Alt KeyL",
          "Alt Semicolon",
          // Toggle fullscreen
          "Alt KeyF",
          // Split window vertically
          "Alt KeyV",
          // Split window horizontally
          "Alt KeyH",
          // Switch to workspace
          "Alt Digit0",
          "Alt Digit1",
          "Alt Digit2",
          "Alt Digit3",
          "Alt Digit4",
          "Alt Digit5",
          "Alt Digit6",
          "Alt Digit7",
          "Alt Digit8",
          "Alt Digit9",
          // Move to workspace
          "Alt Shift Digit0",
          "Alt Shift Digit1",
          "Alt Shift Digit2",
          "Alt Shift Digit3",
          "Alt Shift Digit4",
          "Alt Shift Digit5",
          "Alt Shift Digit6",
          "Alt Shift Digit7",
          "Alt Shift Digit8",
          "Alt Shift Digit9",
          // Close window
          "Alt Shift KeyQ",
      });
  return base::Contains(*kTilingWMaccelerators, accelerator_name);
}

}  // namespace

// static
CrosWindowManagementContext& CrosWindowManagementContext::Get(
    Profile* profile) {
  return *CrosWindowManagementContextFactory::GetForProfileIfExists(profile);
}

// static
void CrosWindowManagementContext::BindFactory(
    Profile* profile,
    const content::ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<blink::mojom::CrosWindowManagementFactory>
        pending_receiver) {
  // Profile could be shutting down.
  auto* dispatcher =
      CrosWindowManagementContextFactory::GetForProfileIfExists(profile);
  if (!dispatcher)
    return;

  dispatcher->factory_receivers_.Add(dispatcher, std::move(pending_receiver),
                                     info);
}

CrosWindowManagementContext::CrosWindowManagementContext(Profile* profile)
    : profile_(*profile),
      system_extensions_registry_(
          SystemExtensionsProvider::Get(profile).registry()),
      system_extensions_service_worker_manager_(
          SystemExtensionsProvider::Get(profile).service_worker_manager()) {
  cros_window_management_instances_.set_disconnect_handler(base::BindRepeating(
      &CrosWindowManagementContext::OnCrosWindowManagementDisconnect,
      // Safe because disconnect handlers aren't dispatched
      // after this class is destroyed.
      base::Unretained(this)));

  service_worker_manager_observation_.Observe(
      std::to_address(system_extensions_service_worker_manager_));

  instance_registry_observation_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile)
           ->InstanceRegistry());

  // Let CrosWindowManagementContext be a PreTargetHandler on aura::Env to
  // ensure that it receives KeyEvents.
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kAccessibility);
}

CrosWindowManagementContext::~CrosWindowManagementContext() {
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
}

void CrosWindowManagementContext::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(event->type() == ui::EventType::ET_KEY_PRESSED ||
         event->type() == ui::EventType::ET_KEY_RELEASED);
  // TODO(b/238578914): Eventually we will allow System Extensions to register
  // their accelerators. For prototyping, the accelerator name is a string
  // consisting of the modifiers pressed (Alt and/or Control) and the DOM key
  // that was pressed. We skip any events without modifiers. For example:
  // +----------------------+------------------------------------------------+
  // |    Keys pressed      |               Accelerator Name                 |
  // +----------------------+------------------------------------------------+
  // | `Ctrl + a`           | `"Control KeyA"`                               |
  // | `Ctrl + Alt + b`     | `"Control Alt KeyB"`                           |
  // | `Ctrl + Shift + a`   | `"Control Shift KeyA"`                         |
  // | `Ctrl + Alt + Shift` | Skipped (Only modifiers)                       |
  // | `Shift + a`          | Skipped (Neither Control nor Alt were pressed) |
  // +--------------------+--------------------------------------------------+
  // Ignore modifier-only accelerators.
  if (ui::KeycodeConverter::IsDomKeyForModifier(event->GetDomKey())) {
    return;
  }
  std::vector<std::string> keys;
  if (event->IsControlDown()) {
    keys.push_back(
        ui::KeycodeConverter::DomKeyToKeyString(ui::DomKey::CONTROL));
  }
  if (event->IsAltDown()) {
    keys.push_back(ui::KeycodeConverter::DomKeyToKeyString(ui::DomKey::ALT));
  }

  // We only support accelerators that use at least one of `Control` or `Alt`.
  if (keys.size() == 0) {
    return;
  }

  if (event->IsShiftDown()) {
    keys.push_back(ui::KeycodeConverter::DomKeyToKeyString(ui::DomKey::SHIFT));
  }

  keys.push_back(event->GetCodeString());

  blink::mojom::AcceleratorEventPtr event_ptr =
      blink::mojom::AcceleratorEvent::New();
  event_ptr->type = event->type() == ui::EventType::ET_KEY_PRESSED
                        ? blink::mojom::AcceleratorEvent::Type::kDown
                        : blink::mojom::AcceleratorEvent::Type::kUp;
  event_ptr->accelerator_name = base::JoinString(keys, " ");
  event_ptr->repeat = event->is_repeat();

  // If the accelerator is for the tiling window manager, then mark the event
  // as handled to avoid conflicts with other system shortcuts. Eventually,
  // System Extensions will integrate with the shortcut manager and won't need
  // this, but this improves the experience at the prototype stage.
  // TODO(b/238578914): Remove once System Extensions can register their
  // accelerators.
  if (!system_extensions_registry_->GetIds().empty() &&
      IsTilingWindowManagerAccelerator(event_ptr->accelerator_name)) {
    event->SetHandled();
  }

  GetCrosWindowManagementInstances(base::BindRepeating(
      [](const blink::mojom::AcceleratorEventPtr& event_ptr,
         WindowManagementImpl& window_management_impl) {
        window_management_impl.DispatchAcceleratorEvent(event_ptr.Clone());
      },
      std::move(event_ptr)));
}

void CrosWindowManagementContext::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  if (update.IsDestruction()) {
    GetCrosWindowManagementInstances(base::BindRepeating(
        [](base::UnguessableToken id,
           WindowManagementImpl& window_management_impl) {
          window_management_impl.DispatchWindowClosedEvent(id);
        },
        update.InstanceId()));
  } else if (update.IsCreation()) {
    GetCrosWindowManagementInstances(base::BindRepeating(
        [](base::UnguessableToken id,
           WindowManagementImpl& window_management_impl) {
          window_management_impl.DispatchWindowOpenedEvent(id);
        },
        update.InstanceId()));
  }
}

void CrosWindowManagementContext::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* instance_registry) {
  instance_registry_observation_.Reset();
}

void CrosWindowManagementContext::OnCrosWindowManagementDisconnect() {
  const SystemExtensionsServiceWorkerInfo& info =
      cros_window_management_instances_.current_context();

  bool info_removed = service_worker_info_to_impl_map_.erase(info);
  DCHECK(info_removed);

  // No need to remove it from cros_window_management_instances_ because
  // the ReceiverSet takes care of it.
}

void CrosWindowManagementContext::Create(
    mojo::PendingAssociatedReceiver<blink::mojom::CrosWindowManagement>
        pending_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::CrosWindowManagementObserver>
        observer_remote) {
  const content::ServiceWorkerVersionBaseInfo& service_worker_version_info =
      factory_receivers_.current_context();

  // The System Extension might have been uninstalled by the time its Service
  // Worker starts. Ignore the bind request in that case.
  const auto* system_extension =
      system_extensions_registry_->GetByUrl(service_worker_version_info.scope);
  if (!system_extension)
    return;

  SystemExtensionsServiceWorkerInfo service_worker_info{
      .system_extension_id = system_extension->id,
      .service_worker_version_id = service_worker_version_info.version_id,
      .service_worker_process_id = service_worker_version_info.process_id};

  auto cros_window_management = std::make_unique<WindowManagementImpl>(
      service_worker_info.service_worker_process_id,
      std::move(observer_remote));
  auto* cros_window_management_ptr = cros_window_management.get();

  cros_window_management_instances_.Add(std::move(cros_window_management),
                                        std::move(pending_receiver),
                                        service_worker_info);

  auto [_, inserted] = service_worker_info_to_impl_map_.emplace(
      service_worker_info, cros_window_management_ptr);
  DCHECK(inserted);

  RunPendingTasks(system_extension->id, *cros_window_management_ptr);
}

void CrosWindowManagementContext::OnRegisterServiceWorker(
    const SystemExtensionId& system_extension_id,
    blink::ServiceWorkerStatusCode status_code) {
  if (status_code != blink::ServiceWorkerStatusCode::kOk)
    return;

  GetCrosWindowManagement(
      system_extension_id,
      base::BindOnce([](WindowManagementImpl& cros_window_management) {
        cros_window_management.DispatchStartEvent();
      }));
}

void CrosWindowManagementContext::GetCrosWindowManagementInstances(
    base::RepeatingCallback<void(WindowManagementImpl&)> callback) {
  const std::vector<SystemExtensionId> ids =
      system_extensions_registry_->GetIds();
  for (const auto& id : ids) {
    auto* system_extension = system_extensions_registry_->GetById(id);
    DCHECK(system_extension);

    if (system_extension->type != SystemExtensionType::kWindowManagement)
      continue;

    GetCrosWindowManagement(system_extension->id, callback);
  }
}

void CrosWindowManagementContext::GetCrosWindowManagement(
    const SystemExtensionId& system_extension_id,
    base::OnceCallback<void(WindowManagementImpl&)> callback) {
  auto& pending_callbacks = id_to_pending_callbacks_[system_extension_id];

  const bool need_to_start_worker = pending_callbacks.empty();
  pending_callbacks.push_back(std::move(callback));

  if (!need_to_start_worker)
    return;

  system_extensions_service_worker_manager_->StartServiceWorker(
      system_extension_id,
      base::BindOnce(&CrosWindowManagementContext::OnServiceWorkerStarted,
                     weak_ptr_factory_.GetWeakPtr(), system_extension_id));
}

void CrosWindowManagementContext::OnServiceWorkerStarted(
    const SystemExtensionId& system_extension_id,
    StatusOrSystemExtensionsServiceWorkerInfo status_or_info) {
  if (!status_or_info.ok()) {
    id_to_pending_callbacks_.erase(system_extension_id);
    return;
  }

  auto info_and_impl_it =
      service_worker_info_to_impl_map_.find(status_or_info.value());
  if (info_and_impl_it == service_worker_info_to_impl_map_.end())
    return;

  RunPendingTasks(system_extension_id, *info_and_impl_it->second);
}

void CrosWindowManagementContext::RunPendingTasks(
    const SystemExtensionId& system_extension_id,
    WindowManagementImpl& window_management_impl) {
  std::vector<base::OnceCallback<void(WindowManagementImpl&)>> callbacks;

  std::swap(callbacks, id_to_pending_callbacks_[system_extension_id]);
  for (auto& callback : callbacks) {
    std::move(callback).Run(window_management_impl);
  }
}

}  // namespace ash
