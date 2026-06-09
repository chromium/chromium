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
}  // namespace content

namespace glic {

BASE_DECLARE_FEATURE(kGlicGuestUrlMultiInstanceParam);

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

// If multi-instance is enabled return the guest_url with the multi-instance
// parameter added. Otherwise return the guest_url unchanged.
GURL MaybeAddMultiInstanceParameter(const GURL& guest_url);

// Returns true if `web_contents` contains the Glic WebUI application.
bool IsGlicWebUI(const content::WebContents* web_contents);

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
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_H_
