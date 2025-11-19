// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_HELPER_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_HELPER_METRICS_H_

#include "base/containers/flat_set.h"
#include "chrome/browser/glic/public/glic_instance.h"

namespace glic {

class GlicInstanceHelperMetrics {
 public:
  GlicInstanceHelperMetrics();
  ~GlicInstanceHelperMetrics();

  void OnBoundToInstance(const InstanceId& instance_id);
  void OnPinnedByInstance(const InstanceId& instance_id);

 private:
  base::flat_set<InstanceId> bound_instances_;
  base::flat_set<InstanceId> pinned_by_instances_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_HELPER_METRICS_H_
