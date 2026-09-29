// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UTILS_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UTILS_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/lens_bitmap_processing.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

class Profile;
class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_search {
enum class ContextualSearchSource;
class ContextualSearchMetricsRecorder;
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace lens {
class ClientToAimMessage;
}  // namespace lens

namespace contextual_tasks {
namespace mojom {
class Page;
}  // namespace mojom

class ContextualTasksUIInterface;
class AimMessagePoster;
struct SiteExclusionDetail;

// Utility method to create config params for the
// ContextualSearchContextController.
std::unique_ptr<
    contextual_search::ContextualSearchContextController::ConfigParams>
CreateQueryControllerConfigParams();

// Finds the UI interface associated with the given WebContents. Returns nullptr
// if the `web_contents` does not have an associated UI.
ContextualTasksUIInterface* GetWebUiInterface(
    content::WebContents* web_contents);

// Shows the error page on the given page and records the error page shown
// metric for the given source.
void ShowAndRecordErrorPage(mojo::Remote<contextual_tasks::mojom::Page>& page,
                            contextual_search::ContextualSearchSource source);

// Records the error page shown metric for the given source.
void RecordErrorPageShown(contextual_search::ContextualSearchSource source);

// Records the HTTP response code of the inner frame contents.
void RecordInnerFrameContentsHttpResponseCode(int http_status_code,
                                              bool is_zero_state);

// Returns true if tab sharing and tab input capabilities are supported
// for the given profile (checking AIM and Fusebox eligibility).
bool IsTabSharingEligible(Profile* profile);

// Returns true if tab sharing is eligible and context sharing is enabled for
// the given profile.
bool CanShareTabContext(Profile* profile);

// Creates the image encoding options used for uploading images and tab
// viewports.
lens::ImageEncodingOptions CreateImageEncodingOptions();

// Records metrics when a tab is added as context.
void RecordTabAddedMetric(
    tabs::TabInterface* tab,
    contextual_search::ContextualSearchMetricsRecorder* metrics_recorder,
    bool is_tab_suggestion_chip,
    BrowserWindowInterface* browser_window_interface = nullptr);

#if !BUILDFLAG(IS_ANDROID)
using TabContextSnapshotCallback = base::OnceCallback<void(
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data)>;

// Captures page context for the given `tab_id` and uploads it to
// `session_handle` (or snapshots it via `on_snapshot` when `delay_upload` is
// true). Creates a context token on `session_handle`, invokes
// `on_token_created`, initiates async page context extraction via
// TabContextualizationController, underlines the tab strip if enabled, and on
// completion checks `is_token_valid` before starting upload or snapshotting.
// If `on_context_uploaded` is provided, it is invoked after upload/snapshot.
// Returns the created context token on success, or an error.
base::expected<base::UnguessableToken,
               contextual_search::ContextUploadErrorType>
CaptureAndUploadTabContext(
    int32_t tab_id,
    contextual_search::ContextualSearchSessionHandle* session_handle,
    bool delay_upload = false,
    base::RepeatingClosure on_context_uploaded = base::DoNothing(),
    BrowserWindowInterface* browser_window_interface = nullptr,
    base::RepeatingCallback<bool(const base::UnguessableToken&)>
        is_token_valid = base::NullCallback(),
    TabContextSnapshotCallback on_snapshot = base::NullCallback(),
    base::OnceCallback<void(const base::UnguessableToken&)> on_token_created =
        base::NullCallback());

// Removes the local tab underline for `tab_id` if context management is
// enabled.
void RemoveTabUnderline(
    int32_t tab_id,
    BrowserWindowInterface* browser_window_interface = nullptr);
#endif

// Returns true if the given URL is valid to show as a suggested tab.
// `profile` and `site_exclusion_detail` must be non-null.
bool IsValidUrlForSuggestedTab(const GURL& url,
                               Profile* profile,
                               SiteExclusionDetail& site_exclusion_detail);

// Prepares the information needed to create an AIM query request.
// This utility handles:
// - Standard proto metadata (query, tools, models).
// - Deletion of spent injected inputs from the WebUI.
// - Integration of the Lens Overlay interaction token.
std::unique_ptr<contextual_search::ContextualSearchContextController::
                    CreateClientToAimRequestInfo>
PrepareClientToAimRequestInfo(
    const std::string& query,
    contextual_search::ContextualSearchSessionHandle* session_handle,
    AimMessagePoster* message_poster,
    omnibox::ToolMode active_tool,
    omnibox::ModelMode active_model,
    std::optional<int64_t> active_tab_context_id,
    std::optional<base::UnguessableToken> overlay_token,
    bool is_voice_search,
    const std::map<std::string, std::string>& additional_cgi_params = {});

// Finalizes the AIM query request (consuming tokens) and delivers it to the
// page.
void FinalizeAndSendAimQuery(
    std::unique_ptr<contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> request_info,
    contextual_search::ContextualSearchSessionHandle* session_handle,
    AimMessagePoster* message_poster);

// Sends a message to the WebUI that an injected input has been removed.
void SendInjectedInputRemovedUpdate(AimMessagePoster* message_poster,
                                    const std::string& id);

// Returns true if the side panel should be used instead of the bottom sheet.
bool ShouldShowSidePanel();

// Returns true if running on Android mobile (phone form factor).
bool IsAndroidMobileFormFactor();

// Returns true if running on a large-screen Android device (tablet or desktop
// form factor).
bool IsAndroidLargeFormFactor();

// Returns whether the provided URL is to a contextual tasks WebUI page.
bool IsContextualTasksUrl(const GURL& url);

// Returns the functional URL (usually the inner frame URL) for the given
// WebContents if it is a Contextual Tasks page; otherwise returns an empty
// GURL.
GURL GetContextualTasksFunctionalURL(content::WebContents* web_contents);

// Returns the pretty display URL (e.g. chrome://google.com/search) for the
// given WebContents if it is a Contextual Tasks page.
GURL GetContextualTasksDisplayURL(content::WebContents* web_contents);

// Returns the effective pin state for the contextual tasks button.
bool GetEffectivePinState(Profile* profile);

#if !BUILDFLAG(IS_ANDROID)
// Updates the visibility of the contextual tasks pinned toolbar ActionItem.
void UpdatePinButtonVisibilityState(BrowserWindowInterface* browser_window);
#endif

// Returns whether dark mode should be used for the given profile and URL.
// If the URL contains a 'cs' parameter, that takes precedence. Otherwise,
// returns true if ThemeService uses dark colors or if the profile is
// off-the-record (Incognito).
bool ShouldUseDarkMode(Profile* profile, const GURL& url);

// Returns whether dark mode should be used for the given profile. Returns true
// if ThemeService uses dark colors or if the profile is off-the-record
// (Incognito).
bool ShouldUseDarkMode(Profile* profile);

// Returns the ClientToAimMessage containing the HandshakePing
// with supported capabilities.
lens::ClientToAimMessage GetHandshakeMessageProto();

// Returns the serialized ClientToAimMessage containing the HandshakePing
// with supported capabilities.
std::vector<uint8_t> GetSerializedHandshakeMessage();

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UTILS_H_
