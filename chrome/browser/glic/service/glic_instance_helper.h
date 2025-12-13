// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_HELPER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_HELPER_H_

#include <optional>

#include "base/callback_list.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/service/metrics/glic_instance_helper_metrics.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace glic {

// Attaches a InstanceId to a TabInterface. An instance of this class is
// created by and owned by TabFeatures.
class GlicInstanceHelper {
 public:
  DECLARE_USER_DATA(GlicInstanceHelper);

  static GlicInstanceHelper* From(tabs::TabInterface* tab);

  explicit GlicInstanceHelper(tabs::TabInterface* tab);
  ~GlicInstanceHelper();

  std::optional<InstanceId> GetInstanceId() const { return instance_id_; }
  void SetInstanceId(const InstanceId& instance_id);

  void OnPinnedByInstance(const InstanceId& instance_id);

  void SetIsDaisyChained();
  void OnDaisyChainAction(DaisyChainFirstAction action);

  base::CallbackListSubscription SubscribeToDestruction(
      base::RepeatingCallback<void(tabs::TabInterface*, const InstanceId&)>
          callback);

 private:
  std::optional<InstanceId> instance_id_;
  GlicInstanceHelperMetrics metrics_;
  raw_ptr<tabs::TabInterface> tab_;
  ui::ScopedUnownedUserData<GlicInstanceHelper> scoped_unowned_user_data_;
  base::RepeatingCallbackList<void(tabs::TabInterface*, const InstanceId&)>
      on_destroy_callback_list_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_HELPER_H_
