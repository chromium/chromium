// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/glic_linkout_handler.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/glic_enums.mojom.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#endif

namespace critical_actions {

namespace {

// Returns the Glic turn ID to pass for `anchor_id` when opening a conversation
// from `source`, or std::nullopt to pass none.
std::optional<std::string> GetTurnIdForSource(
    std::string_view anchor_id,
    glic::mojom::InvocationSource source) {
  // An action's anchor is the turn where that action happened. The entry-menu
  // linkout is visit-level, and a visit can have several critical actions, so
  // no single turn belongs to it.
  if (source != glic::mojom::InvocationSource::
                    kHistoryPageCriticalActionChatLinkout ||
      anchor_id.empty()) {
    return std::nullopt;
  }
  // For Glic, the conversation anchor is the turn ID. Its length is already
  // limited when it is stored.
  return std::string(anchor_id);
}

#if BUILDFLAG(IS_ANDROID)
// Returns the Glic target to open a conversation on when the caller has no tab
// (e.g. HistoryActivity): the active tab of the most recently active tabbed
// browser window of `profile`, or a new tab in that window if it has none.
// Returns std::nullopt if `profile` has no such window.
// TODO(b/579834304): Consider targeting the window that launched the caller
// instead.
std::optional<glic::Target> GetTargetWithoutTab(Profile& profile) {
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(&profile);
  BrowserWindowInterface* browser =
      collection ? collection->FindTabbedBrowser() : nullptr;
  TabListInterface* tab_list =
      browser ? TabListInterface::From(browser) : nullptr;
  if (!tab_list) {
    return std::nullopt;
  }
  if (tabs::TabInterface* active_tab = tab_list->GetActiveTab()) {
    return glic::Target(*active_tab);
  }
  // Let Glic open the new tab, so that it treats it as a new surface and waits
  // for its navigation to commit.
  return glic::Target(glic::NewTab{.window = browser});
}
#endif

}  // namespace

// static
GlicLinkoutHandler* GlicLinkoutHandler::GetInstance() {
  static base::NoDestructor<GlicLinkoutHandler> instance;
  return instance.get();
}

GlicLinkoutHandler::GlicLinkoutHandler() = default;
GlicLinkoutHandler::~GlicLinkoutHandler() = default;

void GlicLinkoutHandler::OpenConversation(
    content::WebContents* web_contents,
    const CriticalActionEntry& entry,
    glic::mojom::InvocationSource source,
    base::OnceCallback<void(OpenConversationResult)> callback) {
  if (entry.conversation_id.empty()) {
    std::move(callback).Run(OpenConversationResult::kErrorInvalidActionEntry);
    return;
  }

  if (!web_contents) {
    std::move(callback).Run(OpenConversationResult::kErrorInternal);
    return;
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    std::move(callback).Run(OpenConversationResult::kErrorInternal);
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  OpenConversation(profile, tab, entry, source, std::move(callback));
}

void GlicLinkoutHandler::OpenConversation(
    Profile* profile,
    tabs::TabInterface* tab,
    const CriticalActionEntry& entry,
    glic::mojom::InvocationSource source,
    base::OnceCallback<void(OpenConversationResult)> callback) {
  if (entry.conversation_id.empty()) {
    std::move(callback).Run(OpenConversationResult::kErrorInvalidActionEntry);
    return;
  }

  glic::GlicKeyedService* glic_service =
      profile ? glic::GlicKeyedService::Get(profile) : nullptr;
  if (!profile || !glic::GlicEnabling::IsEnabledForProfile(profile) ||
      !glic_service) {
    std::move(callback).Run(OpenConversationResult::kErrorInternal);
    return;
  }

  // A conversation stored in `profile` must not be opened in another profile's
  // tab (e.g. an Incognito one). Refuse it rather than crash, and report it, as
  // it points to a caller bug.
  if (tab && tab->GetProfile() != profile) {
    base::debug::DumpWithoutCrashing();
    std::move(callback).Run(OpenConversationResult::kErrorInternal);
    return;
  }

  std::optional<glic::Target> target;
  if (tab) {
    target.emplace(*tab);
  } else {
#if BUILDFLAG(IS_ANDROID)
    target = GetTargetWithoutTab(*profile);
#endif
  }

  if (!target) {
    std::move(callback).Run(OpenConversationResult::kErrorInternal);
    return;
  }

  target->conversation = glic::ConversationId(
      entry.conversation_id,
      GetTurnIdForSource(entry.conversation_anchor_id, source));

  glic::GlicInvokeOptions options(std::move(*target), source);
  options.fre_completion_wait_mode = glic::FreCompletionWaitMode::kNever;
  options.supersede_if_in_progress = true;
  options.pin_on_bind = false;

  glic_service->Invoke(std::move(options));
  std::move(callback).Run(OpenConversationResult::kSuccess);
}

}  // namespace critical_actions
