// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_FLOW_DELEGATE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_FLOW_DELEGATE_H_

#include "base/memory/weak_ptr.h"

namespace actor {

// Decides which browser UI a task shows. Shows none by default.
class ActorFlowDelegate {
 public:
  ActorFlowDelegate();
  ActorFlowDelegate(const ActorFlowDelegate&) = delete;
  ActorFlowDelegate& operator=(const ActorFlowDelegate&) = delete;
  virtual ~ActorFlowDelegate();

  // Whether browser UI tied to the task is shown. Must return the same value
  // for the lifetime of a task.
  virtual bool ShouldShowBrowserUi() const;

  // Returns a weak pointer to this delegate.
  base::WeakPtr<ActorFlowDelegate> GetWeakPtr();

 private:
  base::WeakPtrFactory<ActorFlowDelegate> weak_ptr_factory_{this};
};

// Shows all browser UI for a task.
class BrowserUiActorFlowDelegate : public ActorFlowDelegate {
 public:
  BrowserUiActorFlowDelegate();
  ~BrowserUiActorFlowDelegate() override;

  // ActorFlowDelegate:
  bool ShouldShowBrowserUi() const override;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_FLOW_DELEGATE_H_
