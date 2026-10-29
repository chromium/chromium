// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_H_
#define CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_H_

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom-forward.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/mojom/window_open_disposition.mojom-forward.h"
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

class PrefService;
class Profile;

namespace tabs {
class TabInterface;
}

namespace glic {
class Host;
class GlicWebContentsManager;
class GlicWebClientManager;

// Prepares a WebContents to host the Glic guest client by configuring
// preferences, draggable regions, process markers, and attaching
// GlicGuestObserver.
void PrepareGlicGuestWebContents(content::WebContents& guest_contents,
                                 GlicWebContentsManager& contents_manager);

void SetHostForGuest(content::WebContents& guest_contents, Host* host);
void SetContentsManagerForWebContents(content::WebContents* web_contents,
                                      GlicWebContentsManager* contents_manager);
GlicWebContentsManager* GetContentsManagerForWebContents(
    content::WebContents* web_contents);
GlicWebClientManager* GetWebClientManagerForWebContents(
    content::WebContents* web_contents);

// Returns the URL/origin from where the guest web client will be loaded from.
GURL GetGuestURL(content::BrowserContext* browser_context);
url::Origin GetGuestOrigin(content::BrowserContext* browser_context);
std::string GetGlicAllowedOrigins();
bool IsOriginAllowedGlicApi(const url::Origin& origin,
                            content::BrowserContext* browser_context);
bool IsGuestOriginAllowed(const url::Origin& origin,
                          content::BrowserContext* browser_context);
bool IsAdminBlockedUrl(const GURL& url);
bool IsFrameAllowedGlicApi(content::RenderFrameHost& frame_host);

// Returns the maximum time to wait for the guest web client to load before
// timing out.
base::TimeDelta GetMaxLoadingTime();

// Returns the StoragePartitionConfig for the Glic webview storage partition.
content::StoragePartitionConfig GetGlicStoragePartitionConfig(
    content::BrowserContext* browser_context);

// Checks if a preset url is enabled and returns it if so. Otherwise, returns
// `guest_url`.
GURL MaybeApplyPresetGuestUrl(GURL guest_url);

// Returns an updated guest URL that includes a language parameter, set to the
// browser's UI language. If the parameter is already present, its current value
// will not be changed.
GURL GetLocalizedGuestURL(const GURL& guest_url);

// Returns true if `web_contents` contains the Glic WebUI application.
bool IsGlicWebUI(const content::WebContents* web_contents);

// Returns true if `web_contents` is the Glic guest WebContents.
bool IsGlicGuest(const content::WebContents* web_contents);

// Handles off-PWC window creation requested by a Glic guest (e.g. from
// window.open or target=_blank links). Opens the URL in a new tab or popup.
// Returns true if handled.
bool MaybeHandleGlicWindowCreation(
    content::RenderFrameHost* opener,
    const GURL& target_url,
    ui::mojom::WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features);

// Returns true if `web_contents` contains the Glic overlay WebUI.
bool IsGlicOverlay(const content::WebContents* web_contents);

// Returns true if `web_contents` is any Glic WebContents (guest, WebUI, or
// overlay).
bool IsAnyGlicWebContents(const content::WebContents* web_contents);

// Binds WebClientHandler for guest frame.
void BindGlicWebClientHandler(
    content::RenderFrameHost* rfh,
    mojo::PendingReceiver<glic::mojom::WebClientHandler> receiver);
// Returns true if `process_host` is either the Glic FRE WebUI or the Glic
// main WebUI.
bool IsProcessHostForGlic(content::RenderProcessHost* process_host);

// Returns the guest web contents if `web_contents` is the glic host or already
// the glic guest.
content::WebContents* GetGlicGuestWebContents(
    content::WebContents* web_contents);

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

// Instantiates Glic overlay metadata on a WebContents.
void CreateGlicOverlayData(content::WebContents* overlay_contents);

// Returns Glic form factor mapping for the given device form factor.
mojom::FormFactor GetGlicFormFactor(ui::DeviceFormFactor form_factor);

// Returns the Glic Platform.
mojom::Platform GetGlicPlatform();

// Returns the active onboarding arm for the profile, or std::nullopt if the
// user has already completed onboarding or if `profile` is null.
std::optional<std::string> GetOnboardingArm(Profile* profile);

// Populates the WebClientInitialState fields that do not depend on the
// page handler.
void PopulateGlobalClientInitialState(mojom::WebClientInitialState* state,
                                      Profile* profile);

// Returns the Glic zoom factor calculated from the zoom level pref.
double GetZoomFactor(PrefService* pref_service);

// Evaluates the copy-time page context eligibility for `source`. Returns
// `std::nullopt` if paste eligibility checks are disabled or `source` does not
// correspond to a tab frame.
std::optional<bool> GetClipboardCopyEligibility(
    const content::ClipboardEndpoint& source);

// Stages `is_eligible` to be bound to the next clipboard sequence number
// produced by an upcoming copy, or clears any pending staged eligibility when
// `is_eligible` is `std::nullopt`. When `clear_at_end_of_task` is true, any
// unconsumed pending state is automatically cleared at the end of the current
// task turn if no clipboard write occurred; otherwise unconsumed pending state
// expires after a bounded timeout so abandoned writes never stay armed
// indefinitely.
void StageClipboardCopyEligibility(std::optional<bool> is_eligible,
                                   bool clear_at_end_of_task = false);

// Returns true if copy eligibility is currently staged awaiting a clipboard
// write.
bool HasPendingClipboardCopyEligibilityForTesting();

// Directly binds `is_eligible` to `seqno` and clears any pending staged copy
// eligibility.
void SetClipboardEligibilityForSeqno(ui::ClipboardSequenceNumberToken seqno,
                                     bool is_eligible);

// Called before the OS clipboard writes the sequence number. Checks and stashes
// the eligibility for the upcoming copy event.
void OnBeforeClipboardCopy(const content::ClipboardEndpoint& source);

// Captures copy-time Glic page-context eligibility for `source` before
// asynchronous copy policy checks run and wraps `callback` so the captured
// eligibility is staged only when the copy is allowed (or replaced with an
// enterprise warning string).
content::ContentBrowserClient::IsClipboardCopyAllowedCallback
WrapClipboardCopyAllowedCallback(
    const content::ClipboardEndpoint& source,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback);

// Applies the pending copy eligibility to the given sequence number.
// This is used in browser tests where we don't actually trigger an OS clipboard
// write, so the ClipboardObserver never fires naturally.
void SetClipboardEligibilitySeqnoForTesting(
    ui::ClipboardSequenceNumberToken seqno);

// Resets cached and pending clipboard eligibility state for tests.
void ResetClipboardEligibilityMonitorForTesting();

// Returns true if pasting data into Glic is allowed by policy (based on the
// source tab's page context eligibility).
bool IsClipboardPasteAllowed(const content::ClipboardEndpoint& source,
                             const content::ClipboardEndpoint& destination,
                             const ui::ClipboardMetadata& metadata);

// Returns true if pasting into Glic must be blocked because `metadata.seqno`
// came from a blocked or still-scanning enterprise copy, or from a
// Chrome-only copy whose source `BrowserContext` is gone. Surfaces a
// paste-block dialog over Glic unless a copy scan is still in flight.
bool MaybeBlockPasteForRestrictedCopy(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const ui::ClipboardMetadata& metadata);

// Wraps `callback` so asynchronous paste policy completions (e.g., after a
// warning bypass) still block restricted copies if the source
// `BrowserContext` disappeared or a copy restriction completed in the
// meantime.
content::ContentBrowserClient::IsClipboardPasteAllowedCallback
WrapClipboardPasteAllowedCallback(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const ui::ClipboardMetadata& metadata,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback);

// Logs the format of the clipboard data being pasted into Glic.
void LogPasteAttempt(const content::ClipboardEndpoint& source,
                     const ui::ClipboardMetadata& metadata);
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_H_
