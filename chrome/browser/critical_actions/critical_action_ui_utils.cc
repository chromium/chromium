// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/critical_action_ui_utils.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/browser/critical_actions/glic_linkout_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/common/glic_enums.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace critical_actions {

namespace {

// LINT.IfChange(OpenConversationSourceToHistogramSuffix)
std::string_view OpenConversationSourceToHistogramSuffix(
    OpenConversationSource source) {
  switch (source) {
    case OpenConversationSource::kEntryMenu:
      return "EntryMenu";
    case OpenConversationSource::kCriticalActionRow:
      return "CriticalActionRow";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/histograms.xml:OpenConversationSource)

// Returns the Glic invocation source to open a conversation with for a request
// from `source`. `GlicLinkoutHandler` derives from it whether the conversation
// is opened at the entry's anchored turn: only
// `kHistoryPageCriticalActionChatLinkout` targets the turn, while the entry
// menu's visit-level `kHistoryPageChatLinkout` opens the conversation at its
// default position.
glic::mojom::InvocationSource GetGlicInvocationSource(
    OpenConversationSource source) {
  switch (source) {
    case OpenConversationSource::kEntryMenu:
      return glic::mojom::InvocationSource::kHistoryPageChatLinkout;
    case OpenConversationSource::kCriticalActionRow:
      // `kCriticalActionRow` is meant for a per-row entry point that is only
      // offered when action-level chat linkouts are enabled, but the caller
      // (e.g. the renderer) is not trusted to enforce that. When the flag is
      // off, fall back to the entry-menu behavior: still open the conversation
      // (like "Go to Gemini chat", which is also only gated in the UI), but
      // without targeting the anchored turn and without the new invocation
      // source, so neither leaks out from behind the flag.
      if (!features::IsActionLevelChatLinkoutsEnabled()) {
        return glic::mojom::InvocationSource::kHistoryPageChatLinkout;
      }
      return glic::mojom::InvocationSource::
          kHistoryPageCriticalActionChatLinkout;
  }
  NOTREACHED();
}

void RecordOpenConversationResult(OpenConversationSource source,
                                  OpenConversationResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.OpenConversationResult.",
                    OpenConversationSourceToHistogramSuffix(source)}),
      result);
}

void RecordOpenConversationHasAnchor(bool has_anchor) {
  base::UmaHistogramBoolean(
      "CriticalActions.OpenConversation.HasConversationAnchor", has_anchor);
}

void FinishOpenConversation(OpenConversationSource source,
                            OpenConversationCallback callback,
                            OpenConversationResult result) {
  RecordOpenConversationResult(source, result);
  std::move(callback).Run(result);
}

// Opens the conversation of a critical action entry, retrieved from
// `CriticalActionService`, with the given Glic invocation source, and invokes
// the trailing callback with the result.
using OpenEntryCallback = base::OnceCallback<void(const CriticalActionEntry&,
                                                  glic::mojom::InvocationSource,
                                                  OpenConversationCallback)>;

void OnCriticalActionRetrieved(OpenConversationSource source,
                               OpenEntryCallback open_entry,
                               OpenConversationCallback callback,
                               std::optional<CriticalActionEntry> entry) {
  if (!entry) {
    FinishOpenConversation(source, std::move(callback),
                           OpenConversationResult::kErrorInvalidActionEntry);
    return;
  }

  // Anchors are only stored while action-level chat linkouts are enabled, and
  // only the per-row "Open chat" button opens the conversation at the anchored
  // turn, so only record whether one exists for that source while the flag is
  // on.
  if (source == OpenConversationSource::kCriticalActionRow &&
      features::IsActionLevelChatLinkoutsEnabled()) {
    RecordOpenConversationHasAnchor(!entry->conversation_anchor_id.empty());
  }

  std::move(open_entry)
      .Run(
          *entry, GetGlicInvocationSource(source),
          base::BindOnce(&FinishOpenConversation, source, std::move(callback)));
}

// Looks up `critical_action_id` in `profile`'s `CriticalActionService` and, if
// found, opens its conversation with `open_entry`. Shared by
// `OpenCriticalActionConversation()` and
// `OpenCriticalActionConversationForTab()`, which only differ in where the
// conversation is opened.
void LookUpAndOpenConversation(Profile* profile,
                               std::string_view critical_action_id,
                               OpenConversationSource source,
                               OpenEntryCallback open_entry,
                               OpenConversationCallback callback) {
  if (critical_action_id.empty()) {
    FinishOpenConversation(source, std::move(callback),
                           OpenConversationResult::kErrorInvalidActionEntry);
    return;
  }

  CriticalActionService* critical_action_service =
      profile ? CriticalActionFactory::GetForProfile(profile) : nullptr;
  if (!critical_action_service) {
    FinishOpenConversation(source, std::move(callback),
                           OpenConversationResult::kErrorInternal);
    return;
  }

  critical_action_service->GetCriticalAction(
      critical_action_id,
      base::BindOnce(&OnCriticalActionRetrieved, source, std::move(open_entry),
                     std::move(callback)));
}

void OpenEntryOnWebContents(base::WeakPtr<content::WebContents> web_contents,
                            const CriticalActionEntry& entry,
                            glic::mojom::InvocationSource invocation_source,
                            OpenConversationCallback callback) {
  GlicLinkoutHandler::GetInstance()->OpenConversation(
      web_contents.get(), entry, invocation_source, std::move(callback));
}

void OpenEntryOnTab(base::WeakPtr<Profile> profile,
                    bool tab_requested,
                    base::WeakPtr<tabs::TabInterface> tab,
                    const CriticalActionEntry& entry,
                    glic::mojom::InvocationSource invocation_source,
                    OpenConversationCallback callback) {
  // Don't fall back to another tab if the caller's tab was destroyed during
  // the lookup.
  if (tab_requested && !tab) {
    std::move(callback).Run(OpenConversationResult::kErrorInternal);
    return;
  }
  GlicLinkoutHandler::GetInstance()->OpenConversation(
      profile.get(), tab.get(), entry, invocation_source, std::move(callback));
}

std::string GetPasswordManagerLinkoutUrl(const GURL& url) {
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty()) {
    domain = std::string(url.host());
  }
  if (!domain.empty()) {
    return base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/", domain});
  }
  return chrome::kChromeUIPasswordManagerURL;
}

std::string GetSiteDetailsLinkoutUrl(const GURL& page_url) {
  const url::Origin origin = url::Origin::Create(page_url);
  // An opaque origin serialises to the literal string "null", which would
  // produce a site details page for a site that does not exist. Fall back to
  // the settings root instead.
  if (origin.opaque()) {
    return chrome::kChromeUISettingsURL;
  }
  return net::AppendQueryParameter(
             GURL(base::StrCat(
                 {chrome::kChromeUISettingsURL, chrome::kSiteDetailsSubpage})),
             "site", origin.Serialize())
      .spec();
}

std::string GetContactInfoLinkoutUrl() {
#if BUILDFLAG(IS_ANDROID)
  // Android settings (SettingsFragmentRegistry) has no /contactInfo route;
  // /addresses opens the contact info page there.
  constexpr std::string_view kSubPage = chrome::kAddressesSubPage;
#else
  constexpr std::string_view kSubPage = chrome::kContactInfoSubPage;
#endif
  return base::StrCat({chrome::kChromeUISettingsURL, kSubPage});
}

}  // namespace

std::optional<std::string> GetCriticalActionLinkoutUrl(ActionType action_type,
                                                       const GURL& page_url) {
  switch (action_type) {
    case ActionType::kCredentialAccess:
    case ActionType::kGooglePasswordManager:
    case ActionType::kFederatedLogin:
    case ActionType::kCredentialsOtp:
      return GetPasswordManagerLinkoutUrl(page_url);
    case ActionType::kFormFill:
    case ActionType::kFormFillContactInfo:
      return GetContactInfoLinkoutUrl();
    case ActionType::kFormFillPayments:
      return base::StrCat(
          {chrome::kChromeUISettingsURL, chrome::kPaymentsSubPage});
    case ActionType::kDownload:
      return chrome::kChromeUIDownloadsURL;
    case ActionType::kSettingChange:
      return GetSiteDetailsLinkoutUrl(page_url);
    case ActionType::kWebMcpTool:
      // WebMCP tool calls have no management surface to link to. The visit
      // row itself already links to the page.
      return std::nullopt;
    case ActionType::kUnknown:
      return chrome::kChromeUISettingsURL;
  }
}

void OpenCriticalActionConversation(Profile* profile,
                                    content::WebContents* web_contents,
                                    std::string_view critical_action_id,
                                    OpenConversationSource source,
                                    OpenConversationCallback callback) {
  if (profile && web_contents) {
    CHECK_EQ(Profile::FromBrowserContext(web_contents->GetBrowserContext()),
             profile);
  }
  LookUpAndOpenConversation(
      profile, critical_action_id, source,
      base::BindOnce(&OpenEntryOnWebContents,
                     web_contents ? web_contents->GetWeakPtr() : nullptr),
      std::move(callback));
}

void OpenCriticalActionConversationForTab(Profile* profile,
                                          tabs::TabInterface* tab,
                                          std::string_view critical_action_id,
                                          OpenConversationSource source,
                                          OpenConversationCallback callback) {
  // Unlike the WebContents variant's callers, which take the profile and the
  // WebContents from the same WebUI, callers of this variant may get `tab` from
  // a supplier that doesn't guarantee it belongs to `profile` (e.g. the Android
  // history UI can hand over an Incognito tab). Refuse such a tab rather than
  // open a conversation stored in `profile` in it, and report it, as it points
  // to a caller bug.
  if (profile && tab && tab->GetProfile() != profile) {
    base::debug::DumpWithoutCrashing();
    FinishOpenConversation(source, std::move(callback),
                           OpenConversationResult::kErrorInternal);
    return;
  }
  LookUpAndOpenConversation(
      profile, critical_action_id, source,
      base::BindOnce(&OpenEntryOnTab, profile ? profile->GetWeakPtr() : nullptr,
                     /*tab_requested=*/tab != nullptr,
                     tab ? tab->GetWeakPtr() : nullptr),
      std::move(callback));
}

}  // namespace critical_actions
