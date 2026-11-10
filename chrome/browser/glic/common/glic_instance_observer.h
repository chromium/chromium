// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_GLIC_INSTANCE_OBSERVER_H_
#define CHROME_BROWSER_GLIC_COMMON_GLIC_INSTANCE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "components/actor/core/task_id.h"

namespace glic {

class GlicInstance;
class GlicInstanceImpl;

// Internal observer interface for GlicInstanceImpl lifecycle and conversation
// activity events. Scoped to //chrome/browser/glic:private_types so it is not
// exposed outside //chrome/browser/glic.
class GlicInstanceObserver : public base::CheckedObserver {
 public:
  virtual void OnInstanceVisibilityChanged(const GlicInstance& instance) {}
  virtual void OnInstanceConversationRegistered(const GlicInstance& instance) {}
  virtual void OnInstanceResponseStarted(const GlicInstance& instance) {}
  virtual void OnInstanceResponseStopped(const GlicInstance& instance,
                                         mojom::ResponseStopCause cause) {}
  virtual void OnInstanceWebClientCleared(const GlicInstance& instance) {}
  virtual void OnInstanceTabAddedToTask(const GlicInstance& instance,
                                        actor::TaskId task_id) {}
  virtual void OnInstanceDestroyed(GlicInstanceImpl* instance) {}
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_GLIC_INSTANCE_OBSERVER_H_
