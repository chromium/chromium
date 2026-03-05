// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/gca_service_handler.h"

#include "chrome/browser/devtools/aida_client.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace {

// Define a new network traffic annotation for the GCA service.
constexpr net::NetworkTrafficAnnotationTag kGcaTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("devtools_gca_service", R"(
      semantics {
        sender: "Chrome DevTools"
        description:
          "Chrome DevTools uses the Gemini Code Assist API "
          "(aicode.googleapis.com) to provide AI-powered features such as "
          "code completion, code generation, and natural language chat about "
          "the code within the DevTools interface."
        trigger:
          "User interacts with an enabled AI-powered feature in Chrome DevTools. "
          "For example, if the user has opted in to Console Insights, typing in "
          "the console may trigger a request. Similarly, using the AI chat panel "
          "or triggering code completion, which requires prior opt-in."
        data:
          "The data sent includes the code context (snippets), user queries "
          "(natural language or code), and data related to the specific AI "
          "feature (Console Insights, AI Assistance, and AI Code Completion) "
          "that the user opted in to. "
          "OAuth 2.0 tokens are also sent to authenticate and authorize the "
          "requests."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            owners: "//chrome/browser/devtools/OWNERS"
          }
        }
        user_data {
          type: ACCESS_TOKEN
          type: USER_CONTENT
          type: WEB_CONTENT
        }
        last_reviewed: "2026-02-19"
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature is controlled by the 'kUseGcaApi' base::Feature flag "
          "and is gated by the respective DevTools AI settings toggles "
          "(e.g., Console Insights, AI Assistance, and AI Code Completion). "
          "Requests are only sent if the user has opted in to at least one AI "
          "feature in DevTools settings. Users must be signed in to Chrome to "
          "use these features."
        chrome_policy {
          DeveloperToolsAvailability {
            DeveloperToolsAvailability: 2
          }
        }
      }
    )");

}  // namespace

const net::NetworkTrafficAnnotationTag& GcaServiceHandler::TrafficAnnotation() {
  return kGcaTrafficAnnotation;
}

GcaServiceHandler::GcaServiceHandler() = default;
GcaServiceHandler::~GcaServiceHandler() = default;

void GcaServiceHandler::CanMakeRequest(
    Profile* profile,
    base::OnceCallback<void(bool success)> callback) {
  AidaClient::Availability availability = AidaClient::CanUseAida(profile);

  if (availability.blocked) {
    std::move(callback).Run(false);
    return;
  }

  DevToolsHttpServiceHandler::CanMakeRequest(profile, std::move(callback));
}

GURL GcaServiceHandler::BaseURL() const {
  return GURL("https://aicode.googleapis.com/");
}

signin::OAuthConsumerId GcaServiceHandler::OAuthConsumerId() const {
  return signin::OAuthConsumerId::kDevtoolsAiCode;
}

net::NetworkTrafficAnnotationTag
GcaServiceHandler::NetworkTrafficAnnotationTag() const {
  return kGcaTrafficAnnotation;
}
