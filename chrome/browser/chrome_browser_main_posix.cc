// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_posix.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <sys/resource.h>

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/app/shutdown_signal_handlers_posix.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/sessions/session_restore.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/result_codes.h"

using content::BrowserThread;

namespace {

// See comment in |PreEarlyInitialization()|, where sigaction is called.
void SIGCHLDHandler(int signal) {
}

// ExitHandler takes care of servicing an exit (from a signal) at the
// appropriate time. Specifically if we get an exit and have not finished
// session restore we delay the exit. To do otherwise means we're exiting part
// way through startup which causes all sorts of problems.
class ExitHandler {
 public:
  // Invokes exit when appropriate.
  static void ExitWhenPossibleOnUIThread();

 private:
  ExitHandler();
  ~ExitHandler();

  // Called when a session restore has finished.
  void OnSessionRestoreDone(int num_tabs_restored);

  // Does the appropriate call to Exit.
  static void Exit();

  // Points to the on-session-restored callback that was registered with
  // SessionRestore's callback list. When objects of this class are destroyed,
  // the subscription object's destructor will automatically unregister the
  // callback in SessionRestore, so that the callback list does not contain any
  // obsolete callbacks.
  SessionRestore::CallbackSubscription
      on_session_restored_callback_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ExitHandler);
};

// static
void ExitHandler::ExitWhenPossibleOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (SessionRestore::IsRestoringSynchronously()) {
    // ExitHandler takes care of deleting itself.
    new ExitHandler();
  } else {
    Exit();
  }
}

ExitHandler::ExitHandler() {
  on_session_restored_callback_subscription_ =
      SessionRestore::RegisterOnSessionRestoredCallback(
          base::Bind(&ExitHandler::OnSessionRestoreDone,
                     base::Unretained(this)));
}

ExitHandler::~ExitHandler() {
}

void ExitHandler::OnSessionRestoreDone(int /* num_tabs */) {
  if (!SessionRestore::IsRestoringSynchronously()) {
    // At this point the message loop may not be running (meaning we haven't
    // gotten through browser startup, but are close). Post the task to at which
    // point the message loop is running.
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(&ExitHandler::Exit));
    delete this;
  }
}

// static
void ExitHandler::Exit() {
#if defined(OS_CHROMEOS)
  // On ChromeOS, exiting on signal should be always clean.
  chrome::ExitCleanly();
#else
  chrome::AttemptExit();
#endif
}

}  // namespace

// ChromeBrowserMainPartsPosix -------------------------------------------------

ChromeBrowserMainPartsPosix::ChromeBrowserMainPartsPosix(
    const content::MainFunctionParams& parameters,
    ChromeFeatureListCreator* chrome_feature_list_creator)
    : ChromeBrowserMainParts(parameters,
                             chrome_feature_list_creator) {}

int ChromeBrowserMainPartsPosix::PreEarlyInitialization() {
  const int result = ChromeBrowserMainParts::PreEarlyInitialization();
  if (result != service_manager::RESULT_CODE_NORMAL_EXIT)
    return result;

  // We need to accept SIGCHLD, even though our handler is a no-op because
  // otherwise we cannot wait on children. (According to POSIX 2001.)
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIGCHLDHandler;
  CHECK(sigaction(SIGCHLD, &action, NULL) == 0);

  return service_manager::RESULT_CODE_NORMAL_EXIT;
}

void ChromeBrowserMainPartsPosix::PostMainMessageLoopStart() {
  ChromeBrowserMainParts::PostMainMessageLoopStart();

  // Exit in response to SIGINT, SIGTERM, etc.
  InstallShutdownSignalHandlers(
      base::Bind(&ExitHandler::ExitWhenPossibleOnUIThread),
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}));
}

void ChromeBrowserMainPartsPosix::ShowMissingLocaleMessageBox() {
#if defined(OS_CHROMEOS)
  NOTREACHED();  // Should not ever happen on ChromeOS.
#elif defined(OS_MACOSX)
  // Not called on Mac because we load the locale files differently.
  NOTREACHED();
#elif defined(USE_AURA)
  // TODO(port): We may want a views based message dialog here eventually, but
  // for now, crash.
  NOTREACHED();
#else
#error "Need MessageBox implementation."
#endif
}
