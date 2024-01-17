// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/accessibility_labels_menu_observer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/accessibility_labels_bubble_model.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"

using content::BrowserThread;

AccessibilityLabelsMenuObserver::AccessibilityLabelsMenuObserver(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy) {}

AccessibilityLabelsMenuObserver::~AccessibilityLabelsMenuObserver() {}

void AccessibilityLabelsMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  if (ShouldShowLabelsItem()) {
    proxy_->AddAccessibilityLabelsServiceItem(profile->GetPrefs()->GetBoolean(
        prefs::kAccessibilityImageLabelsEnabled));
  }
}

bool AccessibilityLabelsMenuObserver::IsCommandIdSupported(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE ||
         command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS ||
         command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE_ONCE;
}

bool AccessibilityLabelsMenuObserver::IsCommandIdChecked(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());

  if (command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE ||
      command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS ||
      command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE_ONCE) {
    return profile->GetPrefs()->GetBoolean(
        prefs::kAccessibilityImageLabelsEnabled);
  }
  return false;
}

bool AccessibilityLabelsMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  if (command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE ||
      command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS ||
      command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE_ONCE) {
    return ShouldShowLabelsItem();
  }
  return false;
}

void AccessibilityLabelsMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  if (command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE) {
    // When a user enables the accessibility labeling item, we
    // show a bubble to confirm it. On the other hand, when a user disables this
    // item, we directly update/ the profile and stop integrating the labels
    // service immediately.
    if (!profile->GetPrefs()->GetBoolean(
            prefs::kAccessibilityImageLabelsEnabled)) {
      // Always show the confirm bubble when enabling the full feature,
      // regardless of whether it's been shown before.
      ShowConfirmBubble(profile, true /* enable always */);
    } else {
      profile->GetPrefs()->SetBoolean(prefs::kAccessibilityImageLabelsEnabled,
                                      false);
    }
  } else if (command_id ==
             IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE_ONCE) {
    // Only show the confirm bubble if it's never been shown before.
    if (!profile->GetPrefs()->GetBoolean(
            prefs::kAccessibilityImageLabelsOptInAccepted)) {
      ShowConfirmBubble(profile, false /* enable once only */);
    } else {
      AccessibilityLabelsServiceFactory::GetForProfile(profile)
          ->EnableLabelsServiceOnce(proxy_->GetWebContents());
    }
  }
}

bool AccessibilityLabelsMenuObserver::ShouldShowLabelsItem() {
  // Disabled by policy.
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  if (!profile->GetPrefs()->GetBoolean(
          prefs::kAccessibilityImageLabelsEnabled) &&
      profile->GetPrefs()->IsManagedPreference(
          prefs::kAccessibilityImageLabelsEnabled)) {
    return false;
  }

  return accessibility_state_utils::IsScreenReaderEnabled();
}

void AccessibilityLabelsMenuObserver::ShowConfirmBubble(Profile* profile,
                                                        bool enable_always) {
  content::WebContents* web_contents = proxy_->GetWebContents();
  // We use the web contents' primary main frame here rather than getting the
  // view from the local render frame host because we want to ensure that it is
  // non-null (proxy_->GetRenderFrameHost() can return nullptr if the frame goes
  // away). In these cases, the spelling preference changes are still valid
  // (tied to the BrowsingContext / WebContents) so we still want to show the
  // confirmation bubble.
  content::RenderWidgetHostView* view =
      web_contents->GetPrimaryMainFrame()->GetRenderWidgetHost()->GetView();
  gfx::Rect rect = view->GetViewBounds();
  auto model = std::make_unique<AccessibilityLabelsBubbleModel>(
      profile, web_contents, enable_always);
  chrome::ShowConfirmBubble(
      web_contents->GetTopLevelNativeWindow(), view->GetNativeView(),
      gfx::Point(rect.CenterPoint().x(), rect.y()), std::move(model));
}
