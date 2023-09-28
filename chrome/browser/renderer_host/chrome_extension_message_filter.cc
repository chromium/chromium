// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_extension_message_filter.h"

#include <stdint.h>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/l10n_file_util.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/message_bundle.h"

using content::BrowserThread;

namespace {

const uint32_t kExtensionFilteredMessageClasses[] = {
    ExtensionMsgStart,
};

}  // namespace

ChromeExtensionMessageFilter::ChromeExtensionMessageFilter(Profile* profile)
    : BrowserMessageFilter(kExtensionFilteredMessageClasses,
                           std::size(kExtensionFilteredMessageClasses)),
      profile_(profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observed_profile_.Observe(profile);
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromeExtensionMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionHostMsg_GetMessageBundle::ID:
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
    content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
  }
}

void ChromeExtensionMessageFilter::OnGetExtMessageBundle(
    const std::string& extension_id, IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The profile may have been destroyed during the hop from the background
  // thread to the UI thread.
  if (!profile_)
    return;

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
        extensions::l10n_file_util::
            LoadNonLocalizedMessageBundleSubstitutionMap(extension_id));
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
  for (const extensions::SharedModuleInfo::ImportInfo& import :
       base::Reversed(imports)) {
    const extensions::Extension* imported_extension =
        extension_set.GetByID(import.extension_id);
    if (!imported_extension) {
      NOTREACHED() << "Missing shared module " << import.extension_id;
      continue;
    }
    paths_to_load.push_back(imported_extension->path());
  }

  // This blocks tab loading. Priority is inherited from the calling context.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &ChromeExtensionMessageFilter::OnGetExtMessageBundleAsync, this,
          paths_to_load, extension_id, default_locale,
          extension_l10n_util::GetGzippedMessagesPermissionForExtension(
              extension),
          reply_msg));
}

void ChromeExtensionMessageFilter::OnGetExtMessageBundleAsync(
    const std::vector<base::FilePath>& extension_paths,
    const std::string& main_extension_id,
    const std::string& default_locale,
    extension_l10n_util::GzippedMessagesPermission gzip_permission,
    IPC::Message* reply_msg) {
  std::unique_ptr<extensions::MessageBundle::SubstitutionMap> dictionary_map(
      extensions::l10n_file_util::LoadMessageBundleSubstitutionMapFromPaths(
          extension_paths, main_extension_id, default_locale, gzip_permission));

  ExtensionHostMsg_GetMessageBundle::WriteReplyParams(reply_msg,
                                                      *dictionary_map);
  Send(reply_msg);
}

void ChromeExtensionMessageFilter::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile_, profile);
  DCHECK(observed_profile_.IsObservingSource(profile_.get()));
  observed_profile_.Reset();
  profile_ = nullptr;
}
