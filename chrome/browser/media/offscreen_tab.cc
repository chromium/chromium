// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/offscreen_tab.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "components/media_router/browser/presentation/presentation_navigation_policy.h"
#include "components/media_router/browser/presentation/receiver_presentation_service_delegate_impl.h"  // nogncheck
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/presentation_receiver_flags.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#endif  // defined(USE_AURA)

using content::WebContents;

namespace {

// Time intervals used by the logic that detects when the capture of an
// offscreen tab has stopped, to automatically tear it down and free resources.
constexpr base::TimeDelta kMaxWaitForCapture = base::Minutes(1);
constexpr base::TimeDelta kPollInterval = base::Seconds(1);

}  // namespace

#if defined(USE_AURA)
// A WindowObserver that automatically finds a root Window to adopt the
// WebContents native view containing the tab content being streamed, when the
// native view is offscreen, or gets detached from the aura window tree. This is
// a workaround for Aura, which requires the WebContents native view be attached
// somewhere in the window tree in order to gain access to the compositing and
// capture functionality. The WebContents native view, although attached to the
// window tree, does not become visible on-screen (until it is properly made
// visible by the user, for example by switching to the tab).
class OffscreenTab::WindowAdoptionAgent final : protected aura::WindowObserver {
 public:
  explicit WindowAdoptionAgent(aura::Window* content_window)
      : content_window_(content_window) {
    if (content_window_) {
      content_window->AddObserver(this);
      ScheduleFindNewParentIfDetached(content_window_->GetRootWindow());
    }
  }

  WindowAdoptionAgent(const WindowAdoptionAgent&) = delete;
  WindowAdoptionAgent& operator=(const WindowAdoptionAgent&) = delete;

  ~WindowAdoptionAgent() final {
    if (content_window_)
      content_window_->RemoveObserver(this);
  }

 protected:
  void ScheduleFindNewParentIfDetached(aura::Window* root_window) {
    if (root_window)
      return;
    // Post a task to return to the event loop before finding a new parent, to
    // avoid clashing with the currently-in-progress window tree hierarchy
    // changes.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&WindowAdoptionAgent::FindNewParent,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  // aura::WindowObserver implementation.
  void OnWindowDestroyed(aura::Window* window) final {
    DCHECK_EQ(content_window_, window);
    content_window_ = nullptr;
  }

  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) final {
    ScheduleFindNewParentIfDetached(new_root);
  }

 private:
  void FindNewParent() {
    // The window may have been destroyed by the time this is reached.
    if (!content_window_)
      return;
    // If the window has already been attached to a root window, then it's not
    // necessary to find a new parent.
    if (content_window_->GetRootWindow())
      return;
    BrowserList* const browsers = BrowserList::GetInstance();
    Browser* const active_browser =
        browsers ? browsers->GetLastActive() : nullptr;
    BrowserWindow* const active_window =
        active_browser ? active_browser->window() : nullptr;
    aura::Window* const native_window =
        active_window ? active_window->GetNativeWindow() : nullptr;
    aura::Window* const root_window =
        native_window ? native_window->GetRootWindow() : nullptr;
    if (root_window) {
      DVLOG(2) << "Root window " << root_window << " adopts the content window "
               << content_window_ << '.';
      root_window->AddChild(content_window_);
    } else {
      LOG(WARNING) << "Unable to find an aura root window.  "
                      "Compositing of the content may be halted!";
    }
  }

  raw_ptr<aura::Window> content_window_;
  base::WeakPtrFactory<WindowAdoptionAgent> weak_ptr_factory_{this};
};
#endif  // defined(USE_AURA)

OffscreenTab::OffscreenTab(Owner* owner, content::BrowserContext* context)
    : owner_(owner),
      otr_profile_(Profile::FromBrowserContext(context)->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForMediaRouter(),
          /*create_if_needed=*/true)),
      content_capture_was_detected_(false),
      navigation_policy_(
          std::make_unique<media_router::DefaultNavigationPolicy>()) {
  DCHECK(owner_);
  otr_profile_->AddObserver(this);
}

OffscreenTab::~OffscreenTab() {
  DVLOG(1) << "Destroying OffscreenTab for start_url=" << start_url_.spec();
  if (otr_profile_) {
    otr_profile_->RemoveObserver(this);
    ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile_);
  }
}

void OffscreenTab::Start(const GURL& start_url,
                         const gfx::Size& initial_size,
                         const std::string& optional_presentation_id) {
  DCHECK(start_time_.is_null());
  start_url_ = start_url;
  DVLOG(1) << "Starting OffscreenTab with initial size of "
           << initial_size.ToString() << " for start_url=" << start_url_.spec();

  // Create the WebContents to contain the off-screen tab's page.
  WebContents::CreateParams params(otr_profile_);
  if (!optional_presentation_id.empty())
    params.starting_sandbox_flags = content::kPresentationReceiverSandboxFlags;

  offscreen_tab_web_contents_ = WebContents::Create(params);
  offscreen_tab_web_contents_->SetOwnerLocationForDebug(FROM_HERE);
  offscreen_tab_web_contents_->SetDelegate(this);
  WebContentsObserver::Observe(offscreen_tab_web_contents_.get());

#if defined(USE_AURA)
  window_agent_ = std::make_unique<WindowAdoptionAgent>(
      offscreen_tab_web_contents_->GetNativeView());
#endif

  // Set initial size, if specified.
  if (!initial_size.IsEmpty()) {
    offscreen_tab_web_contents_.get()->Resize(gfx::Rect(initial_size));
  }

  // Mute audio output.  When tab capture starts, the audio will be
  // automatically unmuted, but will be captured into the MediaStream.
  offscreen_tab_web_contents_->SetAudioMuted(true);

  if (!optional_presentation_id.empty()) {
    // This offscreen tab is a presentation created through the Presentation
    // API. https://www.w3.org/TR/presentation-api/
    //
    // Create a ReceiverPresentationServiceDelegateImpl associated with the
    // offscreen tab's WebContents.  The new instance will set up the necessary
    // infrastructure to allow controlling pages the ability to connect to the
    // offscreen tab.
    DVLOG(1) << "Register with ReceiverPresentationServiceDelegateImpl, "
             << "presentation_id=" << optional_presentation_id;
    media_router::ReceiverPresentationServiceDelegateImpl::CreateForWebContents(
        offscreen_tab_web_contents_.get(), optional_presentation_id);

    // Presentations are not allowed to perform top-level navigations after
    // initial load.  This is enforced through sandboxing flags, but we also
    // enforce it here.
    navigation_policy_ =
        std::make_unique<media_router::PresentationNavigationPolicy>();
  }

  // Navigate to the initial URL.
  content::NavigationController::LoadURLParams load_params(start_url_);
  load_params.should_replace_current_entry = true;
  load_params.should_clear_history_list = true;
  offscreen_tab_web_contents_->GetController().LoadURLWithParams(load_params);

  start_time_ = base::TimeTicks::Now();
  DieIfContentCaptureEnded();
}

void OffscreenTab::Close() {
  if (offscreen_tab_web_contents_)
    offscreen_tab_web_contents_->ClosePage();
}

void OffscreenTab::CloseContents(WebContents* source) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Javascript in the page called window.close().
  DVLOG(1) << "OffscreenTab for start_url=" << start_url_.spec() << " will die";
  owner_->DestroyTab(this);
  // |this| is no longer valid.
}

bool OffscreenTab::ShouldSuppressDialogs(WebContents* source) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Suppress all because there is no possible direct user interaction with
  // dialogs.
  // TODO(crbug.com/40526231): This does not suppress window.print().
  return true;
}

bool OffscreenTab::ShouldFocusLocationBarByDefault(WebContents* source) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Indicate the location bar should be focused instead of the page, even
  // though there is no location bar.  This will prevent the page from
  // automatically receiving input focus, which should never occur since there
  // is not supposed to be any direct user interaction.
  return true;
}

bool OffscreenTab::ShouldFocusPageAfterCrash(content::WebContents* source) {
  // Never focus the page.  Not even after a crash.
  return false;
}

void OffscreenTab::CanDownload(const GURL& url,
                               const std::string& request_method,
                               base::OnceCallback<void(bool)> callback) {
  // Offscreen tab pages are not allowed to download files.
  std::move(callback).Run(false);
}

bool OffscreenTab::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Context menus should never be shown.  Do nothing, but indicate the context
  // menu was shown so that default implementation in libcontent does not
  // attempt to do so on its own.
  return true;
}

content::KeyboardEventProcessingResult OffscreenTab::PreHandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Intercept and silence all keyboard events before they can be sent to the
  // renderer.
  return content::KeyboardEventProcessingResult::HANDLED;
}

bool OffscreenTab::PreHandleGestureEvent(WebContents* source,
                                         const blink::WebGestureEvent& event) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Intercept and silence all gesture events before they can be sent to the
  // renderer.
  return true;
}

bool OffscreenTab::CanDragEnter(WebContents* source,
                                const content::DropData& data,
                                blink::DragOperationsMask operations_allowed) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Halt all drag attempts onto the page since there should be no direct user
  // interaction with it.
  return false;
}

bool OffscreenTab::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  // Disallow creating separate WebContentses.  The WebContents implementation
  // uses this to spawn new windows/tabs, which is also not allowed for
  // offscreen tabs.
  return true;
}

void OffscreenTab::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  auto* contents = WebContents::FromRenderFrameHost(requesting_frame);
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);

  if (in_fullscreen_mode())
    return;

  non_fullscreen_size_ =
      contents->GetRenderWidgetHostView()->GetViewBounds().size();
  if (contents->IsBeingCaptured() && !contents->GetPreferredSize().IsEmpty())
    contents->Resize(gfx::Rect(contents->GetPreferredSize()));
}

void OffscreenTab::ExitFullscreenModeForTab(WebContents* contents) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);

  if (!in_fullscreen_mode())
    return;

  contents->Resize(gfx::Rect(non_fullscreen_size_));
  non_fullscreen_size_ = gfx::Size();
}

bool OffscreenTab::IsFullscreenForTabOrPending(const WebContents* contents) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);
  return in_fullscreen_mode();
}

blink::mojom::DisplayMode OffscreenTab::GetDisplayMode(
    const WebContents* contents) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);
  return in_fullscreen_mode() ? blink::mojom::DisplayMode::kFullscreen
                              : blink::mojom::DisplayMode::kBrowser;
}

void OffscreenTab::RequestMediaAccessPermission(
    WebContents* contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                          blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
                          nullptr);
}

bool OffscreenTab::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  return false;
}

void OffscreenTab::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(offscreen_tab_web_contents_.get());
  if (!navigation_policy_->AllowNavigation(navigation_handle)) {
    DVLOG(2) << "Closing because NavigationPolicy disallowed "
             << "StartNavigation to " << navigation_handle->GetURL().spec();
    Close();
  }
}

void OffscreenTab::DieIfContentCaptureEnded() {
  DCHECK(offscreen_tab_web_contents_.get());

  if (content_capture_was_detected_) {
    if (!offscreen_tab_web_contents_->IsBeingCaptured()) {
      DVLOG(2) << "Capture of OffscreenTab content has stopped for start_url="
               << start_url_.spec();
      owner_->DestroyTab(this);
      return;  // |this| is no longer valid.
    } else {
      DVLOG(3) << "Capture of OffscreenTab content continues for start_url="
               << start_url_.spec();
    }
  } else if (offscreen_tab_web_contents_->IsBeingCaptured()) {
    DVLOG(2) << "Capture of OffscreenTab content has started for start_url="
             << start_url_.spec();
    content_capture_was_detected_ = true;
  } else if (base::TimeTicks::Now() - start_time_ > kMaxWaitForCapture) {
    // More than a minute has elapsed since this OffscreenTab was started and
    // content capture still hasn't started.  As a safety precaution, assume
    // that content capture is never going to start and die to free up
    // resources.
    LOG(WARNING) << "Capture of OffscreenTab content did not start "
                 << "within timeout for start_url=" << start_url_.spec();
    owner_->DestroyTab(this);
    return;  // |this| is no longer valid.
  }

  // Schedule the timer to check again in a second.
  capture_poll_timer_.Start(
      FROM_HERE, kPollInterval,
      base::BindOnce(&OffscreenTab::DieIfContentCaptureEnded,
                     base::Unretained(this)));
}

void OffscreenTab::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK(profile == otr_profile_);
  otr_profile_ = nullptr;
  owner_->DestroyTab(this);
}
