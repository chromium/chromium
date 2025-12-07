// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browser_window_helper.h"

#include "base/check_deref.h"
#include "chrome/browser/extensions/app_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/constants.h"
#include "url/origin.h"

namespace extensions {

namespace {

// Returns true if the given |web_contents| should be closed when the extension
// is unloaded.
bool ShouldCloseTabOnExtensionUnload(const Extension* extension,
                                     content::WebContents* web_contents) {
  // Case 1: A "regular" extension page, e.g. chrome-extension://<id>/page.html.
  // Note: we check the tuple or precursor tuple in order to close any
  // windows with opaque origins that were opened by extensions, and may
  // still be running code.
  const url::SchemeHostPort& tuple_or_precursor_tuple =
      web_contents->GetPrimaryMainFrame()
          ->GetLastCommittedOrigin()
          .GetTupleOrPrecursorTupleIfOpaque();
  if (tuple_or_precursor_tuple.scheme() == extensions::kExtensionScheme &&
      tuple_or_precursor_tuple.host() == extension->id()) {
    // Edge-case: Chrome URL overrides (such as NTP overrides) are handled
    // differently (reloaded), and managed by ExtensionWebUI. Ignore them.
    if (!web_contents->GetLastCommittedURL().SchemeIs(
            content::kChromeUIScheme)) {
      return true;
    }
  }

  // Case 2: Check if the page is a page associated with a hosted app, which
  // can have non-extension schemes. For example, the Gmail hosted app would
  // have a URL of https://mail.google.com.
  if (AppTabHelper::FromWebContents(web_contents)->GetExtensionAppId() ==
      extension->id()) {
    return true;
  }

  return false;
}

// Unmutes the given |contents| if it was muted by the extension with
// |extension_id|.
void UnmuteIfMutedByExtension(content::WebContents* contents,
                              const ExtensionId& extension_id) {
  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  if (metadata->reason == TabMutedReason::kExtension &&
      metadata->extension_id == extension_id) {
    SetTabAudioMuted(contents, false, TabMutedReason::kExtension, extension_id);
  }
}

}  // namespace

ExtensionBrowserWindowHelper::ExtensionBrowserWindowHelper(
    chrome::BrowserCommandController* command_controller,
    TabStripModel* tab_strip_model,
    Profile* profile)
    : command_controller_(CHECK_DEREF(command_controller)),
      tab_strip_model_(CHECK_DEREF(tab_strip_model)) {
  registry_observation_.Observe(ExtensionRegistry::Get(profile));
}

ExtensionBrowserWindowHelper::~ExtensionBrowserWindowHelper() = default;

void ExtensionBrowserWindowHelper::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  command_controller_->ExtensionStateChanged();
}

void ExtensionBrowserWindowHelper::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  command_controller_->ExtensionStateChanged();

  // Clean up any tabs from |extension|, unless it was terminated. In the
  // terminated case (as when the extension crashed), we let the sad tabs stay.
  if (reason != extensions::UnloadedExtensionReason::TERMINATE)
    CleanUpTabsOnUnload(extension);
}

void ExtensionBrowserWindowHelper::CleanUpTabsOnUnload(
    const Extension* extension) {
  // Iterate backwards as we may remove items while iterating.
  for (int i = tab_strip_model_->count() - 1; i >= 0; --i) {
    content::WebContents* web_contents = tab_strip_model_->GetWebContentsAt(i);
    if (ShouldCloseTabOnExtensionUnload(extension, web_contents)) {
      // Do not close the last tab if it belongs to the extension. Instead
      // replace it with the default NTP.
      if (tab_strip_model_->count() == 1) {
        const GURL new_tab_url(chrome::kChromeUINewTabURL);
        // Replace the extension page with default NTP. This behavior is similar
        // to how Chrome URL overrides (such as NTP overrides) are handled by
        // ExtensionWebUI.
        web_contents->GetController().LoadURL(new_tab_url, content::Referrer(),
                                              ui::PAGE_TRANSITION_RELOAD,
                                              std::string());
      } else {
        tab_strip_model_->CloseWebContentsAt(i, TabCloseTypes::CLOSE_NONE);
      }
    } else {
      UnmuteIfMutedByExtension(web_contents, extension->id());
    }
  }
}

}  // namespace extensions
