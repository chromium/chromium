// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"

#include <string>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/not_fatal_until.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

IncognitoConnectability::ScopedAlertTracker::Mode g_alert_mode =
    IncognitoConnectability::ScopedAlertTracker::INTERACTIVE;
int g_alert_count = 0;

}  // namespace

IncognitoConnectability::ScopedAlertTracker::ScopedAlertTracker(Mode mode)
    : last_checked_invocation_count_(g_alert_count) {
  DCHECK_EQ(INTERACTIVE, g_alert_mode);
  DCHECK_NE(INTERACTIVE, mode);
  g_alert_mode = mode;
}

IncognitoConnectability::ScopedAlertTracker::~ScopedAlertTracker() {
  DCHECK_NE(INTERACTIVE, g_alert_mode);
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

IncognitoConnectability::~IncognitoConnectability() {
}

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
  if (tab_context.infobar) {
    // This tab is already displaying an infobar for this extension and origin.
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
      tab_context.infobar = IncognitoConnectabilityInfoBarDelegate::Create(
          infobar_manager,
          l10n_util::GetStringFUTF16(template_id,
                                     base::UTF8ToUTF16(origin.spec()),
                                     base::UTF8ToUTF16(extension->name())),
          base::BindOnce(&IncognitoConnectability::OnInteractiveResponse,
                         weak_factory_.GetWeakPtr(), extension->id(), origin,
                         infobar_manager));
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

IncognitoConnectability::TabContext::TabContext() : infobar(nullptr) {
}

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
  CHECK(origin_it != pending_origins_.end(), base::NotFatalUntil::M130);
  PendingOrigin& pending_origin = origin_it->second;
  DCHECK(base::Contains(pending_origin, infobar_manager));

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
      if (other_infobar_manager != infobar_manager) {
        // Disarm the delegate so that it doesn't think the infobar has been
        // dismissed.
        IncognitoConnectabilityInfoBarDelegate* delegate =
            static_cast<IncognitoConnectabilityInfoBarDelegate*>(
                other_tab_context.infobar->delegate());
        delegate->set_answered();
        other_infobar_manager->RemoveInfoBar(other_tab_context.infobar);
      }
      callbacks.insert(
          callbacks.end(),
          std::make_move_iterator(other_tab_context.callbacks.begin()),
          std::make_move_iterator(other_tab_context.callbacks.end()));
    }
    pending_origins_.erase(origin_it);
  }

  DCHECK(!callbacks.empty());
  for (auto& callback : callbacks) {
    std::move(callback).Run(response == ScopedAlertTracker::ALWAYS_ALLOW);
  }
}

bool IncognitoConnectability::IsInMap(const Extension* extension,
                                      const GURL& origin,
                                      const ExtensionToOriginsMap& map) {
  DCHECK_EQ(origin, origin.DeprecatedGetOriginAsURL());
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
