// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_HANDLER_H_

#include <optional>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "ui/base/page_transition_types.h"

namespace content {
class WeakDocumentPtr;
}

namespace url {
class Origin;
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
  // append-only.
  // This metric is related to BlockState but is only used for metrics
  // reporting, and it differentiates multiple possible states that map to
  // BlockState.
  enum class BlockStateMetric {
    kDeniedDefault,
    kAllowedDefaultMail,
    kAllowedDefaultNews_Deprecated,  // No longer emitted.
    kNewsNotDefault_Deprecated,      // No longer emitted.
    kAllowedByEnterprisePolicy,
    kAllowedByPreference,
    kPrompt,
    // Insert new metric values above this line and update kMaxValue.
    kMaxValue = kPrompt,
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
    virtual scoped_refptr<shell_integration::DefaultSchemeClientWorker>
    CreateShellWorker(const GURL& url) = 0;
    virtual BlockState GetBlockState(const std::string& scheme,
                                     Profile* profile) = 0;
    virtual void BlockRequest() = 0;
    virtual void RunExternalProtocolDialog(
        const GURL& url,
        content::WebContents* web_contents,
        ui::PageTransition page_transition,
        bool has_user_gesture,
        const std::optional<url::Origin>& initiating_origin,
        const std::u16string& program_name) = 0;
    virtual void LaunchUrlWithoutSecurityCheck(
        const GURL& url,
        content::WebContents* web_contents) = 0;
    virtual void FinishedProcessingCheck() = 0;

    virtual void OnSetBlockState(const std::string& scheme,
                                 const url::Origin& initiating_origin,
                                 ExternalProtocolHandler::BlockState state) {}
    virtual ~Delegate() = default;
  };

  // UMA histogram metric names.
  static const char kBlockStateMetric[];
  static const char kHandleStateMetric[];

  ExternalProtocolHandler(const ExternalProtocolHandler&) = delete;
  ExternalProtocolHandler& operator=(const ExternalProtocolHandler&) = delete;

  // Called on the UI thread. Allows switching out the
  // ExternalProtocolHandler::Delegate for testing code.
  static void SetDelegateForTesting(Delegate* delegate);

  // True if |initiating_origin| is not nullptr and is considered
  // potentially trustworthy.
  static bool MayRememberAllowDecisionsForThisOrigin(
      const url::Origin* initiating_origin);

  // Returns whether we should block a given scheme.
  // |initiating_origin| can be nullptr if the user is performing a
  // browser initiated top frame navigation, for example by typing in the
  // address bar or right-clicking a link and selecting 'Open In New Tab'.
  // Renderer-initiated navigations will set |initiating_origin| to the origin
  // of the content requesting the navigation.
  static BlockState GetBlockState(const std::string& scheme,
                                  const url::Origin* initiating_origin,
                                  Profile* profile);

  // Sets whether we should block a given scheme + origin.
  static void SetBlockState(const std::string& scheme,
                            const url::Origin& initiating_origin,
                            BlockState state,
                            Profile* profile);

  // Checks to see if the protocol is allowed, if it is allowlisted,
  // the application associated with the protocol is launched on the io thread,
  // if it is denylisted, returns silently. Otherwise, an
  // ExternalProtocolDialog is created asking the user. If the user accepts,
  // LaunchUrlWithoutSecurityCheck is called on the io thread and the
  // application is launched.
  // If possible, |initiator_document| identifies the document that requested
  // the external protocol launch.
  // Must run on the UI thread.
  static void LaunchUrl(
      const GURL& url,
      content::WebContents::Getter web_contents_getter,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      bool is_in_fenced_frame_tree,
      const std::optional<url::Origin>& initiating_origin,
      content::WeakDocumentPtr initiator_document
#if BUILDFLAG(IS_ANDROID)
      ,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory
#endif
  );

  // Starts a url using the external protocol handler with the help
  // of shellexecute. Should only be called if the protocol is allowlisted
  // (checked in LaunchUrl) or if the user explicitly allows it. (By selecting
  // "Open Application" in an ExternalProtocolDialog.) |url| might be escaped
  // already when calling into this function but e.g. from LaunchUrl but it
  // doesn't have to be because is also escaped in it.
  // NOTE: You should NOT call this function directly unless you are sure the
  // url you have has been checked against the denylist.
  // All calls to this function should originate in some way from LaunchUrl.
  static void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents,
      content::WeakDocumentPtr initiator_document);

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

#if !BUILDFLAG(IS_ANDROID)
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
  //
  // The dialog displays |initiating_origin| to the user so that they can
  // attribute the external protocol request to a site that initiated it. If an
  // opaque origin (for example, an origin inside a sandboxed iframe) initiated
  // the request, then |initiating_origin| should be set to the precursor origin
  // (that is, the origin that created the opaque origin).
  static void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      bool is_in_fenced_frame_tree,
      const std::optional<url::Origin>& initiating_origin,
      content::WeakDocumentPtr initiator_document,
      const std::u16string& program_name);
#endif

  // Clears the external protocol handling data.
  static void ClearData(Profile* profile);
};

#endif  // CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_HANDLER_H_
