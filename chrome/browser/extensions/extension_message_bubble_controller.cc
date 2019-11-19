// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_bubble_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/url_constants.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// How many extensions to show in the bubble (max).
const int kMaxExtensionsToShow = 7;

// Whether or not to ignore the learn more link navigation for testing.
bool g_should_ignore_learn_more_for_testing = false;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleController::Delegate

ExtensionMessageBubbleController::Delegate::Delegate(Profile* profile)
    : profile_(profile),
      service_(ExtensionSystem::Get(profile)->extension_service()),
      registry_(ExtensionRegistry::Get(profile)) {
}

ExtensionMessageBubbleController::Delegate::~Delegate() {}

base::string16 ExtensionMessageBubbleController::Delegate::GetLearnMoreLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

void ExtensionMessageBubbleController::Delegate::OnAction() {}

bool ExtensionMessageBubbleController::Delegate::HasBubbleInfoBeenAcknowledged(
    const std::string& extension_id) {
  std::string pref_name = get_acknowledged_flag_pref_name();
  if (pref_name.empty())
    return false;
  bool pref_state = false;
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  prefs->ReadPrefAsBoolean(extension_id, pref_name, &pref_state);
  return pref_state;
}

void ExtensionMessageBubbleController::Delegate::SetBubbleInfoBeenAcknowledged(
    const std::string& extension_id,
    bool value) {
  std::string pref_name = get_acknowledged_flag_pref_name();
  if (pref_name.empty())
    return;
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  prefs->UpdateExtensionPref(
      extension_id, pref_name,
      value ? std::make_unique<base::Value>(value) : nullptr);
}

std::string
ExtensionMessageBubbleController::Delegate::get_acknowledged_flag_pref_name()
    const {
  return acknowledged_pref_name_;
}

void ExtensionMessageBubbleController::Delegate::
    set_acknowledged_flag_pref_name(const std::string& pref_name) {
  acknowledged_pref_name_ = pref_name;
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleController

ExtensionMessageBubbleController::ExtensionMessageBubbleController(
    Delegate* delegate,
    Browser* browser)
    : browser_(browser),
      model_(ToolbarActionsModel::Get(browser_->profile())),
      user_action_(ACTION_BOUNDARY),
      delegate_(delegate),
      initialized_(false),
      is_highlighting_(false),
      is_active_bubble_(false) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_->profile()));
  BrowserList::AddObserver(this);
}

ExtensionMessageBubbleController::~ExtensionMessageBubbleController() {
  BrowserList::RemoveObserver(this);
  if (is_active_bubble_)
    model_->set_has_active_bubble(false);
  if (is_highlighting_)
    model_->StopHighlighting();
}

Profile* ExtensionMessageBubbleController::profile() {
  return browser_->profile();
}

bool ExtensionMessageBubbleController::ShouldShow() {
  // In the case when there are multiple extensions in the list, we need to
  // check if each extension entry is still installed, and, if not, remove it
  // from the list.
  UpdateExtensionIdList();
  return !GetExtensionIdList().empty() &&
         (!model_->has_active_bubble() || is_active_bubble_) &&
         delegate_->ShouldShow(GetExtensionIdList());
}

std::vector<base::string16>
ExtensionMessageBubbleController::GetExtensionList() {
  ExtensionIdList* list = GetOrCreateExtensionList();
  if (list->empty())
    return std::vector<base::string16>();

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  std::vector<base::string16> return_value;
  for (const std::string& id : *list) {
    const Extension* extension =
        registry->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
    return_value.push_back(base::UTF8ToUTF16(extension->name()));
  }
  return return_value;
}

base::string16 ExtensionMessageBubbleController::GetExtensionListForDisplay() {
  if (!delegate_->ShouldShowExtensionList())
    return base::string16();

  std::vector<base::string16> extension_list = GetExtensionList();
  if (extension_list.size() > kMaxExtensionsToShow) {
    int old_size = extension_list.size();
    extension_list.erase(extension_list.begin() + kMaxExtensionsToShow,
                         extension_list.end());
    extension_list.push_back(delegate_->GetOverflowText(
        base::NumberToString16(old_size - kMaxExtensionsToShow)));
  }
  const base::char16 bullet_point = 0x2022;
  base::string16 prefix = bullet_point + base::ASCIIToUTF16(" ");
  for (base::string16& str : extension_list)
    str.insert(0, prefix);
  return base::JoinString(extension_list, base::ASCIIToUTF16("\n"));
}

const ExtensionIdList& ExtensionMessageBubbleController::GetExtensionIdList() {
  return *GetOrCreateExtensionList();
}

void ExtensionMessageBubbleController::UpdateExtensionIdList() {
  ExtensionIdList* extension_ids = GetOrCreateExtensionList();
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  int include_mask = delegate_->ShouldLimitToEnabledExtensions()
                         ? ExtensionRegistry::ENABLED
                         : ExtensionRegistry::EVERYTHING;
  for (auto iter = extension_ids->begin(); iter != extension_ids->end();) {
    const Extension* extension =
        registry->GetExtensionById(*iter, include_mask);
    if (extension)
      ++iter;
    else
      iter = extension_ids->erase(iter);
  }
}

bool ExtensionMessageBubbleController::CloseOnDeactivate() {
  return delegate_->ShouldCloseOnDeactivate();
}

void ExtensionMessageBubbleController::HighlightExtensionsIfNecessary() {
  DCHECK(is_active_bubble_);
  if (delegate_->ShouldHighlightExtensions() && !is_highlighting_) {
    is_highlighting_ = true;
    const ExtensionIdList& extension_ids = GetExtensionIdList();
    DCHECK(!extension_ids.empty());
    model_->HighlightActions(extension_ids,
                             ToolbarActionsModel::HIGHLIGHT_WARNING);
  }
}

void ExtensionMessageBubbleController::OnShown(
    const base::Closure& close_bubble_callback) {
  close_bubble_callback_ = close_bubble_callback;
  DCHECK(is_active_bubble_);
  delegate_->OnShown(GetExtensionIdList());

  if (!extension_registry_observer_.IsObserving(
          ExtensionRegistry::Get(browser_->profile()))) {
    extension_registry_observer_.Add(
        ExtensionRegistry::Get(browser_->profile()));
  }
}

void ExtensionMessageBubbleController::OnBubbleAction() {
  // In addition to closing the bubble, OnBubbleAction() may result in a removal
  // or disabling of the extension. To prevent triggering OnExtensionUnloaded(),
  // which will also try to close the bubble, the controller's extension
  // registry observer is removed. Note, we do not remove the extension registry
  // observer in the cases of OnBubbleDismiss() and OnLinkedClicked() since they
  // do not result in extensions being unloaded.
  extension_registry_observer_.RemoveAll();
  DCHECK_EQ(ACTION_BOUNDARY, user_action_);
  user_action_ = ACTION_EXECUTE;

  delegate_->LogAction(ACTION_EXECUTE);
  delegate_->PerformAction(*GetOrCreateExtensionList());

  OnClose();
}

void ExtensionMessageBubbleController::OnBubbleDismiss(
    bool closed_by_deactivation) {
  // OnBubbleDismiss() can be called twice when we receive multiple
  // "OnWidgetDestroying" notifications (this can at least happen when we close
  // a window with a notification open). Handle this gracefully.
  if (user_action_ != ACTION_BOUNDARY) {
    DCHECK(user_action_ == ACTION_DISMISS_USER_ACTION ||
           user_action_ == ACTION_DISMISS_DEACTIVATION);
    return;
  }

  user_action_ = closed_by_deactivation ? ACTION_DISMISS_DEACTIVATION
                                        : ACTION_DISMISS_USER_ACTION;

  delegate_->LogAction(user_action_);

  OnClose();
}

void ExtensionMessageBubbleController::OnLinkClicked() {
  DCHECK_EQ(ACTION_BOUNDARY, user_action_);
  user_action_ = ACTION_LEARN_MORE;

  delegate_->LogAction(ACTION_LEARN_MORE);
  // Opening a new tab for the learn more link can cause the bubble to close, so
  // perform our cleanup here before opening the new tab.
  OnClose();
  if (!g_should_ignore_learn_more_for_testing) {
    GURL learn_more_url = delegate_->GetLearnMoreUrl();
    DCHECK(learn_more_url.is_valid());
    browser_->OpenURL(
        content::OpenURLParams(learn_more_url, content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK, false));
  }
  // Warning: |this| may be deleted here!
}

void ExtensionMessageBubbleController::SetIsActiveBubble() {
  DCHECK(!is_active_bubble_);
  DCHECK(!model_->has_active_bubble());
  is_active_bubble_ = true;
  model_->set_has_active_bubble(true);
}

// static
void ExtensionMessageBubbleController::set_should_ignore_learn_more_for_testing(
    bool should_ignore) {
  g_should_ignore_learn_more_for_testing = should_ignore;
}

void ExtensionMessageBubbleController::HandleExtensionUnloadOrUninstall() {
  UpdateExtensionIdList();
  // If the callback is set, then that means that OnShown() was called, and the
  // bubble was displayed.
  if (close_bubble_callback_ && GetExtensionIdList().empty()) {
    std::move(close_bubble_callback_).Run();
  }
  // If the bubble refers to multiple extensions, we do not close the bubble.
}

void ExtensionMessageBubbleController::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  HandleExtensionUnloadOrUninstall();
}

void ExtensionMessageBubbleController::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  HandleExtensionUnloadOrUninstall();
}

void ExtensionMessageBubbleController::OnShutdown(ExtensionRegistry* registry) {
  // It is possible that the extension registry is destroyed before the
  // controller. In such case, the controller should no longer observe the
  // registry.
  extension_registry_observer_.Remove(registry);
}

void ExtensionMessageBubbleController::OnBrowserRemoved(Browser* browser) {
  extension_registry_observer_.RemoveAll();
  if (browser == browser_) {
    if (is_highlighting_) {
      model_->StopHighlighting();
      is_highlighting_ = false;
    }
    if (is_active_bubble_) {
      model_->set_has_active_bubble(false);
      is_active_bubble_ = false;
    }
  }
}

void ExtensionMessageBubbleController::AcknowledgeExtensions() {
  ExtensionIdList* list = GetOrCreateExtensionList();
  for (ExtensionIdList::const_iterator it = list->begin();
       it != list->end(); ++it)
    delegate_->AcknowledgeExtension(*it, user_action_);
}

ExtensionIdList* ExtensionMessageBubbleController::GetOrCreateExtensionList() {
  if (!initialized_) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
    std::unique_ptr<const ExtensionSet> all_extensions;
    if (!delegate_->ShouldLimitToEnabledExtensions())
      all_extensions = registry->GenerateInstalledExtensionsSet();
    const ExtensionSet& extensions_to_check =
        all_extensions ? *all_extensions : registry->enabled_extensions();
    for (const scoped_refptr<const Extension>& extension :
         extensions_to_check) {
      if (delegate_->ShouldIncludeExtension(extension.get()))
        extension_list_.push_back(extension->id());
    }

    delegate_->LogExtensionCount(extension_list_.size());
    initialized_ = true;
  }

  return &extension_list_;
}

void ExtensionMessageBubbleController::OnClose() {
  DCHECK_NE(ACTION_BOUNDARY, user_action_);
  // If the bubble was closed due to deactivation, don't treat it as
  // acknowledgment so that the user will see the bubble again (until they
  // explicitly take an action).
  if (user_action_ != ACTION_DISMISS_DEACTIVATION ||
      delegate_->ShouldAcknowledgeOnDeactivate()) {
    AcknowledgeExtensions();
    delegate_->OnAction();
  }

  extension_registry_observer_.RemoveAll();
}

}  // namespace extensions
