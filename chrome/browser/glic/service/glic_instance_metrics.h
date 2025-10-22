// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_METRICS_H_

#include <optional>
#include <string>

#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

struct ShowOptions;

// Tracks and logs lifecycle events for a single GlicInstance.
class GlicInstanceMetrics {
 public:
  GlicInstanceMetrics();
  ~GlicInstanceMetrics();

  GlicInstanceMetrics(const GlicInstanceMetrics&) = delete;
  GlicInstanceMetrics& operator=(const GlicInstanceMetrics&) = delete;

  // Called when GlicInstanceImpl is created.
  void OnInstanceCreated();

  // Called when GlicInstanceImpl is destroyed.
  void OnInstanceDestroyed();

  // Called when a GlicInstance is bound to a tab.
  void OnBind();

  // Called when a new warmed GlicInstance is created.
  void OnWarmedInstanceCreated();

  // Called when a warmed instance is promoted to a live instance.
  void OnWarmedInstancePromoted();

  // Called when an instance is created without warming.
  void OnInstanceCreatedWithoutWarming();

  // Called when GlicInstanceImpl::Show is called for a side panel.
  void OnShowSidePanel();

  // Called when GlicInstanceImpl::Show is called for a side panel due to hotkey
  // use.
  void OnShowSidePanelViaHotkey();

  // Called when the floaty is shown via a hotkey due to hotkey use.
  void OnFloatyShownViaHotkey();

  // Called when GlicInstanceImpl::Show is called for a floaty.
  void OnShowFloaty();

  // Called when GlicInstanceImpl::SwitchConversation is called from this
  // instance (usually via 'start new chat' or re etn chats selection).
  void OnSwitchFromConversation(const ShowOptions& show_options);

  // Called when GlicInstanceImpl::SwitchConversation is called to activate this
  // instance (usually via 'start new chat' or recent chats selection).
  void OnSwitchToConversation(const ShowOptions& show_options);

  // Called when GlicInstanceImpl is detaching to a floaty.
  void OnDetach();

  // Called when daisy chaining occurs on the instance.
  void OnDaisyChain();

  // Called when GlicInstanceImpl::RegisterConversation is called.
  void OnRegisterConversation(const std::string& conversation_id);

  // Called when a GlicInstanceImpl is hidden.
  void OnInstanceHidden();

  // Called when Close is called on the instance.
  void OnClose();

  // Called when Toggle is called on the instance.
  void OnToggle();

  // Called when a tab that was bound to this instance is destroyed.
  void OnBoundTabDestroyed();

  // Called when GlicInstanceImpl::CreateTab is called.
  void OnCreateTab();

  // Called when GlicInstanceImpl::CreateTask is called.
  void OnCreateTask();

  // Called when GlicInstanceImpl::PerformActions is called.
  void OnPerformActions();

  // Called when GlicInstanceImpl::StopActorTask is called.
  void OnStopActorTask();

  // Called when GlicInstanceImpl::PauseActorTask is called.
  void OnPauseActorTask();

  // Called when GlicInstanceImpl::ResumeActorTask is called.
  void OnResumeActorTask();

  // Called when GlicInstanceImpl::WebUiStateChanged is called.
  void OnWebUiStateChanged(mojom::WebUiState state);
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_METRICS_H_
