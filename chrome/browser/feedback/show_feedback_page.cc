// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/feedback/feedback_util.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#endif

namespace feedback_private = extensions::api::feedback_private;

namespace chrome {

namespace {

#if defined(OS_CHROMEOS)
// Returns if the feedback page is considered to be triggered from user
// interaction.
bool IsFromUserInteraction(FeedbackSource source) {
  switch (source) {
    case kFeedbackSourceArcApp:
    case kFeedbackSourceAsh:
    case kFeedbackSourceAssistant:
    case kFeedbackSourceBrowserCommand:
    case kFeedbackSourceDesktopTabGroups:
    case kFeedbackSourceMdSettingsAboutPage:
    case kFeedbackSourceOldSettingsAboutPage:
      return true;
    default:
      return false;
  }
}
#endif
}  // namespace

void ShowFeedbackPage(const Browser* browser,
                      FeedbackSource source,
                      const std::string& description_template,
                      const std::string& description_placeholder_text,
                      const std::string& category_tag,
                      const std::string& extra_diagnostics) {
  GURL page_url;
  if (browser) {
    page_url = GetTargetTabUrl(browser->session_id(),
                               browser->tab_strip_model()->active_index());
  }

  Profile* profile = GetFeedbackProfile(browser);
  if (!profile) {
    LOG(ERROR) << "Cannot invoke feedback: No profile found!";
    return;
  }
  if (!profile->GetPrefs()->GetBoolean(prefs::kUserFeedbackAllowed)) {
    return;
  }
  // Record an UMA histogram to know the most frequent feedback request source.
  UMA_HISTOGRAM_ENUMERATION("Feedback.RequestSource", source,
                            kFeedbackSourceCount);

  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(profile);

  feedback_private::FeedbackFlow flow =
      source == kFeedbackSourceSadTabPage
          ? feedback_private::FeedbackFlow::FEEDBACK_FLOW_SADTABCRASH
          : feedback_private::FeedbackFlow::FEEDBACK_FLOW_REGULAR;

  bool include_bluetooth_logs = false;
#if defined(OS_CHROMEOS)
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager &&
      feedback_util::IsGoogleEmail(
          identity_manager->GetPrimaryAccountInfo().email)) {
    flow = feedback_private::FeedbackFlow::FEEDBACK_FLOW_GOOGLEINTERNAL;
    include_bluetooth_logs = IsFromUserInteraction(source);
  }
#endif

  api->RequestFeedbackForFlow(
      description_template, description_placeholder_text, category_tag,
      extra_diagnostics, page_url, flow, source == kFeedbackSourceAssistant,
      include_bluetooth_logs);
}

}  // namespace chrome
