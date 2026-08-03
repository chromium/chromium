// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_H_
#define CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_H_

#include "base/feature_list.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "ui/base/device_form_factor.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
class RenderProcessHost;
class WebContents;
class ClipboardEndpoint;
}  // namespace content

namespace ui {
struct ClipboardMetadata;
}  // namespace ui

class Profile;

namespace tabs {
class TabInterface;
}

namespace glic {

// Returns the URL/origin from where the guest web client will be loaded from.
GURL GetGuestURL();
url::Origin GetGuestOrigin();

// Checks if a preset url is enabled and returns it if so. Otherwise, returns
// `guest_url`.
GURL MaybeApplyPresetGuestUrl(GURL guest_url);

// Returns an updated guest URL that includes a language parameter, set to the
// browser's UI language. If the parameter is already present, its current value
// will not be changed.
GURL GetLocalizedGuestURL(const GURL& guest_url);

// Returns true if `web_contents` contains the Glic WebUI application.
bool IsGlicWebUI(const content::WebContents* web_contents);

// Returns true if `tab` is owned by Glic.
bool IsGlicOwnedTab(tabs::TabInterface* tab);

// Returns true if `web_contents` is the Glic guest WebContents.
bool IsGlicGuest(content::WebContents* web_contents);

// Returns true if `process_host` is either the Glic FRE WebUI or the Glic
// main WebUI.
bool IsProcessHostForGlic(content::RenderProcessHost* process_host);

// Returns the guest web contents if `webui_contents` is the glic host.
content::WebContents* GetGlicGuestWebContents(
    content::WebContents* webui_contents);

// If `guest_contents` is the glic guest, do glic-specific setup and return
// true, otherwise return false.
bool OnGuestAdded(content::WebContents* guest_contents);

// Returns true if the media request ID belongs to any Glic instance.
bool IsMediaRequestFromGlic(content::BrowserContext* browser_context,
                            const std::string& request_id);

// Identifies Glic processes.
void MarkProcessAsGlic(content::RenderProcessHost* rph);

// Instantiates Glic WebUI metadata on a WebContents.
void CreateGlicWebUiData(content::WebContents* webui_contents);

// Returns Glic form factor mapping for the given device form factor.
mojom::FormFactor GetGlicFormFactor(ui::DeviceFormFactor form_factor);

// Returns the Glic Platform.
mojom::Platform GetGlicPlatform();

// Populates the WebClientInitialState fields that do not depend on the
// page handler.
void PopulateGlobalClientInitialState(mojom::WebClientInitialState* state,
                                      Profile* profile);

#if !BUILDFLAG(IS_ANDROID)

// Called before the OS clipboard writes the sequence number. Checks and stashes
// the eligibility for the upcoming copy event.
void OnBeforeClipboardCopy(const content::ClipboardEndpoint& source);

// Applies the pending copy eligibility to the given sequence number.
// This is used in browser tests where we don't actually trigger an OS clipboard
// write, so the ClipboardObserver never fires naturally.
void SetClipboardEligibilitySeqnoForTesting(
    ui::ClipboardSequenceNumberToken seqno);

// Returns true if pasting data into Glic is allowed by policy (based on the
// source tab's page context eligibility).
bool IsClipboardPasteAllowed(const content::ClipboardEndpoint& source,
                             const content::ClipboardEndpoint& destination,
                             const ui::ClipboardMetadata& metadata);

// Logs the format of the clipboard data being pasted into Glic.
void LogPasteAttempt(const content::ClipboardEndpoint& source,
                     const ui::ClipboardMetadata& metadata);
#endif
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_H_
