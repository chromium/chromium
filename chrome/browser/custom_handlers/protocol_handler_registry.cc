// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/child_process_security_policy.h"

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;

namespace {

const ProtocolHandler& LookupHandler(
    const ProtocolHandlerRegistry::ProtocolHandlerMap& handler_map,
    const std::string& scheme) {
  auto p = handler_map.find(scheme);

  if (p != handler_map.end())
    return p->second;

  return ProtocolHandler::EmptyProtocolHandler();
}

// If true default protocol handlers will be removed if the OS level
// registration for a protocol is no longer Chrome.
bool ShouldRemoveHandlersNotInOS() {
#if defined(OS_LINUX)
  // We don't do this on Linux as the OS registration there is not reliable,
  // and Chrome OS doesn't have any notion of OS registration.
  // TODO(benwells): When Linux support is more reliable remove this
  // difference (http://crbug.com/88255).
  return false;
#else
  return shell_integration::GetDefaultWebClientSetPermission() !=
         shell_integration::SET_DEFAULT_NOT_ALLOWED;
#endif
}

GURL TranslateUrl(
    const ProtocolHandlerRegistry::ProtocolHandlerMap& handler_map,
    const GURL& url) {
  const ProtocolHandler& handler = LookupHandler(handler_map, url.scheme());
  if (handler.IsEmpty())
    return GURL();

  GURL translated_url(handler.TranslateUrl(url));
  if (!translated_url.is_valid())
    return GURL();

  return translated_url;
}

}  // namespace

// Delegate --------------------------------------------------------------------

ProtocolHandlerRegistry::Delegate::~Delegate() {}

void ProtocolHandlerRegistry::Delegate::RegisterExternalHandler(
    const std::string& protocol) {
  ChildProcessSecurityPolicy* policy =
    ChildProcessSecurityPolicy::GetInstance();
  if (!policy->IsWebSafeScheme(protocol)) {
    policy->RegisterWebSafeScheme(protocol);
  }
}

void ProtocolHandlerRegistry::Delegate::DeregisterExternalHandler(
    const std::string& protocol) {
}

bool ProtocolHandlerRegistry::Delegate::IsExternalHandlerRegistered(
    const std::string& protocol) {
  // NOTE(koz): This function is safe to call from any thread, despite living
  // in ProfileIOData.
  return ProfileIOData::IsHandledProtocol(protocol);
}

void ProtocolHandlerRegistry::Delegate::RegisterWithOSAsDefaultClient(
    const std::string& protocol,
    shell_integration::DefaultWebClientWorkerCallback callback) {
  // The worker pointer is reference counted. While it is running, the
  // sequence it runs on will hold references it will be automatically freed
  // once all its tasks have finished.
  base::MakeRefCounted<shell_integration::DefaultProtocolClientWorker>(
      std::move(callback), protocol)
      ->StartSetAsDefault();
}

void ProtocolHandlerRegistry::Delegate::CheckDefaultClientWithOS(
    const std::string& protocol,
    shell_integration::DefaultWebClientWorkerCallback callback) {
  // The worker pointer is reference counted. While it is running, the
  // sequence it runs on will hold references it will be automatically freed
  // once all its tasks have finished.
  base::MakeRefCounted<shell_integration::DefaultProtocolClientWorker>(
      std::move(callback), protocol)
      ->StartCheckIsDefault();
}

// ProtocolHandlerRegistry -----------------------------------------------------

ProtocolHandlerRegistry::ProtocolHandlerRegistry(
    content::BrowserContext* context,
    std::unique_ptr<Delegate> delegate)
    : context_(context),
      delegate_(std::move(delegate)),
      enabled_(true),
      is_loading_(false),
      is_loaded_(false) {}

bool ProtocolHandlerRegistry::SilentlyHandleRegisterHandlerRequest(
    const ProtocolHandler& handler) {
  if (handler.IsEmpty() || !CanSchemeBeOverridden(handler.protocol()))
    return true;

  if (!enabled() || IsRegistered(handler) || HasIgnoredEquivalent(handler))
    return true;

  if (AttemptReplace(handler))
    return true;

  return false;
}

void ProtocolHandlerRegistry::OnAcceptRegisterProtocolHandler(
    const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RegisterProtocolHandler(handler, USER))
    return;
  SetDefault(handler);
  Save();
  NotifyChanged();
}

void ProtocolHandlerRegistry::OnDenyRegisterProtocolHandler(
    const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RegisterProtocolHandler(handler, USER);
  Save();
  NotifyChanged();
}

void ProtocolHandlerRegistry::OnIgnoreRegisterProtocolHandler(
    const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  IgnoreProtocolHandler(handler, USER);
  Save();
  NotifyChanged();
}

bool ProtocolHandlerRegistry::AttemptReplace(const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProtocolHandler old_default = GetHandlerFor(handler.protocol());
  bool make_new_handler_default = handler.IsSameOrigin(old_default);
  ProtocolHandlerList to_replace(GetReplacedHandlers(handler));
  if (to_replace.empty())
    return false;
  for (auto p = to_replace.begin(); p != to_replace.end(); ++p) {
    RemoveHandler(*p);
  }
  if (make_new_handler_default) {
    OnAcceptRegisterProtocolHandler(handler);
  } else {
    InsertHandler(handler);
    NotifyChanged();
  }
  return true;
}

ProtocolHandlerRegistry::ProtocolHandlerList
ProtocolHandlerRegistry::GetReplacedHandlers(
    const ProtocolHandler& handler) const {
  ProtocolHandlerList replaced_handlers;
  const ProtocolHandlerList* handlers = GetHandlerList(handler.protocol());
  if (!handlers)
    return replaced_handlers;
  for (auto p = handlers->begin(); p != handlers->end(); ++p) {
    if (handler.IsSameOrigin(*p)) {
      replaced_handlers.push_back(*p);
    }
  }
  return replaced_handlers;
}

void ProtocolHandlerRegistry::ClearDefault(const std::string& scheme) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  default_handlers_.erase(scheme);
  Save();
  NotifyChanged();
}

bool ProtocolHandlerRegistry::IsDefault(
    const ProtocolHandler& handler) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetHandlerFor(handler.protocol()) == handler;
}

void ProtocolHandlerRegistry::InstallDefaultsForChromeOS() {
#if defined(OS_CHROMEOS)
  // Only chromeos has default protocol handlers at this point.
  AddPredefinedHandler(
      ProtocolHandler::CreateProtocolHandler(
          "mailto",
          GURL("https://mail.google.com/mail/?extsrc=mailto&amp;url=%s")));
  AddPredefinedHandler(
      ProtocolHandler::CreateProtocolHandler(
          "webcal",
          GURL("https://www.google.com/calendar/render?cid=%s")));
#else
  NOTREACHED();  // this method should only ever be called in chromeos.
#endif
}

void ProtocolHandlerRegistry::InitProtocolSettings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Any further default additions to the table will get rejected from now on.
  is_loaded_ = true;
  is_loading_ = true;

  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  if (prefs->HasPrefPath(prefs::kCustomHandlersEnabled)) {
    if (prefs->GetBoolean(prefs::kCustomHandlersEnabled)) {
      Enable();
    } else {
      Disable();
    }
  }

  RegisterProtocolHandlersFromPref(prefs::kPolicyRegisteredProtocolHandlers,
                                   POLICY);
  RegisterProtocolHandlersFromPref(prefs::kRegisteredProtocolHandlers, USER);
  IgnoreProtocolHandlersFromPref(prefs::kPolicyIgnoredProtocolHandlers, POLICY);
  IgnoreProtocolHandlersFromPref(prefs::kIgnoredProtocolHandlers, USER);

  is_loading_ = false;

  // For each default protocol handler, check that we are still registered
  // with the OS as the default application.
  if (ShouldRemoveHandlersNotInOS()) {
    for (ProtocolHandlerMap::const_iterator p = default_handlers_.begin();
         p != default_handlers_.end(); ++p) {
      const std::string& protocol = p->second.protocol();
      delegate_->CheckDefaultClientWithOS(
          protocol, GetDefaultWebClientCallback(protocol));
    }
  }
}

int ProtocolHandlerRegistry::GetHandlerIndex(const std::string& scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const ProtocolHandler& handler = GetHandlerFor(scheme);
  if (handler.IsEmpty())
    return -1;
  const ProtocolHandlerList* handlers = GetHandlerList(scheme);
  if (!handlers)
    return -1;

  ProtocolHandlerList::const_iterator p;
  int i;
  for (i = 0, p = handlers->begin(); p != handlers->end(); ++p, ++i) {
    if (*p == handler)
      return i;
  }
  return -1;
}

ProtocolHandlerRegistry::ProtocolHandlerList
ProtocolHandlerRegistry::GetHandlersFor(
    const std::string& scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto p = protocol_handlers_.find(scheme);
  if (p == protocol_handlers_.end()) {
    return ProtocolHandlerList();
  }
  return p->second;
}

ProtocolHandlerRegistry::ProtocolHandlerList
ProtocolHandlerRegistry::GetUserDefinedHandlers(base::Time begin,
                                                base::Time end) const {
  ProtocolHandlerRegistry::ProtocolHandlerList result;
  for (const auto& entry : user_protocol_handlers_) {
    for (const ProtocolHandler& handler : entry.second) {
      if (base::Contains(predefined_protocol_handlers_, handler))
        continue;
      if (begin <= handler.last_modified() && handler.last_modified() < end)
        result.push_back(handler);
    }
  }
  return result;
}

ProtocolHandlerRegistry::ProtocolHandlerList
ProtocolHandlerRegistry::GetUserIgnoredHandlers(base::Time begin,
                                                base::Time end) const {
  ProtocolHandlerRegistry::ProtocolHandlerList result;
  for (const ProtocolHandler& handler : user_ignored_protocol_handlers_) {
    if (begin <= handler.last_modified() && handler.last_modified() < end)
      result.push_back(handler);
  }
  return result;
}

void ProtocolHandlerRegistry::ClearUserDefinedHandlers(base::Time begin,
                                                       base::Time end) {
  for (const ProtocolHandler& handler : GetUserDefinedHandlers(begin, end))
    RemoveHandler(handler);

  for (const ProtocolHandler& handler : GetUserIgnoredHandlers(begin, end))
    RemoveIgnoredHandler(handler);
}

ProtocolHandlerRegistry::ProtocolHandlerList
ProtocolHandlerRegistry::GetIgnoredHandlers() {
  return ignored_protocol_handlers_;
}

void ProtocolHandlerRegistry::GetRegisteredProtocols(
    std::vector<std::string>* output) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProtocolHandlerMultiMap::const_iterator p;
  for (p = protocol_handlers_.begin(); p != protocol_handlers_.end(); ++p) {
    if (!p->second.empty())
      output->push_back(p->first);
  }
}

bool ProtocolHandlerRegistry::CanSchemeBeOverridden(
    const std::string& scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const ProtocolHandlerList* handlers = GetHandlerList(scheme);
  // If we already have a handler for this scheme, we can add more.
  if (handlers != NULL && !handlers->empty())
    return true;
  // Don't override a scheme if it already has an external handler.
  return !delegate_->IsExternalHandlerRegistered(scheme);
}

bool ProtocolHandlerRegistry::IsRegistered(
    const ProtocolHandler& handler) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const ProtocolHandlerList* handlers = GetHandlerList(handler.protocol());
  if (!handlers) {
    return false;
  }
  return base::Contains(*handlers, handler);
}

bool ProtocolHandlerRegistry::IsRegisteredByUser(
    const ProtocolHandler& handler) {
  return HandlerExists(handler, &user_protocol_handlers_);
}

bool ProtocolHandlerRegistry::HasPolicyRegisteredHandler(
    const std::string& scheme) {
  return (policy_protocol_handlers_.find(scheme) !=
          policy_protocol_handlers_.end());
}

bool ProtocolHandlerRegistry::IsIgnored(const ProtocolHandler& handler) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProtocolHandlerList::const_iterator i;
  for (i = ignored_protocol_handlers_.begin();
       i != ignored_protocol_handlers_.end(); ++i) {
    if (*i == handler) {
      return true;
    }
  }
  return false;
}

bool ProtocolHandlerRegistry::HasRegisteredEquivalent(
    const ProtocolHandler& handler) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const ProtocolHandlerList* handlers = GetHandlerList(handler.protocol());
  if (!handlers) {
    return false;
  }
  ProtocolHandlerList::const_iterator i;
  for (i = handlers->begin(); i != handlers->end(); ++i) {
    if (handler.IsEquivalent(*i)) {
      return true;
    }
  }
  return false;
}

bool ProtocolHandlerRegistry::HasIgnoredEquivalent(
    const ProtocolHandler& handler) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProtocolHandlerList::const_iterator i;
  for (i = ignored_protocol_handlers_.begin();
       i != ignored_protocol_handlers_.end(); ++i) {
    if (handler.IsEquivalent(*i)) {
      return true;
    }
  }
  return false;
}

void ProtocolHandlerRegistry::RemoveIgnoredHandler(
    const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool should_notify = false;
  if (HandlerExists(handler, ignored_protocol_handlers_) &&
      HandlerExists(handler, user_ignored_protocol_handlers_)) {
    EraseHandler(handler, &user_ignored_protocol_handlers_);
    Save();
    if (!HandlerExists(handler, policy_ignored_protocol_handlers_)) {
      EraseHandler(handler, &ignored_protocol_handlers_);
      should_notify = true;
    }
  }
  if (should_notify)
    NotifyChanged();
}

bool ProtocolHandlerRegistry::IsHandledProtocol(
    const std::string& scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return enabled_ && !GetHandlerFor(scheme).IsEmpty();
}

void ProtocolHandlerRegistry::RemoveHandler(
    const ProtocolHandler& handler) {
  if (IsIgnored(handler)) {
    RemoveIgnoredHandler(handler);
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProtocolHandlerList& handlers = protocol_handlers_[handler.protocol()];
  bool erase_success = false;
  if (HandlerExists(handler, handlers) &&
      HandlerExists(handler, &user_protocol_handlers_)) {
    EraseHandler(handler, &user_protocol_handlers_);
    erase_success = true;
    if (!HandlerExists(handler, &policy_protocol_handlers_))
      EraseHandler(handler, &protocol_handlers_);
  }
  auto q = default_handlers_.find(handler.protocol());
  if (erase_success && q != default_handlers_.end() && q->second == handler) {
    // Make the new top handler in the list the default.
    if (!handlers.empty()) {
      // NOTE We pass a copy because SetDefault() modifies handlers.
      SetDefault(ProtocolHandler(handlers[0]));
    } else {
      default_handlers_.erase(q);
    }
  }

  if (erase_success && !IsHandledProtocol(handler.protocol())) {
    delegate_->DeregisterExternalHandler(handler.protocol());
  }
  Save();
  if (erase_success)
    NotifyChanged();
}

void ProtocolHandlerRegistry::RemoveDefaultHandler(const std::string& scheme) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProtocolHandler current_default = GetHandlerFor(scheme);
  if (!current_default.IsEmpty())
    RemoveHandler(current_default);
}

const ProtocolHandler& ProtocolHandlerRegistry::GetHandlerFor(
    const std::string& scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return LookupHandler(default_handlers_, scheme);
}

GURL ProtocolHandlerRegistry::Translate(const GURL& url) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return TranslateUrl(default_handlers_, url);
}

void ProtocolHandlerRegistry::Enable() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (enabled_) {
    return;
  }
  enabled_ = true;
  ProtocolHandlerMap::const_iterator p;
  for (p = default_handlers_.begin(); p != default_handlers_.end(); ++p) {
    delegate_->RegisterExternalHandler(p->first);
  }
  Save();
  NotifyChanged();
}

void ProtocolHandlerRegistry::Disable() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!enabled_) {
    return;
  }
  enabled_ = false;

  ProtocolHandlerMap::const_iterator p;
  for (p = default_handlers_.begin(); p != default_handlers_.end(); ++p) {
    delegate_->DeregisterExternalHandler(p->first);
  }
  Save();
  NotifyChanged();
}

void ProtocolHandlerRegistry::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_.reset(NULL);

  weak_ptr_factory_.InvalidateWeakPtrs();
}

// static
void ProtocolHandlerRegistry::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kRegisteredProtocolHandlers);
  registry->RegisterListPref(prefs::kIgnoredProtocolHandlers);
  registry->RegisterListPref(prefs::kPolicyRegisteredProtocolHandlers);
  registry->RegisterListPref(prefs::kPolicyIgnoredProtocolHandlers);
  registry->RegisterBooleanPref(prefs::kCustomHandlersEnabled, true);
}

ProtocolHandlerRegistry::~ProtocolHandlerRegistry() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ProtocolHandlerRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProtocolHandlerRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ProtocolHandlerRegistry::PromoteHandler(const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsRegistered(handler));
  auto p = protocol_handlers_.find(handler.protocol());
  ProtocolHandlerList& list = p->second;
  list.erase(std::find(list.begin(), list.end(), handler));
  list.insert(list.begin(), handler);
}

void ProtocolHandlerRegistry::Save() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_loading_) {
    return;
  }
  std::unique_ptr<base::Value> registered_protocol_handlers(
      EncodeRegisteredHandlers());
  std::unique_ptr<base::Value> ignored_protocol_handlers(
      EncodeIgnoredHandlers());
  PrefService* prefs = user_prefs::UserPrefs::Get(context_);

  prefs->Set(prefs::kRegisteredProtocolHandlers,
      *registered_protocol_handlers);
  prefs->Set(prefs::kIgnoredProtocolHandlers,
      *ignored_protocol_handlers);
  prefs->SetBoolean(prefs::kCustomHandlersEnabled, enabled_);
}

const ProtocolHandlerRegistry::ProtocolHandlerList*
ProtocolHandlerRegistry::GetHandlerList(
    const std::string& scheme) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto p = protocol_handlers_.find(scheme);
  if (p == protocol_handlers_.end()) {
    return NULL;
  }
  return &p->second;
}

void ProtocolHandlerRegistry::SetDefault(const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const std::string& protocol = handler.protocol();
  ProtocolHandlerMap::const_iterator p = default_handlers_.find(protocol);
  // If we're not loading, and we are setting a default for a new protocol,
  // register with the OS.
  if (!is_loading_ && p == default_handlers_.end())
    delegate_->RegisterWithOSAsDefaultClient(
        protocol, GetDefaultWebClientCallback(protocol));
  default_handlers_.erase(protocol);
  default_handlers_.insert(std::make_pair(protocol, handler));

  PromoteHandler(handler);
}

void ProtocolHandlerRegistry::InsertHandler(const ProtocolHandler& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto p = protocol_handlers_.find(handler.protocol());

  if (p != protocol_handlers_.end()) {
    p->second.push_back(handler);
    return;
  }

  ProtocolHandlerList new_list;
  new_list.push_back(handler);
  protocol_handlers_[handler.protocol()] = new_list;
}

base::Value* ProtocolHandlerRegistry::EncodeRegisteredHandlers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ListValue* protocol_handlers = new base::ListValue();
  for (auto i = user_protocol_handlers_.begin();
       i != user_protocol_handlers_.end(); ++i) {
    for (auto j = i->second.begin(); j != i->second.end(); ++j) {
      std::unique_ptr<base::DictionaryValue> encoded = j->Encode();
      if (IsDefault(*j)) {
        encoded->Set("default", std::make_unique<base::Value>(true));
      }
      protocol_handlers->Append(std::move(encoded));
    }
  }
  return protocol_handlers;
}

base::Value* ProtocolHandlerRegistry::EncodeIgnoredHandlers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ListValue* handlers = new base::ListValue();
  for (auto i = user_ignored_protocol_handlers_.begin();
       i != user_ignored_protocol_handlers_.end(); ++i) {
    handlers->Append(i->Encode());
  }
  return handlers;
}

void ProtocolHandlerRegistry::NotifyChanged() {
  for (auto& observer : observers_)
    observer.OnProtocolHandlerRegistryChanged();
}

bool ProtocolHandlerRegistry::RegisterProtocolHandler(
    const ProtocolHandler& handler,
    const HandlerSource source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(CanSchemeBeOverridden(handler.protocol()));
  DCHECK(!handler.IsEmpty());

  // Ignore invalid handlers.
  if (!handler.IsValid())
    return false;

  ProtocolHandlerMultiMap& map =
      (source == POLICY) ? policy_protocol_handlers_ : user_protocol_handlers_;
  ProtocolHandlerList& list = map[handler.protocol()];
  if (!HandlerExists(handler, list))
    list.push_back(handler);
  if (IsRegistered(handler)) {
    return true;
  }
  if (enabled_ && !delegate_->IsExternalHandlerRegistered(handler.protocol()))
    delegate_->RegisterExternalHandler(handler.protocol());
  InsertHandler(handler);
  return true;
}

std::vector<const base::DictionaryValue*>
ProtocolHandlerRegistry::GetHandlersFromPref(const char* pref_name) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<const base::DictionaryValue*> result;
  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  if (!prefs->HasPrefPath(pref_name)) {
    return result;
  }

  const base::ListValue* handlers = prefs->GetList(pref_name);
  if (handlers) {
    for (size_t i = 0; i < handlers->GetSize(); ++i) {
      const base::DictionaryValue* dict;
      if (!handlers->GetDictionary(i, &dict))
        continue;
      if (ProtocolHandler::IsValidDict(dict)) {
        result.push_back(dict);
      }
    }
  }
  return result;
}

void ProtocolHandlerRegistry::RegisterProtocolHandlersFromPref(
    const char* pref_name,
    const HandlerSource source) {
  std::vector<const base::DictionaryValue*> registered_handlers =
      GetHandlersFromPref(pref_name);
  for (std::vector<const base::DictionaryValue*>::const_iterator p =
           registered_handlers.begin();
       p != registered_handlers.end();
       ++p) {
    ProtocolHandler handler = ProtocolHandler::CreateProtocolHandler(*p);
    if (!RegisterProtocolHandler(handler, source))
      continue;
    bool is_default = false;
    if ((*p)->GetBoolean("default", &is_default) && is_default) {
      SetDefault(handler);
    }
  }
}

void ProtocolHandlerRegistry::IgnoreProtocolHandler(
    const ProtocolHandler& handler,
    const HandlerSource source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProtocolHandlerList& list = (source == POLICY)
                                  ? policy_ignored_protocol_handlers_
                                  : user_ignored_protocol_handlers_;
  if (!HandlerExists(handler, list))
    list.push_back(handler);
  if (HandlerExists(handler, ignored_protocol_handlers_))
    return;
  ignored_protocol_handlers_.push_back(handler);
}

void ProtocolHandlerRegistry::IgnoreProtocolHandlersFromPref(
    const char* pref_name,
    const HandlerSource source) {
  std::vector<const base::DictionaryValue*> ignored_handlers =
      GetHandlersFromPref(pref_name);
  for (std::vector<const base::DictionaryValue*>::const_iterator p =
           ignored_handlers.begin();
       p != ignored_handlers.end();
       ++p) {
    IgnoreProtocolHandler(ProtocolHandler::CreateProtocolHandler(*p), source);
  }
}

bool ProtocolHandlerRegistry::HandlerExists(const ProtocolHandler& handler,
                                            ProtocolHandlerMultiMap* map) {
  return HandlerExists(handler, (*map)[handler.protocol()]);
}

bool ProtocolHandlerRegistry::HandlerExists(const ProtocolHandler& handler,
                                            const ProtocolHandlerList& list) {
  return base::Contains(list, handler);
}

void ProtocolHandlerRegistry::EraseHandler(const ProtocolHandler& handler,
                                           ProtocolHandlerMultiMap* map) {
  EraseHandler(handler, &(*map)[handler.protocol()]);
}

void ProtocolHandlerRegistry::EraseHandler(const ProtocolHandler& handler,
                                           ProtocolHandlerList* list) {
  list->erase(std::find(list->begin(), list->end(), handler));
}

void ProtocolHandlerRegistry::OnSetAsDefaultProtocolClientFinished(
    const std::string& protocol,
    shell_integration::DefaultWebClientState state) {
  // Clear if the default protocol client isn't this installation.
  if (ShouldRemoveHandlersNotInOS() &&
      (state == shell_integration::NOT_DEFAULT ||
       state == shell_integration::OTHER_MODE_IS_DEFAULT)) {
    ClearDefault(protocol);
  }
}

void ProtocolHandlerRegistry::AddPredefinedHandler(
    const ProtocolHandler& handler) {
  DCHECK(!is_loaded_);  // Must be called prior InitProtocolSettings.
  RegisterProtocolHandler(handler, USER);
  SetDefault(handler);
  predefined_protocol_handlers_.push_back(handler);
}

shell_integration::DefaultWebClientWorkerCallback
ProtocolHandlerRegistry::GetDefaultWebClientCallback(
    const std::string& protocol) {
  return base::Bind(
      &ProtocolHandlerRegistry::OnSetAsDefaultProtocolClientFinished,
      weak_ptr_factory_.GetWeakPtr(), protocol);
}
