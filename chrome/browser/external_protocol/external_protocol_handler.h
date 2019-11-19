// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/shell_integration.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace content {
class WebContents;
}

class GURL;
class PrefRegistrySimple;
class Profile;

class ExternalProtocolHandler {
 public:
  enum BlockState {
    DONT_BLOCK,
    BLOCK,
    UNKNOWN,
  };

  // This is used to back a UMA histogram, so it should be treated as
  // append-only. Any new values should be inserted immediately prior to
  // HANDLE_STATE_LAST.
  enum HandleState {
    LAUNCH,
    CHECKED_LAUNCH,
    DONT_LAUNCH,
    CHECKED_DONT_LAUNCH_DEPRECATED,
    HANDLE_STATE_LAST
  };

  // Delegate to allow unit testing to provide different behavior.
  class Delegate {
   public:
    virtual scoped_refptr<shell_integration::DefaultProtocolClientWorker>
    CreateShellWorker(
        const shell_integration::DefaultWebClientWorkerCallback& callback,
        const std::string& protocol) = 0;
    virtual BlockState GetBlockState(const std::string& scheme,
                                     Profile* profile) = 0;
    virtual void BlockRequest() = 0;
    virtual void RunExternalProtocolDialog(
        const GURL& url,
        content::WebContents* web_contents,
        ui::PageTransition page_transition,
        bool has_user_gesture,
        const base::Optional<url::Origin>& initiating_origin) = 0;
    virtual void LaunchUrlWithoutSecurityCheck(
        const GURL& url,
        content::WebContents* web_contents) = 0;
    virtual void FinishedProcessingCheck() = 0;

    virtual void OnSetBlockState(const std::string& scheme,
                                 ExternalProtocolHandler::BlockState state) {}
    virtual ~Delegate() {}
  };

  // UMA histogram metric names.
  static const char kHandleStateMetric[];

  // Called on the UI thread. Allows switching out the
  // ExternalProtocolHandler::Delegate for testing code.
  static void SetDelegateForTesting(Delegate* delegate);

  // Returns whether we should block a given scheme.
  static BlockState GetBlockState(const std::string& scheme, Profile* profile);

  // Sets whether we should block a given scheme.
  static void SetBlockState(const std::string& scheme,
                            BlockState state,
                            Profile* profile);

  // Checks to see if the protocol is allowed, if it is allowlisted,
  // the application associated with the protocol is launched on the io thread,
  // if it is denylisted, returns silently. Otherwise, an
  // ExternalProtocolDialog is created asking the user. If the user accepts,
  // LaunchUrlWithoutSecurityCheck is called on the io thread and the
  // application is launched.
  // Must run on the UI thread.
  static void LaunchUrl(const GURL& url,
                        int render_process_host_id,
                        int render_view_routing_id,
                        ui::PageTransition page_transition,
                        bool has_user_gesture,
                        const base::Optional<url::Origin>& initiating_origin);

  // Starts a url using the external protocol handler with the help
  // of shellexecute. Should only be called if the protocol is allowlisted
  // (checked in LaunchUrl) or if the user explicitly allows it. (By selecting
  // "Open Application" in an ExternalProtocolDialog.) It is assumed that the
  // url has already been escaped, which happens in LaunchUrl.
  // NOTE: You should NOT call this function directly unless you are sure the
  // url you have has been checked against the denylist, and has been escaped.
  // All calls to this function should originate in some way from LaunchUrl.
  static void LaunchUrlWithoutSecurityCheck(const GURL& url,
                                            content::WebContents* web_contents);

  // Allows LaunchUrl to proceed with launching an external protocol handler.
  // This is typically triggered by a user gesture, but is also called for
  // each extension API function. Note that each call to LaunchUrl resets
  // the state to false (not allowed).
  static void PermitLaunchUrl();

  // Records an UMA metric for the external protocol HandleState selected, based
  // on if the check box is selected / not and block / Dont block is picked.
  static void RecordHandleStateMetrics(bool checkbox_selected,
                                       BlockState state);

  // Register the ExcludedSchemes preference.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Creates and runs a External Protocol dialog box.
  // |url| - The url of the request.
  // |render_process_host_id| and |routing_id| are used by
  // tab_util::GetWebContentsByID to aquire the tab contents associated with
  // this dialog.
  // NOTE: There is a race between the Time of Check and the Time Of Use for
  //       the command line. Since the caller (web page) does not have access
  //       to change the command line by itself, we do not do anything special
  //       to protect against this scenario.
  // This is implemented separately on each platform.
  // TODO(davidsac): Consider refactoring this to take a WebContents directly.
  // crbug.com/668289
  static void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const base::Optional<url::Origin>& initiating_origin);

  // Clears the external protocol handling data.
  static void ClearData(Profile* profile);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolHandler);
};

#endif  // CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_HANDLER_H_
