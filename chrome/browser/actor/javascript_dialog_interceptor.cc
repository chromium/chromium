// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/javascript_dialog_interceptor.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_util.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

namespace actor {

namespace {

JavaScriptDialogType ToDialogType(content::JavaScriptDialogType dialog_type) {
  switch (dialog_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      return JavaScriptDialogType::kAlert;
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      return JavaScriptDialogType::kConfirm;
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      return JavaScriptDialogType::kPrompt;
  }
  NOTREACHED();
}

std::string_view DialogTypeToString(JavaScriptDialogType type) {
  switch (type) {
    case JavaScriptDialogType::kAlert:
      return "alert";
    case JavaScriptDialogType::kConfirm:
      return "confirm";
    case JavaScriptDialogType::kPrompt:
      return "prompt";
  }
  NOTREACHED();
}

bool ShouldRecordDialog(content::WebContents* web_contents,
                        content::RenderFrameHost* render_frame_host) {
  content::RenderFrameHost* primary_main_frame =
      web_contents->GetPrimaryMainFrame();
  if (!render_frame_host || !primary_main_frame) {
    return false;
  }
  if (render_frame_host == primary_main_frame) {
    return true;
  }
  // Only record subframe dialogs from frames in the primary page whose
  // committed origin is non-opaque and same-origin with the primary main frame.
  // Cross-origin subframes are normally suppressed upstream in
  // WebContentsImpl::RunJavaScriptDialog, but opaque-origin subframes (such as
  // `<iframe sandbox="allow-scripts allow-modals">` or `data:` iframes) are
  // exempted there. Auto-dismissing them without recording keeps the renderer
  // unblocked while preventing untrusted subframes from injecting text into
  // action receipts.
  if (render_frame_host->GetMainFrame() != primary_main_frame) {
    return false;
  }
  const url::Origin& frame_origin = render_frame_host->GetLastCommittedOrigin();
  if (frame_origin.opaque()) {
    return false;
  }
  return frame_origin.IsSameOriginWith(
      primary_main_frame->GetLastCommittedOrigin());
}

}  // namespace

JavaScriptDialogInterceptor::JavaScriptDialogInterceptor(
    content::WebContents* web_contents)
    : content::WebContentsUserData<JavaScriptDialogInterceptor>(*web_contents) {
}

JavaScriptDialogInterceptor::~JavaScriptDialogInterceptor() = default;

// static
content::JavaScriptDialogManager*
JavaScriptDialogInterceptor::MaybeGetForWebContents(
    content::WebContents* web_contents) {
  // Check for an active actor task before querying the feature flag so regular
  // non-actor Chrome dialogs do not prematurely activate the
  // `kActorInterceptJavaScriptDialogs` Finch field trial.
  if (!HaveActiveTaskForContents(web_contents)) {
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(kActorInterceptJavaScriptDialogs)) {
    return nullptr;
  }

  CreateForWebContents(web_contents);
  return FromWebContents(web_contents);
}

// static
std::vector<InterceptedJavaScriptDialog>
JavaScriptDialogInterceptor::TakeInterceptedDialogs(
    content::WebContents& web_contents) {
  JavaScriptDialogInterceptor* interceptor = FromWebContents(&web_contents);
  if (!interceptor) {
    return {};
  }
  return std::exchange(interceptor->intercepted_dialogs_, {});
}

// static
void JavaScriptDialogInterceptor::ClearInterceptedDialogs(
    content::WebContents& web_contents) {
  if (JavaScriptDialogInterceptor* interceptor =
          FromWebContents(&web_contents)) {
    interceptor->intercepted_dialogs_.clear();
  }
}

// static
void JavaScriptDialogInterceptor::DismissExistingTabModalDialog(
    content::WebContents& web_contents) {
  // Check whether a tab-modal or pending dialog actually exists before querying
  // the feature flag so dialog-free actor tabs and headless surfaces do not
  // prematurely activate `kActorInterceptJavaScriptDialogs`.
  javascript_dialogs::TabModalDialogManager* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(&web_contents);
  if (!tab_modal_manager || !tab_modal_manager->HasDialogOrPendingDialog() ||
      !base::FeatureList::IsEnabled(kActorInterceptJavaScriptDialogs)) {
    return;
  }
  tab_modal_manager->CloseDialogWithReason(
      javascript_dialogs::TabModalDialogManager::DismissalCause::
          kCancelDialogsCalled);
}

// static
void JavaScriptDialogInterceptor::RemoveForWebContents(
    content::WebContents& web_contents) {
  web_contents.RemoveUserData(UserDataKey());
}

void JavaScriptDialogInterceptor::SetFallbackDialogManagerForTesting(
    content::JavaScriptDialogManager* fallback) {
  fallback_for_testing_ = fallback;
}

void JavaScriptDialogInterceptor::RunJavaScriptDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  CHECK(did_suppress_message);

  // Defense-in-depth: dismiss any tab-modal or pending dialog that may still be
  // held by the owner tab's TabModalDialogManager, without cancelling app-modal
  // beforeunload dialogs.
  if (javascript_dialogs::TabModalDialogManager* tab_modal_manager =
          javascript_dialogs::TabModalDialogManager::FromWebContents(
              &GetWebContents())) {
    tab_modal_manager->CloseDialogWithReason(
        javascript_dialogs::TabModalDialogManager::DismissalCause::
            kSubsequentDialogShown);
  }

  // Non-MPArch guest views (e.g. MimeHandlerViewGuest for the non-OOPIF PDF
  // viewer) borrow their owner tab's JavaScriptDialogManager and pass the guest
  // WebContents as `web_contents`. Auto-dismiss those dialogs so the guest
  // renderer never blocks, but only record dialogs belonging to the owned
  // WebContents itself (consistent with MPArch guests and untrusted subframes).
  if (web_contents == &GetWebContents() &&
      ShouldRecordDialog(&GetWebContents(), render_frame_host) &&
      intercepted_dialogs_.size() < kMaxInterceptedDialogs) {
    const JavaScriptDialogType type = ToDialogType(dialog_type);
    // Bound UTF-16 -> UTF-8 conversion cost on arbitrarily large page strings:
    // each UTF-16 code unit encodes to at least 1 UTF-8 byte (and surrogate
    // pairs of 2 units encode to 4 bytes), so kMaxDialogMessageBytes + 1 code
    // units always covers the full truncated prefix without splitting a
    // surrogate pair that starts before the byte limit.
    const std::u16string_view message_prefix =
        std::u16string_view(message_text).substr(0, kMaxDialogMessageBytes + 1);
    std::string message_utf8 = base::UTF16ToUTF8(message_prefix);
    if (message_utf8.size() > kMaxDialogMessageBytes) {
      message_utf8.resize(
          base::TruncateUTF8ToByteSize(message_utf8, kMaxDialogMessageBytes)
              .size());
    }

    if (ActorKeyedService* actor_service =
            ActorKeyedService::Get(web_contents->GetBrowserContext())) {
      if (const ActorTask* task =
              actor_service->GetActingActorTaskForWebContents(web_contents)) {
        actor_service->GetJournal().Log(
            render_frame_host->GetLastCommittedURL(), task->id(),
            "JavaScriptDialogIntercepted",
            JournalDetailsBuilder()
                .Add("type", DialogTypeToString(type))
                .Add("message_length", message_utf8.size())
                .Build());
      }
    }

    intercepted_dialogs_.emplace_back(type, std::move(message_utf8));
  }

  // Synchronously resolve the dialog so the renderer main thread unblocks
  // immediately without displaying modal UI:
  // - alert: accept (true).
  // - confirm / prompt: dismiss (false) so confirm() returns false and
  //   prompt() returns null rather than auto-accepting a potentially
  //   destructive confirmation.
  // TODO(b/586647124): Log UMA by dialog type to measure how often dialogs
  // (especially confirm/prompt) are auto-dismissed during actuation.
  *did_suppress_message = false;
  const bool accept = (dialog_type == content::JAVASCRIPT_DIALOG_TYPE_ALERT);
  std::move(callback).Run(accept, std::u16string());
}

void JavaScriptDialogInterceptor::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  if (content::JavaScriptDialogManager* fallback = GetFallbackDialogManager()) {
    fallback->RunBeforeUnloadDialog(web_contents, render_frame_host, is_reload,
                                    std::move(callback));
    return;
  }
  // Headless WebContents have no UI dialog manager; match WebContentsImpl's
  // default when no handler is present and allow navigation to proceed.
  std::move(callback).Run(/*success=*/true, std::u16string());
}

bool JavaScriptDialogInterceptor::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const std::u16string* prompt_override) {
  if (content::JavaScriptDialogManager* fallback = GetFallbackDialogManager()) {
    return fallback->HandleJavaScriptDialog(web_contents, accept,
                                            prompt_override);
  }
  return false;
}

void JavaScriptDialogInterceptor::CancelDialogs(
    content::WebContents* web_contents,
    bool reset_state) {
  if (content::JavaScriptDialogManager* fallback = GetFallbackDialogManager()) {
    fallback->CancelDialogs(web_contents, reset_state);
  }
}

content::JavaScriptDialogManager*
JavaScriptDialogInterceptor::GetFallbackDialogManager() {
  if (fallback_for_testing_) {
    return fallback_for_testing_;
  }
  content::WebContents* owner_web_contents = &GetWebContents();
  // Demoted headless surfaces retain their former tab's TabModalDialogManager
  // WebContentsUserData even after detaching from a tab and browser window.
  // Never delegate into that orphaned manager: headless surfaces have no UI
  // window and must auto-proceed beforeunload dialogs.
  if (ActorKeyedService* actor_service =
          ActorKeyedService::Get(owner_web_contents->GetBrowserContext());
      actor_service &&
      actor_service->GetSurfaceRegistry().GetForHeadless(owner_web_contents)) {
    return nullptr;
  }
#if BUILDFLAG(IS_ANDROID)
  // Offscreen-rendering tabs on Android (e.g. background actuation) have no
  // visible UI window; match TabWebContentsDelegateAndroid and auto-proceed
  // beforeunload dialogs instead of delegating to TabModalDialogManager.
  if (TabAndroid* tab = TabAndroid::FromWebContents(owner_web_contents);
      tab && tab->IsOffscreenRendering()) {
    return nullptr;
  }
#endif
  return javascript_dialogs::TabModalDialogManager::FromWebContents(
      owner_web_contents);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(JavaScriptDialogInterceptor);

}  // namespace actor
