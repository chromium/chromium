// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_BRIDGE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_BRIDGE_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace contextual_tasks {

// Profile-keyed service that manages the Contextual Tasks component extension's
// integration with the browser, specifically providing loadTimeData.
// There is one instance of this service per Profile.
class ContextualTasksExtensionBridge : public KeyedService {
 public:
  explicit ContextualTasksExtensionBridge(Profile* profile);
  ContextualTasksExtensionBridge(const ContextualTasksExtensionBridge&) =
      delete;
  ContextualTasksExtensionBridge& operator=(
      const ContextualTasksExtensionBridge&) = delete;
  ~ContextualTasksExtensionBridge() override;

  static ContextualTasksExtensionBridge* Get(Profile* profile);

 private:
  base::DictValue GetLoadTimeData();

  const raw_ref<Profile> profile_;

  // Subscription for the template data provider.
  base::ScopedClosureRunner load_time_data_subscription_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_BRIDGE_H_
