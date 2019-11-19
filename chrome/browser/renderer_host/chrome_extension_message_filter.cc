// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_extension_message_filter.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/message_bundle.h"

using content::BrowserThread;

namespace {

const uint32_t kExtensionFilteredMessageClasses[] = {
    ExtensionMsgStart,
};

// Logs an action to the extension activity log for the specified profile.
void AddActionToExtensionActivityLog(Profile* profile,
                                     extensions::ActivityLog* activity_log,
                                     scoped_refptr<extensions::Action> action) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the action included a URL, check whether it is for an incognito
  // profile.  The check is performed here so that it can safely be done from
  // the UI thread.
  if (action->page_url().is_valid() || !action->page_title().empty())
    action->set_page_incognito(profile->IsOffTheRecord());
  activity_log->LogAction(action);
}

}  // namespace

ChromeExtensionMessageFilter::ChromeExtensionMessageFilter(
    int render_process_id,
    Profile* profile)
    : BrowserMessageFilter(kExtensionFilteredMessageClasses,
                           base::size(kExtensionFilteredMessageClasses)),
      render_process_id_(render_process_id),
      profile_(profile),
      activity_log_(extensions::ActivityLog::GetInstance(profile)),
      extension_info_map_(
          extensions::ExtensionSystem::Get(profile)->info_map()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observed_profiles_.Add(profile);
}

ChromeExtensionMessageFilter::~ChromeExtensionMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

bool ChromeExtensionMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeExtensionMessageFilter, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ExtensionHostMsg_GetMessageBundle,
                                    OnGetExtMessageBundle)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddAPIActionToActivityLog,
                        OnAddAPIActionToExtensionActivityLog);
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddDOMActionToActivityLog,
                        OnAddDOMActionToExtensionActivityLog);
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddEventToActivityLog,
                        OnAddEventToExtensionActivityLog);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromeExtensionMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionHostMsg_GetMessageBundle::ID:
    case ExtensionHostMsg_AddAPIActionToActivityLog::ID:
    case ExtensionHostMsg_AddDOMActionToActivityLog::ID:
    case ExtensionHostMsg_AddEventToActivityLog::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void ChromeExtensionMessageFilter::OnDestruct() const {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    delete this;
  } else {
    base::DeleteSoon(FROM_HERE, {BrowserThread::UI}, this);
  }
}

void ChromeExtensionMessageFilter::OnGetExtMessageBundle(
    const std::string& extension_id, IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const extensions::ExtensionSet& extension_set =
      extensions::ExtensionRegistry::Get(profile_)->enabled_extensions();
  const extensions::Extension* extension = extension_set.GetByID(extension_id);

  if (!extension) {  // The extension has gone.
    ExtensionHostMsg_GetMessageBundle::WriteReplyParams(
        reply_msg, extensions::MessageBundle::SubstitutionMap());
    Send(reply_msg);
    return;
  }

  const std::string& default_locale =
      extensions::LocaleInfo::GetDefaultLocale(extension);
  if (default_locale.empty()) {
    // A little optimization: send the answer here to avoid an extra thread hop.
    std::unique_ptr<extensions::MessageBundle::SubstitutionMap> dictionary_map(
        extensions::file_util::LoadNonLocalizedMessageBundleSubstitutionMap(
            extension_id));
    ExtensionHostMsg_GetMessageBundle::WriteReplyParams(reply_msg,
                                                        *dictionary_map);
    Send(reply_msg);
    return;
  }

  std::vector<base::FilePath> paths_to_load;
  paths_to_load.push_back(extension->path());

  auto imports = extensions::SharedModuleInfo::GetImports(extension);
  // Iterate through the imports in reverse.  This will allow later imported
  // modules to override earlier imported modules, as the list order is
  // maintained from the definition in manifest.json of the imports.
  for (auto it = imports.rbegin(); it != imports.rend(); ++it) {
    const extensions::Extension* imported_extension =
        extension_set.GetByID(it->extension_id);
    if (!imported_extension) {
      NOTREACHED() << "Missing shared module " << it->extension_id;
      continue;
    }
    paths_to_load.push_back(imported_extension->path());
  }

  // This blocks tab loading. Priority is inherited from the calling context.
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&ChromeExtensionMessageFilter::OnGetExtMessageBundleAsync,
                     this, paths_to_load, extension_id, default_locale,
                     reply_msg));
}

void ChromeExtensionMessageFilter::OnGetExtMessageBundleAsync(
    const std::vector<base::FilePath>& extension_paths,
    const std::string& main_extension_id,
    const std::string& default_locale,
    IPC::Message* reply_msg) {
  std::unique_ptr<extensions::MessageBundle::SubstitutionMap> dictionary_map(
      extensions::file_util::LoadMessageBundleSubstitutionMapFromPaths(
          extension_paths, main_extension_id, default_locale));

  ExtensionHostMsg_GetMessageBundle::WriteReplyParams(reply_msg,
                                                      *dictionary_map);
  Send(reply_msg);
}

void ChromeExtensionMessageFilter::OnAddAPIActionToExtensionActivityLog(
    const std::string& extension_id,
    const ExtensionHostMsg_APIActionOrEvent_Params& params) {
  if (!ShouldLogExtensionAction(extension_id))
    return;

  scoped_refptr<extensions::Action> action = new extensions::Action(
      extension_id, base::Time::Now(), extensions::Action::ACTION_API_CALL,
      params.api_call);
  action->set_args(base::WrapUnique(params.arguments.DeepCopy()));
  if (!params.extra.empty()) {
    action->mutable_other()->SetString(
        activity_log_constants::kActionExtra, params.extra);
  }
  AddActionToExtensionActivityLog(profile_, activity_log_, action);
}

void ChromeExtensionMessageFilter::OnAddDOMActionToExtensionActivityLog(
    const std::string& extension_id,
    const ExtensionHostMsg_DOMAction_Params& params) {
  if (!ShouldLogExtensionAction(extension_id))
    return;

  scoped_refptr<extensions::Action> action = new extensions::Action(
      extension_id, base::Time::Now(), extensions::Action::ACTION_DOM_ACCESS,
      params.api_call);
  action->set_args(base::WrapUnique(params.arguments.DeepCopy()));
  action->set_page_url(params.url);
  action->set_page_title(base::UTF16ToUTF8(params.url_title));
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      params.call_type);
  AddActionToExtensionActivityLog(profile_, activity_log_, action);
}

void ChromeExtensionMessageFilter::OnAddEventToExtensionActivityLog(
    const std::string& extension_id,
    const ExtensionHostMsg_APIActionOrEvent_Params& params) {
  if (!ShouldLogExtensionAction(extension_id))
    return;

  scoped_refptr<extensions::Action> action = new extensions::Action(
      extension_id, base::Time::Now(), extensions::Action::ACTION_API_EVENT,
      params.api_call);
  action->set_args(base::WrapUnique(params.arguments.DeepCopy()));
  if (!params.extra.empty()) {
    action->mutable_other()->SetString(activity_log_constants::kActionExtra,
                                       params.extra);
  }
  AddActionToExtensionActivityLog(profile_, activity_log_, action);
}

void ChromeExtensionMessageFilter::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile_, profile);
  observed_profiles_.Remove(profile_);
  profile_ = nullptr;
  activity_log_ = nullptr;
}

bool ChromeExtensionMessageFilter::ShouldLogExtensionAction(
    const std::string& extension_id) const {
  // We only send these IPCs if activity logging is enabled, but due to race
  // conditions (e.g. logging gets disabled but the renderer sends the message
  // before it gets updated), we still need this check here.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return profile_ &&
         g_browser_process->profile_manager()->IsValidProfile(profile_) &&
         activity_log_ && activity_log_->ShouldLog(extension_id);
}
