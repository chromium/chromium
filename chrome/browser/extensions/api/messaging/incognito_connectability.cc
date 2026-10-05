// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_spec.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

using InfoBarResponseCallback =
    base::OnceCallback<void(IncognitoConnectability::ScopedAlertTracker::Mode)>;

struct ShowInfoBarResult {
  // Null if no infobar was shown.
  raw_ptr<infobars::InfoBar> infobar = nullptr;
  // Whether infobars::BrowserInfoBarManager created the infobar.
  bool is_migrated = false;
};

// Shows the prompt through the centralized framework when migrated, else
// through the legacy delegate.
ShowInfoBarResult ShowInfoBar(content::WebContents* web_contents,
                              infobars::ContentInfoBarManager* infobar_manager,
                              const std::u16string& message,
                              InfoBarResponseCallback callback) {
  // BrowserInfoBarManager only exists for !is_android, and
  // IsInfoBarMigrated() is false on Android anyway, so keep the migrated
  // branch out of the Android build entirely rather than relying on the
  // runtime check alone.
#if !BUILDFLAG(IS_ANDROID)
  if (infobars::IsInfoBarMigrated(
          infobars::InfoBarDelegate::
              INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE)) {
    auto* browser_infobar_manager =
        infobars::BrowserInfoBarManager::From(g_browser_process);
    CHECK(browser_infobar_manager);
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_contents);

    // TODO(https://crbug.com/523212830): Update InfoBar manager to accept
    // arbitrary web contents.
    if (!tab) {
      return ShowInfoBarResult{
          .infobar = IncognitoConnectabilityInfoBarDelegate::Create(
              infobar_manager, message, std::move(callback)),
          .is_migrated = false};
    }

    auto split = base::SplitOnceCallback(std::move(callback));
    infobars::InfoBarShowParams params;
    params.message_text = message;
    // ALWAYS_ALLOW maps to kAccepted and ALWAYS_DENY to kCancelled; any
    // other terminal result means the infobar went away unanswered.
    //
    // Unlike the legacy delegate, which answers from its destructor, this
    // runs on dismissal before the infobar is removed from the manager. A
    // Query() callback that synchronously re-queries the same
    // extension/origin on the same tab is therefore rejected as a duplicate
    // infobar and denied without a prompt; asynchronous retries prompt again.
    params.result_callback = base::BindRepeating(
        [](InfoBarResponseCallback& callback, content::WebContents*,
           infobars::InfoBarResult result) {
          if (!callback) {
            return;
          }
          auto mode = IncognitoConnectability::ScopedAlertTracker::INTERACTIVE;
          if (result == infobars::InfoBarResult::kAccepted) {
            mode = IncognitoConnectability::ScopedAlertTracker::ALWAYS_ALLOW;
          } else if (result == infobars::InfoBarResult::kCancelled) {
            mode = IncognitoConnectability::ScopedAlertTracker::ALWAYS_DENY;
          }
          std::move(callback).Run(mode);
        },
        base::OwnedRef(std::move(split.first)));
    infobars::InfoBar* infobar = browser_infobar_manager->Show(
        tab,
        infobars::InfoBarDelegate::INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE,
        std::move(params));
    if (!infobar) {
      // Nothing was shown. Answer this tab's queries as unanswered, the
      // same way the legacy delegate's destructor would have, but post it:
      // running it synchronously would re-enter OnInteractiveResponse()
      // and erase the PendingOrigin entry the caller (Query()) is still
      // holding a reference into, before that caller has finished writing
      // through it.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(split.second),
              IncognitoConnectability::ScopedAlertTracker::INTERACTIVE));
    }
    return ShowInfoBarResult{.infobar = infobar, .is_migrated = true};
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return ShowInfoBarResult{
      .infobar = IncognitoConnectabilityInfoBarDelegate::Create(
          infobar_manager, message, std::move(callback)),
      .is_migrated = false};
}

IncognitoConnectability::ScopedAlertTracker::Mode g_alert_mode =
    IncognitoConnectability::ScopedAlertTracker::INTERACTIVE;
int g_alert_count = 0;

}  // namespace

IncognitoConnectability::ScopedAlertTracker::ScopedAlertTracker(Mode mode)
    : last_checked_invocation_count_(g_alert_count) {
  CHECK_EQ(INTERACTIVE, g_alert_mode, base::NotFatalUntil::M161);
  CHECK_NE(INTERACTIVE, mode, base::NotFatalUntil::M161);
  g_alert_mode = mode;
}

IncognitoConnectability::ScopedAlertTracker::~ScopedAlertTracker() {
  CHECK_NE(INTERACTIVE, g_alert_mode, base::NotFatalUntil::M161);
  g_alert_mode = INTERACTIVE;
}

int IncognitoConnectability::ScopedAlertTracker::GetAndResetAlertCount() {
  int result = g_alert_count - last_checked_invocation_count_;
  last_checked_invocation_count_ = g_alert_count;
  return result;
}

IncognitoConnectability::IncognitoConnectability(
    content::BrowserContext* context) {
  CHECK(context->IsOffTheRecord());
}

IncognitoConnectability::~IncognitoConnectability() = default;

// static
IncognitoConnectability* IncognitoConnectability::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<IncognitoConnectability>::Get(context);
}

void IncognitoConnectability::Query(const Extension* extension,
                                    content::WebContents* web_contents,
                                    const GURL& url,
                                    base::OnceCallback<void(bool)> callback) {
  GURL origin = url.DeprecatedGetOriginAsURL();
  if (origin.is_empty()) {
    std::move(callback).Run(false);
    return;
  }

  if (IsInMap(extension, origin, allowed_origins_)) {
    std::move(callback).Run(true);
    return;
  }

  if (IsInMap(extension, origin, disallowed_origins_)) {
    std::move(callback).Run(false);
    return;
  }

  PendingOrigin& pending_origin =
      pending_origins_[make_pair(extension->id(), origin)];
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  TabContext& tab_context = pending_origin[infobar_manager];
  tab_context.callbacks.push_back(std::move(callback));
  if (tab_context.prompt_pending) {
    // This tab is already waiting on a prompt for this extension and origin.
    return;
  }

  // We need to ask the user.
  ++g_alert_count;

  switch (g_alert_mode) {
    // Production code should always be using INTERACTIVE.
    case ScopedAlertTracker::INTERACTIVE: {
      int template_id =
          extension->is_app()
              ? IDS_EXTENSION_PROMPT_APP_CONNECT_FROM_INCOGNITO
              : IDS_EXTENSION_PROMPT_EXTENSION_CONNECT_FROM_INCOGNITO;
      tab_context.prompt_pending = true;
      ShowInfoBarResult result = ShowInfoBar(
          web_contents, infobar_manager,
          l10n_util::GetStringFUTF16(template_id,
                                     base::UTF8ToUTF16(origin.spec()),
                                     base::UTF8ToUTF16(extension->name())),
          base::BindOnce(&IncognitoConnectability::OnInteractiveResponse,
                         weak_factory_.GetWeakPtr(), extension->id(), origin,
                         infobar_manager));
      // Showing the prompt can answer this tab's queries re-entrantly and
      // destroy the TabContext `tab_context` refers to, so look it up again.
      auto origin_it =
          pending_origins_.find(make_pair(extension->id(), origin));
      if (origin_it != pending_origins_.end()) {
        auto tab_it = origin_it->second.find(infobar_manager);
        if (tab_it != origin_it->second.end()) {
          tab_it->second.infobar = result.infobar;
          tab_it->second.is_migrated = result.is_migrated;
        }
      }
      break;
    }

    // Testing code can override to always allow or deny.
    case ScopedAlertTracker::ALWAYS_ALLOW:
    case ScopedAlertTracker::ALWAYS_DENY:
      OnInteractiveResponse(extension->id(), origin, infobar_manager,
                            g_alert_mode);
      break;
  }
}

IncognitoConnectability::TabContext::TabContext() = default;

IncognitoConnectability::TabContext::~TabContext() = default;

void IncognitoConnectability::OnInteractiveResponse(
    const ExtensionId& extension_id,
    const GURL& origin,
    infobars::ContentInfoBarManager* infobar_manager,
    ScopedAlertTracker::Mode response) {
  switch (response) {
    case ScopedAlertTracker::ALWAYS_ALLOW:
      allowed_origins_[extension_id].insert(origin);
      break;
    case ScopedAlertTracker::ALWAYS_DENY:
      disallowed_origins_[extension_id].insert(origin);
      break;
    default:
      // Otherwise the user has not expressed an explicit preference and so
      // nothing should be permanently recorded.
      break;
  }

  PendingOriginMap::iterator origin_it =
      pending_origins_.find(make_pair(extension_id, origin));
  // These queries may already have been answered, e.g. by another tab
  // answering definitively before this tab's result was delivered.
  if (origin_it == pending_origins_.end()) {
    return;
  }
  PendingOrigin& pending_origin = origin_it->second;
  if (!pending_origin.contains(infobar_manager)) {
    return;
  }

  std::vector<base::OnceCallback<void(bool)>> callbacks;
  if (response == ScopedAlertTracker::INTERACTIVE) {
    // No definitive answer for this extension and origin. Execute only the
    // callbacks associated with this tab.
    TabContext& tab_context = pending_origin[infobar_manager];
    callbacks.swap(tab_context.callbacks);
    pending_origin.erase(infobar_manager);
  } else {
    // We have a definitive answer for this extension and origin. Close all
    // other infobars and answer all the callbacks.
    for (auto& map_entry : pending_origin) {
      infobars::ContentInfoBarManager* other_infobar_manager = map_entry.first;
      TabContext& other_tab_context = map_entry.second;
      if (other_infobar_manager != infobar_manager &&
          other_tab_context.infobar) {
        // Take the other tab's infobar down without reporting a result; its
        // callbacks are answered here. Stop tracking it first: removal
        // destroys it synchronously, and the erase below would then report a
        // dangling raw_ptr.
        infobars::InfoBar* other_infobar =
            std::exchange(other_tab_context.infobar, nullptr).get();
#if !BUILDFLAG(IS_ANDROID)
        // Dispatch on how the infobar was created, not on the feature state:
        // a migrated prompt for a tabless WebContents falls back to the
        // legacy delegate, which Hide() must not be handed.
        if (other_tab_context.is_migrated) {
          infobars::BrowserInfoBarManager* browser_infobar_manager =
              infobars::BrowserInfoBarManager::From(g_browser_process);
          CHECK(browser_infobar_manager);
          browser_infobar_manager->Hide(other_infobar);
        } else
#endif  //  !BUILDFLAG(IS_ANDROID)
        {
          IncognitoConnectabilityInfoBarDelegate* delegate =
              static_cast<IncognitoConnectabilityInfoBarDelegate*>(
                  other_infobar->delegate());
          delegate->set_answered();
          other_infobar_manager->RemoveInfoBar(other_infobar);
        }
      }
      callbacks.insert(
          callbacks.end(),
          std::make_move_iterator(other_tab_context.callbacks.begin()),
          std::make_move_iterator(other_tab_context.callbacks.end()));
    }
    pending_origins_.erase(origin_it);
  }

  CHECK(!callbacks.empty(), base::NotFatalUntil::M161);
  for (auto& callback : callbacks) {
    std::move(callback).Run(response == ScopedAlertTracker::ALWAYS_ALLOW);
  }
}

bool IncognitoConnectability::IsInMap(const Extension* extension,
                                      const GURL& origin,
                                      const ExtensionToOriginsMap& map) {
  CHECK_EQ(origin, origin.DeprecatedGetOriginAsURL(),
           base::NotFatalUntil::M161);
  auto it = map.find(extension->id());
  return it != map.end() && it->second.count(origin) > 0;
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<IncognitoConnectability>>::DestructorAtExit
    g_incognito_connectability_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<IncognitoConnectability>*
IncognitoConnectability::GetFactoryInstance() {
  return g_incognito_connectability_factory.Pointer();
}

// static
void IncognitoConnectability::EnsureFactoryBuilt() {
  GetFactoryInstance();
}

}  // namespace extensions
