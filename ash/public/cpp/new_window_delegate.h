// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NEW_WINDOW_DELEGATE_H_
#define ASH_PUBLIC_CPP_NEW_WINDOW_DELEGATE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/bind.h"

class GURL;

namespace aura {
class Window;
}

namespace base {
class FilePath;
}

namespace ui {
class OSExchangeData;
}

namespace ash {

// A delegate interface that an ash user sends to ash to handle certain window
// management responsibilities.
class ASH_PUBLIC_EXPORT NewWindowDelegate {
 public:
  // Sources of feedback requests.
  enum FeedbackSource {
    kFeedbackSourceAsh,
    kFeedbackSourceAssistant,
    kFeedbackSourceQuickAnswers,
    kFeedbackSourceChannelIndicator,
  };

  // Returns an instance connected to ash-chrome.
  static NewWindowDelegate* GetInstance();

  // DEPRECATED: This method is no longer useful after Lacros deprecation. This
  // now returns the same value as `GetInstance` but will be removed soon.
  // TODO(b/367844818): Remove this.
  static NewWindowDelegate* GetPrimary();

  // Invoked when the user uses Ctrl+T to open a new tab.
  virtual void NewTab() = 0;

  // Invoked when the user uses Ctrl-N or Ctrl-Shift-N to open a new window. If
  // the |should_trigger_session_restore| is true, a new window opening should
  // be treated like the start of a session (with potential session restore,
  // startup URLs, etc.). Otherwise, don't restore the session.
  virtual void NewWindow(bool incognito,
                         bool should_trigger_session_restore) = 0;

  using NewWindowForDetachingTabCallback =
      base::OnceCallback<void(aura::Window*)>;

  // Opens a new Browser window in response to a drag'n drop operation performed
  // by the user while in "tablet mode".
  virtual void NewWindowForDetachingTab(
      aura::Window* source_window,
      const ui::OSExchangeData& drop_data,
      NewWindowForDetachingTabCallback closure) = 0;

  // Opens the specified URL in a new tab.
  // If the |from| is kUserInteraction then the page will load with a user
  // activation. This means the page will be able to autoplay media without
  // restriction.
  // If the |from| is kArc, then the new window is annotated a special tag,
  // so that on requesting to opening ARC app from the page, confirmation
  // dialog will be skipped.
  // |Disposition| corresponds to the subset of |WindowOpenDisposition| that is
  // supported by crosapi.
  enum class OpenUrlFrom {
    kUnspecified,
    kUserInteraction,
    kArc,
  };
  enum class Disposition {
    kNewForegroundTab,
    kNewWindow,
    kOffTheRecord,
    kSwitchToTab,
  };
  virtual void OpenUrl(const GURL& url,
                       OpenUrlFrom from,
                       Disposition disposition) = 0;

  // Invoked when an accelerator (calculator key) is used to open calculator.
  virtual void OpenCalculator() = 0;

  // Invoked when an accelerator is used to open the file manager.
  virtual void OpenFileManager() = 0;

  // Opens File Manager in the MyFiles/Downloads folder.
  virtual void OpenDownloadsFolder() = 0;

  // Invoked when the user opens Crosh.
  virtual void OpenCrosh() = 0;

  // Invoked when an accelerator is used to open diagnostics.
  virtual void OpenDiagnostics() = 0;

  // Invoked when an accelerator is used to open help center.
  virtual void OpenGetHelp() = 0;

  // Invoked when the user uses Shift+Ctrl+T to restore the closed tab.
  virtual void RestoreTab() = 0;

  // Show the shortcut customization app.
  virtual void ShowShortcutCustomizationApp() = 0;

  // Shows the task manager window.
  virtual void ShowTaskManager() = 0;

  // Opens the feedback page for "Report Issue".
  // |source| indicates the source of the feedback request, which is Ash by
  // default.
  // |description_template| is the preset description when the feedback dialog
  // is opened.
  virtual void OpenFeedbackPage(
      FeedbackSource source = kFeedbackSourceAsh,
      const std::string& description_template = std::string()) = 0;

  // Show the Personalization hub.
  virtual void OpenPersonalizationHub() = 0;

  // Shows the a captive portal signin window.
  virtual void OpenCaptivePortalSignin(const GURL& url) = 0;

  // Opens a file on the local file system (which may be DriveFS).
  virtual void OpenFile(const base::FilePath& file_path) = 0;

 protected:
  NewWindowDelegate();
  NewWindowDelegate(const NewWindowDelegate&) = delete;
  NewWindowDelegate& operator=(const NewWindowDelegate&) = delete;
  virtual ~NewWindowDelegate();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NEW_WINDOW_DELEGATE_H_
