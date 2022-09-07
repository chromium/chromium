// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_FEATURE_LIST_CREATOR_H_
#define ANDROID_WEBVIEW_BROWSER_AW_FEATURE_LIST_CREATOR_H_

#include <memory>
#include <utility>

#include "android_webview/browser/aw_browser_policy_connector.h"
#include "android_webview/browser/aw_field_trials.h"
#include "android_webview/browser/variations/aw_variations_service_client.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/variations/service/variations_field_trial_creator.h"

class PrefService;

namespace android_webview {

// Used by WebView to set up field trials based on the stored variations seed
// data.
class AwFeatureListCreator {
 public:
  AwFeatureListCreator();

  AwFeatureListCreator(const AwFeatureListCreator&) = delete;
  AwFeatureListCreator& operator=(const AwFeatureListCreator&) = delete;

  ~AwFeatureListCreator();

  // Initializes all necessary parameters to create the feature list and setup
  // field trials.
  void CreateFeatureListAndFieldTrials();

  void CreateLocalState();

  // Passes ownership of the |local_state_| to the caller.
  std::unique_ptr<PrefService> TakePrefService() {
    DCHECK(local_state_);
    return std::move(local_state_);
  }

  // Passes ownership of the |browser_policy_connector_| to the caller.
  std::unique_ptr<AwBrowserPolicyConnector> TakeBrowserPolicyConnector() {
    DCHECK(browser_policy_connector_);
    return std::move(browser_policy_connector_);
  }

  static void DisableSignatureVerificationForTesting();

 private:
  std::unique_ptr<PrefService> CreatePrefService();

  // Sets up the field trials and related initialization.
  void SetUpFieldTrials();

  // Stores the seed. VariationsSeedStore keeps a raw pointer to this, so it
  // must persist for the process lifetime. Not persisted accross runs.
  // If TakePrefService() is called, the caller will take the ownership
  // of this variable. Stop using this variable afterwards.
  std::unique_ptr<PrefService> local_state_;

  // Performs set up for any WebView specific field trials.
  std::unique_ptr<AwFieldTrials> aw_field_trials_;

  // Responsible for creating a feature list from the seed.
  std::unique_ptr<variations::VariationsFieldTrialCreator>
      variations_field_trial_creator_;

  std::unique_ptr<AwVariationsServiceClient> client_;

  std::unique_ptr<AwBrowserPolicyConnector> browser_policy_connector_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_FEATURE_LIST_CREATOR_H_
