// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_DELEGATE_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_DELEGATE_H_

namespace glic {

// Interface passed to a host on creation. Used methods that must interact
// with the GlicInstanceCoordinator.
class GlicInstanceDelegate {
 public:
  virtual ~GlicInstanceDelegate() = default;

  virtual void AttachInstance() = 0;
  virtual void DetachInstance() = 0;
  virtual void CloseInstanceAndShutdown() = 0;

  // Instead of going through GlicService Actor methods could be here.
  virtual void CreateTab() = 0;
  virtual void CreateTask() = 0;
  virtual void PerformActions() = 0;
  virtual void StopActorTask() = 0;
  virtual void PauseActorTask() = 0;
  virtual void ResumeActorTask() = 0;

  // Zero-State suggests need to be here since they are dependent on the context
  // for this instance.
  virtual void GetZeroStateSuggestionsAndSubscribe() = 0;
  // The definition of focused tab could be different per embedder.
  virtual void GetZeroStateSuggestionsForFocusedTab() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_DELEGATE_H_
