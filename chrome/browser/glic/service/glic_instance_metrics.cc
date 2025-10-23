// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_metrics.h"

#include <string>
#include <string_view>

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/service/glic_ui_types.h"

namespace glic {

GlicInstanceMetrics::GlicInstanceMetrics() = default;

GlicInstanceMetrics::~GlicInstanceMetrics() = default;

void GlicInstanceMetrics::OnInstanceCreated() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Created"));
}

void GlicInstanceMetrics::OnInstanceDestroyed() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Destroyed"));
}

void GlicInstanceMetrics::OnBind() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Bind"));
}

void GlicInstanceMetrics::OnWarmedInstancePromoted() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.PromotedWarmedInstance"));
}

void GlicInstanceMetrics::OnWarmedInstanceCreated() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.CreatedWarmedInstance"));
}

void GlicInstanceMetrics::OnInstanceCreatedWithoutWarming() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.CreatedInstanceWithoutWarming"));
}

void GlicInstanceMetrics::OnSwitchFromConversation(
    const ShowOptions& show_options) {
  if (std::holds_alternative<FloatingShowOptions>(
          show_options.embedder_options)) {
    base::RecordAction(
        base::UserMetricsAction("Glic.Instance.SwitchFromConversation.Floaty"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Glic.Instance.SwitchFromConversation.SidePanel"));
  }
}

void GlicInstanceMetrics::OnSwitchToConversation(
    const ShowOptions& show_options) {
  if (std::holds_alternative<FloatingShowOptions>(
          show_options.embedder_options)) {
    base::RecordAction(
        base::UserMetricsAction("Glic.Instance.SwitchToConversation.Floaty"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Glic.Instance.SwitchToConversation.SidePanel"));
  }
}

void GlicInstanceMetrics::OnShowInSidePanel() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Show.SidePanel"));
}

void GlicInstanceMetrics::OnShowSidePanelViaHotkey() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.Show.SidePanelViaHotkey"));
}

void GlicInstanceMetrics::OnShowInFloaty() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Show.Floaty"));
}

void GlicInstanceMetrics::OnFloatyShownViaHotkey() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.Show.FloatyViaHotkey"));
}

void GlicInstanceMetrics::OnDetach() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Detach"));
}

void GlicInstanceMetrics::OnDaisyChain() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.OnDaisyChain"));
}

void GlicInstanceMetrics::OnRegisterConversation(
    const std::string& conversation_id) {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.RegisterConversation"));
}

void GlicInstanceMetrics::OnInstanceHidden() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Hide"));
}

void GlicInstanceMetrics::OnClose() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Close"));
}

void GlicInstanceMetrics::OnToggle() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Toggle"));
}

void GlicInstanceMetrics::OnBoundTabDestroyed() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.BoundTabDestroyed"));
}

void GlicInstanceMetrics::OnCreateTab() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.CreateTab"));
}

void GlicInstanceMetrics::OnCreateTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.CreateTask"));
}

void GlicInstanceMetrics::OnPerformActions() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.PerformActions"));
}

void GlicInstanceMetrics::OnStopActorTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.StopActorTask"));
}

void GlicInstanceMetrics::OnPauseActorTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.PauseActorTask"));
}

void GlicInstanceMetrics::OnResumeActorTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.ResumeActorTask"));
}

void GlicInstanceMetrics::OnWebUiStateChanged(mojom::WebUiState state) {
  switch (state) {
    case mojom::WebUiState::kUninitialized:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.Uninitialized"));
      break;
    case mojom::WebUiState::kBeginLoad:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.BeginLoad"));
      break;
    case mojom::WebUiState::kShowLoading:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.ShowLoading"));
      break;
    case mojom::WebUiState::kHoldLoading:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.HoldLoading"));
      break;
    case mojom::WebUiState::kFinishLoading:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.FinishLoading"));
      break;
    case mojom::WebUiState::kError:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.Error"));
      break;
    case mojom::WebUiState::kOffline:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.Offline"));
      break;
    case mojom::WebUiState::kUnavailable:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.Unavailable"));
      break;
    case mojom::WebUiState::kReady:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.Ready"));
      break;
    case mojom::WebUiState::kUnresponsive:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.Unresponsive"));
      break;
    case mojom::WebUiState::kSignIn:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.SignIn"));
      break;
    case mojom::WebUiState::kGuestError:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.GuestError"));
      break;
    case mojom::WebUiState::kDisabledByAdmin:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.DisabledByAdmin"));
      break;
  }
}

}  // namespace glic
