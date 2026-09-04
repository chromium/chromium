// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_session_base.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "chrome/browser/devtools/devtools_availability_checker.h"
#include "chrome/browser/policy/developer_tools_policy_checker.h"
#include "chrome/browser/policy/developer_tools_policy_checker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"

namespace {

base::HistogramBase::Sample32 GetCommandUmaId(std::string_view command_name) {
  return static_cast<base::HistogramBase::Sample32>(
      base::HashMetricName(command_name));
}

}  // namespace

ChromeDevToolsSessionBase::ChromeDevToolsSessionBase(
    content::DevToolsAgentHostClientChannel* channel)
    : dispatcher_(this), client_channel_(channel) {
  if (Profile* profile = Profile::FromBrowserContext(
          client_channel_->GetAgentHost()->GetBrowserContext())) {
    pref_change_registrar_.Init(profile->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kDevToolsAvailability,
        base::BindRepeating(&ChromeDevToolsSessionBase::OnDevToolsPolicyChanged,
                            base::Unretained(this)));
    if (policy::DeveloperToolsPolicyChecker* checker =
            policy::DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(
                profile)) {
      policy_checker_callback_subscription_ =
          checker->AddObserver(base::BindRepeating(
              &ChromeDevToolsSessionBase::OnDevToolsPolicyChanged,
              base::Unretained(this)));
    }
  }
}

ChromeDevToolsSessionBase::~ChromeDevToolsSessionBase() = default;

void ChromeDevToolsSessionBase::OnDevToolsPolicyChanged() {
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      client_channel_->GetAgentHost();
  Profile* profile =
      Profile::FromBrowserContext(agent_host->GetBrowserContext());
  if (profile && !IsInspectionAllowed(profile, agent_host.get())) {
    agent_host->ForceDetachAllSessions();
  }
}

void ChromeDevToolsSessionBase::HandleCommand(
    base::span<const uint8_t> message,
    content::DevToolsManagerDelegate::NotHandledCallback callback) {
  crdtp::Dispatchable dispatchable(
      crdtp::SpanFrom(message), std::string_view(),
      [cb = std::move(callback)](int call_id, crdtp::span<uint8_t> method,
                                 crdtp::span<uint8_t> message,
                                 std::string_view fallthrough_data) {
        cb.Run(message);
      });
  DCHECK(dispatchable.ok());  // Checked by content::DevToolsSession.

  auto command_uma_id = GetCommandUmaId(std::string_view(
      reinterpret_cast<const char*>(dispatchable.Method().data()),
      dispatchable.Method().size()));
  std::string client_type = client_channel_->GetClient()->GetTypeForMetrics();
  DCHECK(client_type == "DevTools" || client_type == "Extension" ||
         client_type == "RemoteDebugger" || client_type == "Other");
  base::UmaHistogramSparse("DevTools.CDPCommandFrom" + client_type,
                           command_uma_id);

  dispatcher_.Dispatch(dispatchable);
}

// The following methods handle responses or notifications coming from
// the browser to the client.
void ChromeDevToolsSessionBase::SendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void ChromeDevToolsSessionBase::SendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void ChromeDevToolsSessionBase::FlushProtocolNotifications() {}
