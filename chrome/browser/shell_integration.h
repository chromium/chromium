// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_H_
#define CHROME_BROWSER_SHELL_INTEGRATION_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
}

namespace shell_integration {

// Sets Chrome as the default browser (only for the current user).
//
// Don't use this, because:
//   - This does not work on Windows version 8 or higher.
//   - This cannot provide feedback as to success because setting a default
//     browser is asynchronous.
//
// Use `DefaultBrowserWorker` instead.
// TODO(crbug.com/40248220): Extend `DefaultBrowserWorker` to work better
// on the Mac and remove this function.
bool SetAsDefaultBrowser();

// Sets Chrome as the default client application for the given scheme (only
// for the current user). Prefer to use the `DefaultSchemeClientWorker` class
// below since it works on all OSs.
//
// TODO(crbug.com/40248220): Extend `DefaultSchemeClientWorker` to work
// better on the Mac and remove this function.
bool SetAsDefaultClientForScheme(const std::string& scheme);

// The different types of permissions required to set a default web client.
enum DefaultWebClientSetPermission {
  // The browser distribution is not permitted to be made default.
  SET_DEFAULT_NOT_ALLOWED,
  // No special permission or interaction is required to set the default
  // browser. This is used in Linux and Windows 7 and under. This is returned
  // for compatibility on the Mac, even though the Mac requires interaction.
  // TODO(crbug.com/40248220): Fix this.
  SET_DEFAULT_UNATTENDED,
  // On the Mac and on Windows 8+, a browser can be made default only in an
  // interactive flow. This value is returned for Windows 8+.
  // TODO(crbug.com/40248220): Fix it so that this value is also returned
  // on the Mac.
  SET_DEFAULT_INTERACTIVE,
};

// Returns requirements for making the running browser the default browser.
DefaultWebClientSetPermission GetDefaultBrowserSetPermission();

// Returns requirements for making the running browser the default client
// application for specific schemes outside of the default browser.
DefaultWebClientSetPermission GetDefaultSchemeClientSetPermission();

// Returns true if the running browser can be set as the default browser,
// whether user interaction is needed or not. Use
// GetDefaultWebClientSetPermission() if this distinction is important.
bool CanSetAsDefaultBrowser();

// Returns a string representing the application to be launched given the
// scheme of the requested url. This string may be a name or a path, but
// neither is guaranteed and it should only be used as a display string.
// Returns an empty string on failure.
std::u16string GetApplicationNameForScheme(const GURL& url);

#if BUILDFLAG(IS_MAC)
// Returns a vector which containing all the application paths that can be used
// to launch the requested URL.
// Returns an empty vector if no application is found.
std::vector<base::FilePath> GetAllApplicationPathsForURL(const GURL& url);

// Returns true if the application at `path` can be used to launch the given
// `url`.
bool CanApplicationHandleURL(const base::FilePath& app_path, const GURL& url);
#endif

// Chrome's default web client state as a browser as a scheme client. If the
// current install mode is not default, the brand's other modes are
// checked. This allows callers to take specific action in case the current mode
// (e.g., Chrome Dev) is not the default handler, but another of the brand's
// modes (e.g., stable Chrome) is.
enum DefaultWebClientState {
  // No install mode for the brand is the default client.
  NOT_DEFAULT,
  // The current install mode is the default client.
  IS_DEFAULT,
  // An error occurred while attempting to check the default client.
  UNKNOWN_DEFAULT,
  // The current install mode is not default, although one of the brand's
  // other install modes is.
  OTHER_MODE_IS_DEFAULT,
  NUM_DEFAULT_STATES
};

// Attempt to determine if this instance of Chrome is the default browser and
// return the appropriate state. (Defined as being the handler for HTTP/HTTPS
// schemes; we don't want to report "no" here if the user has simply chosen
// to open HTML files in a text editor and FTP links with an FTP client.)
DefaultWebClientState GetDefaultBrowser();

// Returns true if Firefox is likely to be the default browser for the current
// user. This method is very fast so it can be invoked in the UI thread.
bool IsFirefoxDefaultBrowser();

#if BUILDFLAG(IS_WIN)
// Returns true if IE is likely to be the default browser for the current
// user. This method is very fast so it can be invoked in the UI thread.
bool IsIEDefaultBrowser();

// Returns the install id of the installation set as default browser. If no
// installation of Firefox is set as the default browser, returns an empty
// string.
std::string GetFirefoxProgIdSuffix();
#endif

// Attempt to determine if this instance of Chrome is the default client
// application for the given scheme and return the appropriate state.
DefaultWebClientState IsDefaultClientForScheme(const std::string& scheme);

// Is the current instance of Chrome running in App mode.
bool IsRunningInAppMode();

// Set up command line arguments for launching a URL or an app.
// The new command line reuses the current process's user data directory (and
// login profile, for ChromeOS).
// If |extension_app_id| is non-empty, the arguments use kAppId=<id>.
// Otherwise, kApp=<url> is used.
base::CommandLine CommandLineArgsForLauncher(
    const GURL& url,
    const std::string& extension_app_id,
    const base::FilePath& profile_path,
    const std::string& run_on_os_login_mode);

// Set up command line arguments for launching chrome at the given url using the
// given profile. All arguments must be non-empty and valid.
base::CommandLine CommandLineArgsForUrlShortcut(
    const base::FilePath& chrome_exe_program,
    const base::FilePath& profile_path,
    const GURL& url);

// Append command line arguments for launching a new chrome.exe process
// based on the current process.
// The new command line reuses the current process's user data directory and
// profile.
void AppendProfileArgs(const base::FilePath& profile_path,
                       base::CommandLine* command_line);

#if !BUILDFLAG(IS_WIN)
// Gets the name of the Chrome Apps menu folder in which to place app
// shortcuts. This is needed for Mac and Linux.
std::u16string GetAppShortcutsSubdirName();
#endif

// The type of callback used to communicate processing state to consumers of
// DefaultBrowserWorker and DefaultSchemeClientWorker.
using DefaultWebClientWorkerCallback =
    base::OnceCallback<void(DefaultWebClientState)>;

// The type of callback used to communicate processing state to consumers of
// DefaultBrowserWorker and DefaultSchemeClientWorker.
using DefaultSchemeHandlerWorkerCallback =
    base::OnceCallback<void(DefaultWebClientState, const std::u16string&)>;

//  Helper objects that handle checking if Chrome is the default browser
//  or application for a url scheme on Windows and Linux, and also setting
//  it as the default. These operations are performed asynchronously on a
//  blocking sequence since registry access (on Windows) or the preference
//  database (on Linux) are involved and this can be slow.
//  By default, the worker will present the user with an interactive flow if
//  required by the platform. This can be suppressed via
//  set_interactive_permitted(), in which case an attempt to set Chrome as
//  the default handler will silently fail on such platforms.
class DefaultWebClientWorker
    : public base::RefCountedThreadSafe<DefaultWebClientWorker> {
 public:
  DefaultWebClientWorker(const DefaultWebClientWorker&) = delete;
  DefaultWebClientWorker& operator=(const DefaultWebClientWorker&) = delete;

  // Controls whether the worker can use user interaction to set the default
  // web client. If false, the set-as-default operation will fail on OS where
  // it is required.
  void set_interactive_permitted(bool interactive_permitted) {
    interactive_permitted_ = interactive_permitted;
  }

  // Checks to see if Chrome is the default web client application. The
  // provided callback will be run to communicate the default state to the
  // caller.
  void StartCheckIsDefault(DefaultWebClientWorkerCallback callback);

  // Sets Chrome as the default web client application. Once done, it will
  // trigger a check for the default state using StartCheckIsDefault() to return
  // the default state to the caller.
  void StartSetAsDefault(DefaultWebClientWorkerCallback callback);

 protected:
  friend class base::RefCountedThreadSafe<DefaultWebClientWorker>;

  explicit DefaultWebClientWorker(const char* worker_name);
  virtual ~DefaultWebClientWorker();

  // Communicates the result via |callback|. When
  // |is_following_set_as_default| is true, |state| will be reported to UMA as
  // the result of the set-as-default operation.
  void OnCheckIsDefaultComplete(DefaultWebClientState state,
                                bool is_following_set_as_default,
                                DefaultWebClientWorkerCallback callback);

  // When false, the operation to set as default will fail for interactive
  // flows.
  bool interactive_permitted_ = true;

 private:
  // Checks whether Chrome is the default web client. Always called on a
  // blocking sequence. When |is_following_set_as_default| is true, The default
  // state will be reported to UMA as the result of the set-as-default
  // operation.
  void CheckIsDefault(bool is_following_set_as_default,
                      DefaultWebClientWorkerCallback callback);

  // Sets Chrome as the default web client. Always called on a blocking
  // sequence.
  void SetAsDefault(DefaultWebClientWorkerCallback callback);

  // Implementation of CheckIsDefault() and SetAsDefault() for subclasses.
  virtual DefaultWebClientState CheckIsDefaultImpl() = 0;

  // The callback may be run synchronously or at an arbitrary time later on this
  // thread.
  // Note: Subclasses MUST make sure |on_finished_callback| is executed.
  virtual void SetAsDefaultImpl(base::OnceClosure on_finished_callback) = 0;

  // Reports the result for the set-as-default operation.
  void ReportSetDefaultResult(DefaultWebClientState state);

  // Used to differentiate UMA metrics for setting the default browser and
  // setting the default scheme client. The pointer must be valid for the
  // lifetime of the worker.
  const char* worker_name_;
};

// Worker for checking and setting the default browser.
class DefaultBrowserWorker : public DefaultWebClientWorker {
 public:
  DefaultBrowserWorker();

  DefaultBrowserWorker(const DefaultBrowserWorker&) = delete;
  DefaultBrowserWorker& operator=(const DefaultBrowserWorker&) = delete;

  static void DisableSetAsDefaultForTesting();

 protected:
  ~DefaultBrowserWorker() override;

 private:
  // Check if Chrome is the default browser.
  DefaultWebClientState CheckIsDefaultImpl() override;

  // Set Chrome as the default browser.
  void SetAsDefaultImpl(base::OnceClosure on_finished_callback) override;

  static bool g_disable_set_as_default_for_testing;
};

// Worker for checking and setting the default client application
// for a given scheme. A different worker instance is needed for each
// scheme you are interested in, so to check or set the default for
// multiple scheme you should use multiple worker objects.
class DefaultSchemeClientWorker : public DefaultWebClientWorker {
 public:
  explicit DefaultSchemeClientWorker(const std::string& scheme);
  explicit DefaultSchemeClientWorker(const GURL& url);

  DefaultSchemeClientWorker(const DefaultSchemeClientWorker&) = delete;
  DefaultSchemeClientWorker& operator=(const DefaultSchemeClientWorker&) =
      delete;

  // Checks to see if Chrome is the default application for the |url_|.
  // The provided callback will be run to communicate the default state to the
  // caller, and also return the name of the default client if available.
  void StartCheckIsDefaultAndGetDefaultClientName(
      DefaultSchemeHandlerWorkerCallback callback);

  const std::string& scheme() const { return scheme_; }
  const GURL& url() const { return url_; }

 protected:
  ~DefaultSchemeClientWorker() override;

  // Communicates the result via |callback|.
  void OnCheckIsDefaultAndGetDefaultClientNameComplete(
      DefaultWebClientState state,
      std::u16string program_name,
      DefaultSchemeHandlerWorkerCallback callback);

 private:
  // Checks whether Chrome is the default client for |url_|. This also returns
  // the default client name if available.
  void CheckIsDefaultAndGetDefaultClientName(
      DefaultSchemeHandlerWorkerCallback callback);

  // Check if Chrome is the default handler for this scheme.
  DefaultWebClientState CheckIsDefaultImpl() override;

  // Gets the default client name for |scheme_|. Always called on a blocking
  // sequence.
  virtual std::u16string GetDefaultClientNameImpl();

  // Set Chrome as the default handler for this scheme.
  void SetAsDefaultImpl(base::OnceClosure on_finished_callback) override;

  const std::string scheme_;
  const GURL url_;
};

namespace internal {

// The different ways to set the default web client.
enum class WebClientSetMethod {
  // Method to set the default browser.
  kDefaultBrowser,

  // Method to set a default scheme handler outside of default browser.
  kDefaultSchemeHandler,
};

// Returns requirements for making the running browser either the default
// browser or the default client application for specific schemes for the
// current user, according to a specific platform.
DefaultWebClientSetPermission GetPlatformSpecificDefaultWebClientSetPermission(
    WebClientSetMethod method);

}  // namespace internal
}  // namespace shell_integration

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_H_
