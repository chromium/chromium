// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_update_service.h"

#include <memory>
#include <string>
#include <vector>

#include "android_webview/common/aw_paths.h"
#include "android_webview/nonembedded/component_updater/aw_component_updater_configurator.h"
#include "android_webview/nonembedded/component_updater/registration.h"
#include "android_webview/nonembedded/webview_apk_process.h"
#include "base/android/callback_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/component_updater_utils.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/nonembedded/nonembedded_jni_headers/AwComponentUpdateService_jni.h"

namespace android_webview {

// static
AwComponentUpdateService* AwComponentUpdateService::GetInstance() {
  static base::NoDestructor<AwComponentUpdateService> instance;
  return instance.get();
}

// static
void JNI_AwComponentUpdateService_StartComponentUpdateService(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_finished_callback,
    jboolean j_on_demand_update) {
  AwComponentUpdateService::GetInstance()->StartComponentUpdateService(
      base::BindOnce(
          &base::android::RunIntCallbackAndroid,
          base::android::ScopedJavaGlobalRef<jobject>(j_finished_callback)),
      j_on_demand_update);
}

AwComponentUpdateService::AwComponentUpdateService()
    : AwComponentUpdateService(MakeAwComponentUpdaterConfigurator(
          base::CommandLine::ForCurrentProcess(),
          WebViewApkProcess::GetInstance()->GetPrefService())) {}

AwComponentUpdateService::AwComponentUpdateService(
    scoped_refptr<update_client::Configurator> configurator)
    : update_client_(update_client::UpdateClientFactory(configurator)) {}

AwComponentUpdateService::~AwComponentUpdateService() = default;

// Start ComponentUpdateService once.
void AwComponentUpdateService::StartComponentUpdateService(
    UpdateCallback finished_callback,
    bool on_demand_update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RegisterComponents(
      base::BindRepeating(&AwComponentUpdateService::RegisterComponent,
                          base::Unretained(this)),
      base::BindOnce(
          &AwComponentUpdateService::ScheduleUpdatesOfRegisteredComponents,
          weak_ptr_factory_.GetWeakPtr(), std::move(finished_callback),
          on_demand_update));
}

bool AwComponentUpdateService::RegisterComponent(
    const component_updater::ComponentRegistration& component) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40750393): Add the histograms being logged in
  // CrxUpdateService once we support logging metrics from nonembedded WebView.

  if (component.app_id.empty() || !component.version.IsValid() ||
      !component.installer) {
    return false;
  }

  // Update the registration data if the component has been registered before.
  auto it = components_.find(component.app_id);
  if (it != components_.end()) {
    it->second = component;
    return true;
  }

  components_.insert(std::make_pair(component.app_id, component));
  components_order_.push_back(component.app_id);
  return true;
}

void AwComponentUpdateService::CheckForUpdates(UpdateCallback on_finished,
                                               bool on_demand_update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40750393): Add the histograms being logged in
  // CrxUpdateService once we support logging metrics from nonembedded WebView.

  std::vector<std::string> secure_ids;    // Require HTTPS for update checks.
  std::vector<std::string> unsecure_ids;  // Can fallback to HTTP.
  for (const auto& id : components_order_) {
    DCHECK(base::Contains(components_, id));

    const auto component = component_updater::GetComponent(components_, id);
    if (!component || component->requires_network_encryption)
      secure_ids.push_back(id);
    else
      unsecure_ids.push_back(id);
  }

  if (unsecure_ids.empty() && secure_ids.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_finished), 0));
    return;
  }

  auto on_finished_callback =
      base::BindOnce(&AwComponentUpdateService::RecordComponentsUpdated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_finished));

  // Reset updated components counter.
  components_updated_count_ = 0;

  if (!unsecure_ids.empty()) {
    update_client_->Update(
        unsecure_ids,
        base::BindOnce(&AwComponentUpdateService::GetCrxComponents,
                       base::Unretained(this)),
        {}, on_demand_update,
        base::BindOnce(&AwComponentUpdateService::OnUpdateComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       secure_ids.empty() ? std::move(on_finished_callback)
                                          : update_client::Callback(),
                       base::TimeTicks::Now()));
  }

  if (!secure_ids.empty()) {
    update_client_->Update(
        secure_ids,
        base::BindOnce(&AwComponentUpdateService::GetCrxComponents,
                       base::Unretained(this)),
        {}, on_demand_update,
        base::BindOnce(&AwComponentUpdateService::OnUpdateComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(on_finished_callback),
                       base::TimeTicks::Now()));
  }

  return;
}

void AwComponentUpdateService::OnUpdateComplete(
    update_client::Callback callback,
    const base::TimeTicks& start_time,
    update_client::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40750393): Add the histograms being logged in
  // CrxUpdateService once we support logging metrics from nonembedded WebView.

  if (!callback.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), error));
  }
}

update_client::CrxComponent AwComponentUpdateService::ToCrxComponent(
    const component_updater::ComponentRegistration& component) const {
  update_client::CrxComponent crx;
  crx.pk_hash = component.public_key_hash;
  crx.app_id = component.app_id;
  crx.installer = component.installer;
  crx.action_handler = component.action_handler;
  crx.version = component.version;
  crx.fingerprint = component.fingerprint;
  crx.name = component.name;
  crx.installer_attributes = component.installer_attributes;
  crx.requires_network_encryption = component.requires_network_encryption;

  crx.crx_format_requirement =
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF;

  return crx;
}

std::optional<component_updater::ComponentRegistration>
AwComponentUpdateService::GetComponent(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return component_updater::GetComponent(components_, id);
}

void AwComponentUpdateService::GetCrxComponents(
    const std::vector<std::string>& ids,
    base::OnceCallback<
        void(const std::vector<std::optional<update_client::CrxComponent>>&)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::optional<update_client::CrxComponent>> crxs;
  for (std::optional<component_updater::ComponentRegistration> item :
       component_updater::GetCrxComponents(components_, ids)) {
    crxs.push_back(
        item ? std::optional<update_client::CrxComponent>{ToCrxComponent(*item)}
             : std::nullopt);
  }
  std::move(callback).Run(crxs);
}

void AwComponentUpdateService::ScheduleUpdatesOfRegisteredComponents(
    UpdateCallback on_finished_updates,
    bool on_demand_update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CheckForUpdates(std::move(on_finished_updates), on_demand_update);
}

void AwComponentUpdateService::RegisterComponents(
    RegisterComponentsCallback register_callback,
    base::OnceClosure on_finished) {
  RegisterComponentsForUpdate(register_callback, std::move(on_finished));
}

void AwComponentUpdateService::IncrementComponentsUpdatedCount() {
  components_updated_count_++;
}

void AwComponentUpdateService::RecordComponentsUpdated(
    UpdateCallback on_finished,
    update_client::Error error) {
  std::move(on_finished).Run(components_updated_count_);
}

}  // namespace android_webview
