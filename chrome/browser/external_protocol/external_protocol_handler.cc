// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include <stddef.h>

#include <utility>

#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/external_protocol/auto_launch_protocols_policy_handler.h"
#include "chrome/browser/external_protocol/constants.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/weak_document_ptr.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/navigation_interception/intercept_navigation_delegate.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/url_formatter/elide_url.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#endif

namespace {

// Anti-flood protection controls whether we accept requests for launching
// external protocols. Set to false each time an external protocol is requested,
// and set back to true on each user gesture, extension API call, and navigation
// to an external handler via bookmarks or the omnibox. This variable should
// only be accessed from the UI thread.
bool g_accept_requests = true;

ExternalProtocolHandler::Delegate* g_external_protocol_handler_delegate =
    nullptr;

constexpr auto kDeniedSchemes = base::MakeFixedFlatSet<std::string_view>({
    "afp",
    "data",
    "disk",
    "disks",
    // ShellExecuting file:///C:/WINDOWS/system32/notepad.exe will simply
    // execute the file specified!  Hopefully we won't see any "file" schemes
    // because we think of file:// URLs as handled URLs, but better to be safe
    // than to let an attacker format the user's hard drive.
    "file",
    "hcp",
    "ie.http",
    "javascript",
    "mk",
    "ms-help",
    "nntp",
    "res",
    "shell",
    "vbscript",
    // view-source is a special case in chrome. When it comes through an
    // iframe or a redirect, it looks like an external protocol, but we don't
    // want to shellexecute it.
    "view-source",
    "vnd.ms.radio",
});

void AddMessageToConsole(const content::WeakDocumentPtr& document,
                         blink::mojom::ConsoleMessageLevel level,
                         const std::string& message) {
  if (content::RenderFrameHost* rfh = document.AsRenderFrameHostIfValid())
    rfh->AddMessageToConsole(level, message);
}

#if !BUILDFLAG(IS_ANDROID)
// Functions enabling unit testing. Using a NULL delegate will use the default
// behavior; if a delegate is provided it will be used instead.
scoped_refptr<shell_integration::DefaultSchemeClientWorker> CreateShellWorker(
    const GURL& url,
    ExternalProtocolHandler::Delegate* delegate) {
  if (delegate)
    return delegate->CreateShellWorker(url);
  return base::MakeRefCounted<shell_integration::DefaultSchemeClientWorker>(
      url);
}
#endif

ExternalProtocolHandler::BlockState GetBlockStateWithDelegate(
    const std::string& scheme,
    const url::Origin* initiating_origin,
    ExternalProtocolHandler::Delegate* delegate,
    Profile* profile) {
  if (delegate)
    return delegate->GetBlockState(scheme, profile);
  return ExternalProtocolHandler::GetBlockState(scheme, initiating_origin,
                                                profile);
}

#if !BUILDFLAG(IS_ANDROID)
void RunExternalProtocolDialogWithDelegate(
    const GURL& url,
    content::WebContents* web_contents,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    bool is_in_fenced_frame_tree,
    const std::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document,
    const std::u16string& program_name,
    ExternalProtocolHandler::Delegate* delegate) {
  DCHECK(web_contents);
  if (delegate) {
    delegate->RunExternalProtocolDialog(url, web_contents, page_transition,
                                        has_user_gesture, initiating_origin,
                                        program_name);
    return;
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // If the Shell does not have a registered name for the protocol,
  // attempting to invoke the protocol will fail.
  if (program_name.empty()) {
    AddMessageToConsole(
        initiator_document, blink::mojom::ConsoleMessageLevel::kError,
        "Failed to launch '" + url.possibly_invalid_spec() +
            "' because the scheme does not have a registered handler.");
    return;
  }
#endif

  ExternalProtocolHandler::RunExternalProtocolDialog(
      url, web_contents, page_transition, has_user_gesture,
      is_in_fenced_frame_tree, initiating_origin, std::move(initiator_document),
      program_name);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void LaunchUrlWithoutSecurityCheckWithDelegate(
    const GURL& url,
    content::WebContents* web_contents,
    content::WeakDocumentPtr initiator_document,
    ExternalProtocolHandler::Delegate* delegate) {
  if (delegate) {
    delegate->LaunchUrlWithoutSecurityCheck(url, web_contents);
    return;
  }

  // |web_contents| is only passed in to find browser context. Do not assume
  // that the external protocol request came from the main frame.
  if (!web_contents)
    return;

  AddMessageToConsole(
      initiator_document, blink::mojom::ConsoleMessageLevel::kInfo,
      "Launched external handler for '" + url.possibly_invalid_spec() + "'.");

  platform_util::OpenExternal(
#if BUILDFLAG(IS_CHROMEOS_ASH)
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
#endif
      url);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // If the protocol navigation occurs in a new tab, close it.
  // Avoid calling CloseContents if the tab is not in this browser's tab strip
  // model; this can happen if the protocol was initiated by something
  // internal to Chrome.
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (browser && web_contents->GetController().IsInitialNavigation() &&
      browser->tab_strip_model()->count() > 1 &&
      browser->tab_strip_model()->GetIndexOfWebContents(web_contents) !=
          TabStripModel::kNoTab) {
    // Defer destruction of `WebContents` to avoid synchronously destroying
    // NavigationURLLoader(Impl) here. See https://issues.chromium.org/361600654
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&content::WebContents::Close,
                                  web_contents->GetWeakPtr()));
  }
#endif
}

#if !BUILDFLAG(IS_ANDROID)
// When we are about to launch a URL with the default OS level application, we
// check if the external application will be us. If it is we just ignore the
// request.
void OnDefaultSchemeClientWorkerFinished(
    const GURL& escaped_url,
    content::WebContents::Getter web_contents_getter,
    bool prompt_user,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    bool is_in_fenced_frame_tree,
    const std::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document,
    ExternalProtocolHandler::Delegate* delegate,
    shell_integration::DefaultWebClientState state,
    const std::u16string& program_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (delegate)
    delegate->FinishedProcessingCheck();

  content::WebContents* web_contents = web_contents_getter.Run();

  // The default handler is hidden if it is Chrome itself, as nothing will
  // happen if it is selected (since this is invoked by the external protocol
  // handling flow).
  bool chrome_is_default_handler = state == shell_integration::IS_DEFAULT;

  // On ChromeOS, Click to Call is integrated into the external protocol dialog.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (web_contents && ShouldOfferClickToCallForURL(
                          web_contents->GetBrowserContext(), escaped_url)) {
    // Handle tel links by opening the Click to Call dialog. This will call back
    // into LaunchUrlWithoutSecurityCheck if the user selects a system handler.
    ClickToCallUiController::ShowDialog(
        web_contents, initiating_origin, std::move(initiator_document),
        escaped_url, chrome_is_default_handler, program_name);
    return;
  }
#endif

  if (chrome_is_default_handler) {
    if (delegate)
      delegate->BlockRequest();
    return;
  }

  // If we get here, either we are not the default or we cannot work out
  // what the default is, so we proceed.
  if (prompt_user) {
    // Never prompt the user without a web_contents.
    if (!web_contents) {
      return;
    }

    // Anchor to the outermost WebContents, for e.g. embedded <webview>s.
    web_contents = web_contents->GetOutermostWebContents();

    // Skip if the WebContents instance is not prepared to show a dialog.
    if (!web_modal::WebContentsModalDialogManager::FromWebContents(
            web_contents)) {
      LOG(ERROR) << "Skipping ExternalProtocolDialog"
                 << ", escaped_url=" << escaped_url.possibly_invalid_spec()
                 << ", initiating_origin="
                 << url_formatter::FormatOriginForSecurityDisplay(
                        initiating_origin.value_or(url::Origin()))
                 << ", web_contents?" << !!web_contents << ", browser?"
                 << (web_contents && chrome::FindBrowserWithTab(web_contents));
      base::debug::DumpWithoutCrashing();
      return;
    }

    // Ask the user if they want to allow the protocol. This will call
    // LaunchUrlWithoutSecurityCheck if the user decides to accept the
    // protocol.
    RunExternalProtocolDialogWithDelegate(
        escaped_url, web_contents, page_transition, has_user_gesture,
        is_in_fenced_frame_tree, initiating_origin,
        std::move(initiator_document), program_name, delegate);
    return;
  }

  LaunchUrlWithoutSecurityCheckWithDelegate(
      escaped_url, web_contents, std::move(initiator_document), delegate);
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool IsSchemeOriginPairAllowedByPolicy(const std::string& scheme,
                                       const url::Origin* initiating_origin,
                                       PrefService* prefs) {
  if (!initiating_origin)
    return false;

  const base::Value::List& exempted_protocols =
      prefs->GetList(prefs::kAutoLaunchProtocolsFromOrigins);

  const base::Value::List* origin_patterns = nullptr;
  for (const base::Value& entry : exempted_protocols) {
    const base::Value::Dict& protocol_origins_map = entry.GetDict();
    const std::string* protocol = protocol_origins_map.FindString(
        policy::external_protocol::kProtocolNameKey);
    DCHECK(protocol);
    if (*protocol == scheme) {
      origin_patterns = protocol_origins_map.FindList(
          policy::external_protocol::kOriginListKey);
      break;
    }
  }
  if (!origin_patterns)
    return false;

  url_matcher::URLMatcher matcher;
  base::MatcherStringPattern::ID id(0);
  url_matcher::util::AddFilters(&matcher, true /* allowed */, &id,
                                *origin_patterns);

  auto matching_set = matcher.MatchURL(initiating_origin->GetURL());
  return !matching_set.empty();
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(LoggedScheme)
enum class LoggedScheme {
  OTHER = 0,
  SEARCH_MS = 1,
  SEARCH = 2,
  MAILTO = 3,
  MICROSOFT_EDGE = 4,
  kMaxValue = MICROSOFT_EDGE,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/permissions/enums.xml:ExternalProtocolScheme)

void LogRequestForScheme(const std::string& scheme) {
  constexpr auto kSchemeToBucket =
      base::MakeFixedFlatMap<std::string_view, LoggedScheme>(
          {{"search-ms", LoggedScheme::SEARCH_MS},
           {"search", LoggedScheme::SEARCH},
           {"mailto", LoggedScheme::MAILTO},
           {"microsoft-edge", LoggedScheme::MICROSOFT_EDGE}});
  static_assert(kSchemeToBucket.size() ==
                static_cast<size_t>(LoggedScheme::kMaxValue));
  auto iterator = kSchemeToBucket.find(scheme);
  LoggedScheme scheme_bucket = iterator != kSchemeToBucket.end()
                                   ? iterator->second
                                   : LoggedScheme::OTHER;
  base::UmaHistogramEnumeration("BrowserDialogs.ExternalProtocol.Scheme",
                                scheme_bucket);
}

}  // namespace

const char ExternalProtocolHandler::kBlockStateMetric[] =
    "BrowserDialogs.ExternalProtocol.BlockState";
const char ExternalProtocolHandler::kHandleStateMetric[] =
    "BrowserDialogs.ExternalProtocol.HandleState";

// static
void ExternalProtocolHandler::SetDelegateForTesting(Delegate* delegate) {
  g_external_protocol_handler_delegate = delegate;
}

bool ExternalProtocolHandler::MayRememberAllowDecisionsForThisOrigin(
    const url::Origin* initiating_origin) {
  return initiating_origin &&
         network::IsOriginPotentiallyTrustworthy(*initiating_origin);
}

// static.
ExternalProtocolHandler::BlockState ExternalProtocolHandler::GetBlockState(
    const std::string& scheme,
    const url::Origin* initiating_origin,
    Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  LogRequestForScheme(scheme);

  // If we are being flooded with requests, block the request.
  if (!g_accept_requests)
    return BLOCK;

  if (scheme.length() == 1) {
    // We have a URL that looks something like:
    //   C:/WINDOWS/system32/notepad.exe
    // ShellExecuting this URL will cause the specified program to be executed.
    return BLOCK;
  }

  // Always block the hard-coded denied schemes.
  if (kDeniedSchemes.contains(scheme)) {
    base::UmaHistogramEnumeration(kBlockStateMetric,
                                  BlockStateMetric::kDeniedDefault);
    return BLOCK;
  }

  // The mailto scheme is allowed explicitly because of its ubiquity on the web
  // and because every platform provides a default handler for it.
  if (scheme == "mailto") {
    base::UmaHistogramEnumeration(kBlockStateMetric,
                                  BlockStateMetric::kAllowedDefaultMail);
    return DONT_BLOCK;
  }

  PrefService* profile_prefs = profile->GetPrefs();
  if (profile_prefs) {  // May be NULL during testing.
    if (IsSchemeOriginPairAllowedByPolicy(scheme, initiating_origin,
                                          profile_prefs)) {
      base::UmaHistogramEnumeration(
          kBlockStateMetric, BlockStateMetric::kAllowedByEnterprisePolicy);
      return DONT_BLOCK;
    }

    if (MayRememberAllowDecisionsForThisOrigin(initiating_origin)) {
      // Check if there is a matching {Origin+Protocol} pair exemption:
      const base::Value::Dict& allowed_origin_protocol_pairs =
          profile_prefs->GetDict(
              prefs::kProtocolHandlerPerOriginAllowedProtocols);
      const base::Value::Dict* allowed_protocols_for_origin =
          allowed_origin_protocol_pairs.FindDict(
              initiating_origin->Serialize());
      if (allowed_protocols_for_origin) {
        std::optional<bool> allow =
            allowed_protocols_for_origin->FindBool(scheme);
        if (allow.has_value() && allow.value()) {
          base::UmaHistogramEnumeration(kBlockStateMetric,
                                        BlockStateMetric::kAllowedByPreference);
          return DONT_BLOCK;
        }
      }
    }
  }

  base::UmaHistogramEnumeration(kBlockStateMetric, BlockStateMetric::kPrompt);
  return UNKNOWN;
}

// static
// This is only called when the "remember" check box is selected from the
// External Protocol Prompt dialog, and that check box is only shown when there
// is a non-empty, potentially-trustworthy initiating origin.
void ExternalProtocolHandler::SetBlockState(
    const std::string& scheme,
    const url::Origin& initiating_origin,
    BlockState state,
    Profile* profile) {
  // Setting the state to BLOCK is no longer supported through the UI.
  DCHECK_NE(state, BLOCK);

  // Set in the stored prefs.
  if (MayRememberAllowDecisionsForThisOrigin(&initiating_origin)) {
    PrefService* profile_prefs = profile->GetPrefs();
    if (profile_prefs) {  // May be NULL during testing.
      ScopedDictPrefUpdate update_allowed_origin_protocol_pairs(
          profile_prefs, prefs::kProtocolHandlerPerOriginAllowedProtocols);

      const std::string serialized_origin = initiating_origin.Serialize();
      base::Value::Dict* allowed_protocols_for_origin =
          update_allowed_origin_protocol_pairs->FindDict(serialized_origin);
      if (!allowed_protocols_for_origin) {
        update_allowed_origin_protocol_pairs->Set(serialized_origin,
                                                  base::Value::Dict());
        allowed_protocols_for_origin =
            update_allowed_origin_protocol_pairs->FindDict(serialized_origin);
      }
      if (state == DONT_BLOCK) {
        allowed_protocols_for_origin->Set(scheme, true);
      } else {
        allowed_protocols_for_origin->Remove(scheme);
        if (allowed_protocols_for_origin->empty()) {
          update_allowed_origin_protocol_pairs->Remove(serialized_origin);
        }
      }
    }
  }

  if (g_external_protocol_handler_delegate) {
    g_external_protocol_handler_delegate->OnSetBlockState(
        scheme, initiating_origin, state);
  }
}

// static
void ExternalProtocolHandler::LaunchUrl(
    const GURL& url,
    content::WebContents::Getter web_contents_getter,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    bool is_in_fenced_frame_tree,
    const std::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document
#if BUILDFLAG(IS_ANDROID)
    ,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory
#endif
) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Disable anti-flood protection if the user is invoking a bookmark or
  // navigating directly using the omnibox.
  if (!g_accept_requests &&
      (PageTransitionCoreTypeIs(page_transition,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
       PageTransitionCoreTypeIs(page_transition, ui::PAGE_TRANSITION_TYPED))) {
    g_accept_requests = true;
  }

  // Escape the input scheme to be sure that the command does not
  // have parameters unexpected by the external program.
  // TODO(mgiuca): This essentially amounts to "remove illegal characters from
  // the URL", something that probably should be done by the GURL constructor
  // itself. The GURL constructor does do it in some cases (e.g., mailto) but
  // not in general. https://crbug.com/788244.
  std::string escaped_url_string = base::EscapeExternalHandlerValue(url.spec());
  GURL escaped_url(escaped_url_string);

  content::WebContents* web_contents = web_contents_getter.Run();
  Profile* profile = nullptr;
  if (web_contents)  // Maybe NULL during testing.
    profile = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  BlockState block_state = GetBlockStateWithDelegate(
      escaped_url.scheme(), base::OptionalToPtr(initiating_origin),
      g_external_protocol_handler_delegate, profile);
  if (block_state == BLOCK) {
    AddMessageToConsole(
        initiator_document, blink::mojom::ConsoleMessageLevel::kError,
        "Not allowed to launch '" + url.possibly_invalid_spec() + "'" +
            (g_accept_requests ? "." : " because a user gesture is required."));

    if (g_external_protocol_handler_delegate)
      g_external_protocol_handler_delegate->BlockRequest();
    return;
  }

  g_accept_requests = false;

  // Shell integration code below doesn't work on Android - default handler
  // checks are instead handled through the InterceptNavigationDelegate. See
  // ExternalNavigationHandler.java.
  // The Origin is used for security checks, not for displaying to the user, so
  // the precursor origin should not be used.
  // Also, a protocol dialog isn't used on Android.
#if BUILDFLAG(IS_ANDROID)
  navigation_interception::InterceptNavigationDelegate* delegate =
      navigation_interception::InterceptNavigationDelegate::Get(web_contents);
  if (delegate) {
    delegate->HandleSubframeExternalProtocol(escaped_url, page_transition,
                                             has_user_gesture,
                                             initiating_origin, out_factory);
  }
  return;
#else
  std::optional<url::Origin> initiating_origin_or_precursor;
  if (initiating_origin) {
    // Transform the initiating origin to its precursor origin if it is
    // opaque. |initiating_origin| is shown in the UI to attribute the external
    // protocol request to a particular site, and showing an opaque origin isn't
    // useful.
    if (initiating_origin->opaque()) {
      initiating_origin_or_precursor = url::Origin::Create(
          initiating_origin->GetTupleOrPrecursorTupleIfOpaque().GetURL());
    } else {
      initiating_origin_or_precursor = initiating_origin;
    }
  }

  // The worker creates tasks with references to itself and puts them into
  // message loops.
  shell_integration::DefaultSchemeHandlerWorkerCallback callback =
      base::BindOnce(&OnDefaultSchemeClientWorkerFinished, escaped_url,
                     std::move(web_contents_getter), block_state == UNKNOWN,
                     page_transition, has_user_gesture, is_in_fenced_frame_tree,
                     initiating_origin_or_precursor,
                     std::move(initiator_document),
                     g_external_protocol_handler_delegate);

  // Start the check process running. This will send tasks to a worker task
  // runner and when the answer is known will send the result back to
  // OnDefaultSchemeClientWorkerFinished().
  CreateShellWorker(escaped_url, g_external_protocol_handler_delegate)
      ->StartCheckIsDefaultAndGetDefaultClientName(std::move(callback));
#endif
}

// static
void ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
    const GURL& url,
    content::WebContents* web_contents,
    content::WeakDocumentPtr initiator_document) {
  // Escape the input scheme to be sure that the command does not
  // have parameters unexpected by the external program. The url passed in the
  // |url| parameter might already be escaped but the EscapeExternalHandlerValue
  // is idempotent so it is safe to apply it again.
  // TODO(crbug.com/40551459): This essentially amounts to "remove illegal
  // characters from the URL", something that probably should be done by the
  // GURL constructor itself.
  std::string escaped_url_string = base::EscapeExternalHandlerValue(url.spec());
  GURL escaped_url(escaped_url_string);

  LaunchUrlWithoutSecurityCheckWithDelegate(
      escaped_url, web_contents, std::move(initiator_document),
      g_external_protocol_handler_delegate);
}

// static
void ExternalProtocolHandler::PermitLaunchUrl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  g_accept_requests = true;
}

// static
void ExternalProtocolHandler::RecordHandleStateMetrics(bool checkbox_selected,
                                                       BlockState block_state) {
  HandleState handle_state = DONT_LAUNCH;
  switch (block_state) {
    case DONT_BLOCK:
      handle_state = checkbox_selected ? CHECKED_LAUNCH : LAUNCH;
      break;
    case BLOCK:
      handle_state =
          checkbox_selected ? CHECKED_DONT_LAUNCH_DEPRECATED : DONT_LAUNCH;
      break;
    case UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  DCHECK_NE(CHECKED_DONT_LAUNCH_DEPRECATED, handle_state);
  UMA_HISTOGRAM_ENUMERATION(kHandleStateMetric, handle_state,
                            HANDLE_STATE_LAST);
}

// static
void ExternalProtocolHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      prefs::kProtocolHandlerPerOriginAllowedProtocols);

  registry->RegisterListPref(prefs::kAutoLaunchProtocolsFromOrigins);
}

// static
void ExternalProtocolHandler::ClearData(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  prefs->ClearPref(prefs::kProtocolHandlerPerOriginAllowedProtocols);
}
