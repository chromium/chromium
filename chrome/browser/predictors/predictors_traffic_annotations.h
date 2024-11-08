// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREDICTORS_TRAFFIC_ANNOTATIONS_H_
#define CHROME_BROWSER_PREDICTORS_PREDICTORS_TRAFFIC_ANNOTATIONS_H_

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace predictors {

const net::NetworkTrafficAnnotationTag
    kSearchEnginePreconnectTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("search_engine_preconnect",
                                            R"(
    semantics {
      sender: "Loading Predictor"
      description:
        "This request is issued near the start of a navigation to "
        "speculatively fetch resources that resulting page is predicted to "
        "request."
      trigger:
        "Navigating Chrome (by clicking on a link, bookmark, history item, "
        "using session restore, etc)."
      data:
        "Arbitrary site-controlled data can be included in the URL."
        "Requests may include cookies."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "There are a number of ways to prevent this request:"
        "A) Disable predictive operations under Settings > Performance "
        "   > Preload pages for faster browsing and searching,"
        "B) Disable 'Make searches and browsing better' under Settings > "
        "   Sync and Google services > Make searches and browsing better"
      chrome_policy {
        URLBlocklist {
          URLBlocklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLAllowlist {
          URLAllowlist { }
        }
      }
    }
    comments:
      "This feature can be safely disabled, but enabling it may result in "
      "faster page loads. Using either URLBlocklist or URLAllowlist policies "
      "(or a combination of both) limits the scope of these requests."
)");

const net::NetworkTrafficAnnotationTag
    kLoadingPredictorPreconnectTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("predictive_preconnect",
                                            R"(
    semantics {
      sender: "Loading Predictor"
      description:
        "This request is issued near the start of a navigation to "
        "speculatively fetch resources that resulting page is predicted to "
        "request."
      trigger:
        "Navigating Chrome (by clicking on a link, bookmark, history item, "
        "using session restore, etc)."
      data:
        "Arbitrary site-controlled data can be included in the URL."
        "Requests may include cookies."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "There are a number of ways to prevent this request:"
        "A) Disable predictive operations under Settings > Performance "
        "   > Preload pages for faster browsing and searching,"
        "B) Disable 'Make searches and browsing better' under Settings > "
        "   Sync and Google services > Make searches and browsing better"
      chrome_policy {
        URLBlocklist {
          URLBlocklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLAllowlist {
          URLAllowlist { }
        }
      }
    }
    comments:
      "This feature can be safely disabled, but enabling it may result in "
      "faster page loads. Using either URLBlocklist or URLAllowlist policies "
      "(or a combination of both) limits the scope of these requests."
)");

const net::NetworkTrafficAnnotationTag kNetworkHintsTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("network_hints_preconnect",
                                        R"(
    semantics {
      sender: "Linkrel preconnector"
      description:
        "This request is issued near the start of a navigation to "
        "speculatively fetch resources that resulting page is predicted to "
        "request."
      trigger:
        "Navigating Chrome (by clicking on a link, bookmark, history item, "
        "using session restore, etc)."
      data:
        "Arbitrary site-controlled data can be included in the URL."
        "Requests may include cookies."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "There are a number of ways to prevent this request:"
        "A) Disable predictive operations under Settings > Performance "
        "   > Preload pages for faster browsing and searching,"
        "B) Disable 'Make searches and browsing better' under Settings > "
        "   Sync and Google services > Make searches and browsing better"
      chrome_policy {
        URLBlocklist {
          URLBlocklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLAllowlist {
          URLAllowlist { }
        }
      }
    }
    comments:
      "This feature can be safely disabled, but enabling it may result in "
      "faster page loads. Using either URLBlocklist or URLAllowlist policies "
      "(or a combination of both) limits the scope of these requests."
)");

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PREDICTORS_TRAFFIC_ANNOTATIONS_H_
