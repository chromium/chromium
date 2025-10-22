// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/aida_service_handler.h"

#include "chrome/browser/devtools/aida_client.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

const net::NetworkTrafficAnnotationTag kAidaTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("devtools_cdp_console_insights", R"(
      semantics {
        sender: "Developer Tools via CDP"
        description:
          "In Chrome DevTools, the user can ask for additional insights "
          "regarding an error message. A prompt message for AIDA containing "
          "the error message and sometimes more context such as stack trace, "
          "surrounding code, or network headers is sent to the Chrome "
          "backend via DevTools UI bindings, which in turn queries an AIDA "
          "endpoint."
        trigger: "User asks for more insights on a DevTools error message."
        data: "Prompt for AIDA endpoint, containing instructions, error and "
          "sometimes some additional context information."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "chrome-devtools@google.com"
          }
        }
        user_data {
          type: WEB_CONTENT
        }
        last_reviewed: "2023-11-09"
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting:
          "It's not possible to disable this feature from settings."
        chrome_policy {
          DeveloperToolsAvailability {
            policy_options {mode: MANDATORY}
            DeveloperToolsAvailability: 2
          }
        }
      })");

}  // namespace

const net::NetworkTrafficAnnotationTag&
AidaServiceHandler::TrafficAnnotation() {
  return kAidaTrafficAnnotation;
}

AidaServiceHandler::AidaServiceHandler() = default;
AidaServiceHandler::~AidaServiceHandler() = default;

void AidaServiceHandler::CanMakeRequest(
    Profile* profile,
    base::OnceCallback<void(bool success)> callback) {
  AidaClient::Availability availability = AidaClient::CanUseAida(profile);

  if (availability.blocked) {
    std::move(callback).Run(false);
    return;
  }

  DevToolsHttpServiceHandler::CanMakeRequest(profile, std::move(callback));
}

GURL AidaServiceHandler::BaseURL() const {
  return GURL("https://aida.googleapis.com");
}

signin::ScopeSet AidaServiceHandler::OAuthScopes() const {
  return {GaiaConstants::kAidaOAuth2Scope};
}

net::NetworkTrafficAnnotationTag
AidaServiceHandler::NetworkTrafficAnnotationTag() const {
  return kAidaTrafficAnnotation;
}
