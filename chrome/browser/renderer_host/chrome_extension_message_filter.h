// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_EXTENSION_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_EXTENSION_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"

struct ExtensionHostMsg_APIActionOrEvent_Params;
struct ExtensionHostMsg_DOMAction_Params;

namespace extensions {
class ActivityLog;
class InfoMap;
struct Message;
}

// This class filters out incoming Chrome-specific IPC messages from the
// extension process on the IPC thread.
class ChromeExtensionMessageFilter : public content::BrowserMessageFilter,
                                     public ProfileObserver {
 public:
  ChromeExtensionMessageFilter(int render_process_id, Profile* profile);

  // content::BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  void OnDestruct() const override;

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<ChromeExtensionMessageFilter>;

  ~ChromeExtensionMessageFilter() override;

  // TODO(jamescook): Move these functions into the extensions module. Ideally
  // this would be in extensions::ExtensionMessageFilter but that will require
  // resolving the ActivityLog dependencies on src/chrome.
  // http://crbug.com/339637
  void OnGetExtMessageBundle(const std::string& extension_id,
                             IPC::Message* reply_msg);
  void OnGetExtMessageBundleAsync(
      const std::vector<base::FilePath>& extension_paths,
      const std::string& main_extension_id,
      const std::string& default_locale,
      IPC::Message* reply_msg);
  void OnAddAPIActionToExtensionActivityLog(
      const std::string& extension_id,
      const ExtensionHostMsg_APIActionOrEvent_Params& params);
  void OnAddBlockedCallToExtensionActivityLog(
      const std::string& extension_id,
      const std::string& function_name);
  void OnAddDOMActionToExtensionActivityLog(
      const std::string& extension_id,
      const ExtensionHostMsg_DOMAction_Params& params);
  void OnAddEventToExtensionActivityLog(
      const std::string& extension_id,
      const ExtensionHostMsg_APIActionOrEvent_Params& params);

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Returns true if an action should be logged for the given extension.
  bool ShouldLogExtensionAction(const std::string& extension_id) const;

  const int render_process_id_;

  // The Profile associated with our renderer process.  This should only be
  // accessed on the UI thread! Furthermore since this class is refcounted it
  // may outlive |profile_|, so make sure to NULL check if in doubt; async
  // calls and the like.
  Profile* profile_;

  // The ActivityLog associated with the given profile. Also only safe to
  // access on the UI thread, and may be null.
  extensions::ActivityLog* activity_log_;

  scoped_refptr<extensions::InfoMap> extension_info_map_;

  ScopedObserver<Profile, ProfileObserver> observed_profiles_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_EXTENSION_MESSAGE_FILTER_H_
