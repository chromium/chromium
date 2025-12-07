// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/gdp_service_handler.h"

#include "google_apis/gaia/gaia_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kGdpTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("devtools_gdp_service", R"(
      semantics {
        sender: "Developer Tools via Chrome DevTools Protocol"
        description:
          "This request is used by the Google Developer Profiles (GDP) "
          "integration in DevTools to show the user's GDP profile in "
          "settings, award badges, and enable the GDP sign-up flow."
        trigger:
          "User opens Chrome DevTools, interacts "
          "with a badge award notification, or interacts with the settings "
          "page for Google Developer Profiles."
        data:
          "The data sent consists of the user's access token for their Google "
          "Developer Profile and the specific badge name being awarded."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "chrome-devtools@google.com"
          }
        }
        user_data {
          type: ACCESS_TOKEN
        }
        last_reviewed: "2025-09-25"
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        policy_exception_justification: "Not implemented"
      })");

}  // namespace

GdpServiceHandler::GdpServiceHandler() = default;
GdpServiceHandler::~GdpServiceHandler() = default;

GURL GdpServiceHandler::BaseURL() const {
  return GURL("https://developers.googleapis.com");
}

signin::ScopeSet GdpServiceHandler::OAuthScopes() const {
  return {GaiaConstants::kGdpOAuth2Scope};
}

net::NetworkTrafficAnnotationTag
GdpServiceHandler::NetworkTrafficAnnotationTag() const {
  return kGdpTrafficAnnotation;
}
