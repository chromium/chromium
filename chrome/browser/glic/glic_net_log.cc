// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_net_log.h"

#include "net/log/net_log_with_source.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace glic::net_log {

namespace {

constexpr net::NetworkTrafficAnnotationTag kGlicWebUITrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("glic_web_ui", R"(
    semantics {
      sender: "Gemini in Chrome"
      description:
        "This request is issued when signed-in user tries to access Gemini in "
        "Chrome after a consent was given before hand to use this feature "
        "(i.e. share page content)."
      trigger:
        "Clicking Gemini button or navigating to chrome://glic. Also, "
        "pre-warming chrome://glic, so it loads faster, would be considered as "
        "a trigger."
      data:
        "As the user interacts with the feature, the user can choose to share "
        "a significant amount of data about a browser tab including its URL, "
        "title, text, a screenshot of the currently visible portion of the web "
        "page, and even the raw data of an embedded PDF. All that data is "
        "going to be used as context to provide better answers to the user's "
        "prompts."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          owners: "//chrome/browser/glic/OWNERS"
        }
      }
      user_data {
        type: ACCESS_TOKEN
        type: SENSITIVE_URL
        type: WEB_CONTENT
        type: IMAGE
        type: FILE_DATA
      }
      last_reviewed: "2025-07-22"
    }
    policy {
      cookies_allowed: YES
      cookies_store: "uses a separate cookie store"
      setting: "This feature cannot be disabled by settings."
      chrome_policy {
        GeminiSettings {
            GeminiSettings: 1
        }
        GenAiDefaultSettings {
          GenAiDefaultSettings: 2
        }
      }
    })");

constexpr net::NetworkTrafficAnnotationTag kGlicFreWebUITrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("glic_fre_web_ui", R"(
    semantics {
      sender: "Gemini in Chrome"
      description:
        "Signed-in users who haven't already given consent to share page "
        "content will encounter a first-run experience the first time they try "
        "to use Gemini in Chrome."
      trigger:
        "The user clicks the Gemini button without having given consent to "
        "share page content (i.e. first time user or previous deny/ignore of "
        "consent). It can also be triggered by navigating to chrome://glic-fre."
        "Also, pre-warming chrome://glic-fre, so it loads faster, would be "
        "considered as a trigger."
      data:
        "Minimal data is exchanged. Cookies may also be sent to the "
        "destination URL."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          owners: "//chrome/browser/glic/OWNERS"
        }
      }
      user_data {
        type: ACCESS_TOKEN
      }
      last_reviewed: "2025-07-22"
    }
    policy {
      cookies_allowed: YES
      cookies_store: "uses a separate cookie store"
      setting: "This feature cannot be disabled by settings."
      chrome_policy {
          GeminiSettings {
              GeminiSettings: 1
          }
        GenAiDefaultSettings {
          GenAiDefaultSettings: 2
        }
      }
    })");

}  // namespace

void LogDummyNetworkRequestForTrafficAnnotation(const GURL& url,
                                                GlicPage glic_page) {
  net::NetLogWithSource net_log =
      net::NetLogWithSource::Make(net::NetLogSourceType::URL_REQUEST);
  net_log.AddEvent(net::NetLogEventType::REQUEST_ALIVE, [&]() {
    base::Value::Dict dict;
    dict.Set("priority", "IDLE");
    dict.Set("url", url.spec());
    dict.Set("traffic_annotation",
             (glic_page == GlicPage::kGlicFre
                  ? kGlicFreWebUITrafficAnnotation.unique_id_hash_code
                  : kGlicWebUITrafficAnnotation.unique_id_hash_code));
    dict.Set("dummy_request", true);
    return dict;
  });
}

}  // namespace glic::net_log
