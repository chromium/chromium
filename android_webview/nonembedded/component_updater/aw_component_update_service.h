// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_UPDATE_SERVICE_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_UPDATE_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace base {
class TimeTicks;
}

namespace component_updater {
struct ComponentRegistration;
}

namespace android_webview {
using RegisterComponentsCallback = base::RepeatingCallback<bool(
    const component_updater::ComponentRegistration&)>;

class TestAwComponentUpdateService;

// Native-side implementation of the AwComponentUpdateService. It
// registers components and installs any updates for registered components.
class AwComponentUpdateService {
 public:
  static AwComponentUpdateService* GetInstance();

  // Callback used for updating components, with an int32_t that represents how
  // many components were actually updated.
  using UpdateCallback = base::OnceCallback<void(int32_t)>;

  void StartComponentUpdateService(UpdateCallback finished_callback,
                                   bool on_demand_update);
  bool RegisterComponent(
      const component_updater::ComponentRegistration& component);
  void CheckForUpdates(UpdateCallback on_finished, bool on_demand_update);

  void IncrementComponentsUpdatedCount();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  friend base::NoDestructor<AwComponentUpdateService>;
  friend TestAwComponentUpdateService;

  FRIEND_TEST_ALL_PREFIXES(AwComponentUpdateServiceTest,
                           TestComponentReadyWhenOffline);

  // Accept custom configurator for testing.
  explicit AwComponentUpdateService(
      scoped_refptr<update_client::Configurator> configurator);
  AwComponentUpdateService();

  // Virtual for testing.
  virtual ~AwComponentUpdateService();

  void OnUpdateComplete(update_client::Callback callback,
                        const base::TimeTicks& start_time,
                        update_client::Error error);
  update_client::CrxComponent ToCrxComponent(
      const component_updater::ComponentRegistration& component) const;
  std::optional<component_updater::ComponentRegistration> GetComponent(
      const std::string& id) const;
  void GetCrxComponents(
      const std::vector<std::string>& ids,
      base::OnceCallback<
          void(const std::vector<std::optional<update_client::CrxComponent>>&)>
          callback);
  void ScheduleUpdatesOfRegisteredComponents(UpdateCallback on_finished_updates,
                                             bool on_demand_update);

  // Virtual for testing.
  virtual void RegisterComponents(RegisterComponentsCallback register_callback,
                                  base::OnceClosure on_finished);

  scoped_refptr<update_client::UpdateClient> update_client_;

  // A collection of every registered component.
  base::flat_map<std::string, component_updater::ComponentRegistration>
      components_;

  // Maintains the order in which components have been registered. The
  // position of a component id in this sequence indicates the priority of the
  // component. The sooner the component gets registered, the higher its
  // priority, and the closer this component is to the beginning of the
  // vector.
  std::vector<std::string> components_order_;

  void RecordComponentsUpdated(UpdateCallback on_finished,
                               update_client::Error error);

  // Counts how many components were updated, for UMA logging.
  int32_t components_updated_count_ = 0;

  base::WeakPtrFactory<AwComponentUpdateService> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_UPDATE_SERVICE_H_
