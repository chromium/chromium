// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_EXTENSION_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_EXTENSION_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)

// This class filters out incoming Chrome-specific IPC messages from the
// extension process on the IPC thread.
class ChromeExtensionMessageFilter : public content::BrowserMessageFilter,
                                     public ProfileObserver {
 public:
  explicit ChromeExtensionMessageFilter(Profile* profile);

  ChromeExtensionMessageFilter(const ChromeExtensionMessageFilter&) = delete;
  ChromeExtensionMessageFilter& operator=(const ChromeExtensionMessageFilter&) =
      delete;

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
      extension_l10n_util::GzippedMessagesPermission gzip_permission,
      IPC::Message* reply_msg);

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // The Profile associated with our renderer process.  This should only be
  // accessed on the UI thread! Furthermore since this class is refcounted it
  // may outlive |profile_|, so make sure to NULL check if in doubt; async
  // calls and the like.
  raw_ptr<Profile> profile_;

  base::ScopedObservation<Profile, ProfileObserver> observed_profile_{this};
};
#endif

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_EXTENSION_MESSAGE_FILTER_H_
