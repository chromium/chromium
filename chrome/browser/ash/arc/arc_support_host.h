// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ARC_SUPPORT_HOST_H_
#define CHROME_BROWSER_ASH_ARC_ARC_SUPPORT_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/extensions/arc_support_message_host.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

class Profile;
class GURL;

// Native interface to control ARC support chrome App.
// TODO(hidehiko,lhchavez): Move this into extensions/ directory, and put it
// into "arc" namespace. Add unittests at the time.
class ArcSupportHost : public arc::ArcSupportMessageHost::Observer,
                       public display::DisplayObserver {
 public:
  enum class UIPage {
    NO_PAGE,      // Hide everything.
    TERMS,        // Terms content page.
    ARC_LOADING,  // ARC loading progress page.
    ERROR,        // ARC start error page.
  };

  // Error types whose corresponding message ARC support has.
  enum class Error {
    ANDROID_MANAGEMENT_REQUIRED_ERROR,
    NETWORK_UNAVAILABLE_ERROR,
    SERVER_COMMUNICATION_ERROR,
    SIGN_IN_BAD_AUTHENTICATION_ERROR,
    // Cloud provision flow errors require the error arg to be passed
    // in ErrorInfo struct where the value of arg is error code received
    // from ARC.
    SIGN_IN_CLOUD_PROVISION_FLOW_ACCOUNT_MISSING_ERROR,
    SIGN_IN_CLOUD_PROVISION_FLOW_DOMAIN_JOIN_FAIL_ERROR,
    SIGN_IN_CLOUD_PROVISION_FLOW_NETWORK_ERROR,
    SIGN_IN_CLOUD_PROVISION_FLOW_TRANSIENT_ERROR,
    SIGN_IN_CLOUD_PROVISION_FLOW_PERMANENT_ERROR,
    SIGN_IN_CLOUD_PROVISION_FLOW_INTERRUPTED_ERROR,
    SIGN_IN_CLOUD_PROVISION_FLOW_ENROLLMENT_TOKEN_INVALID,
    SIGN_IN_GMS_CHECKIN_ERROR,
    SIGN_IN_NETWORK_ERROR,
    SIGN_IN_GMS_SIGNIN_ERROR,
    // This error requires error arg to be passed in ErrorInfo struct
    // where the value of arg indicates the specific provisioning result
    // for which this error message is shown.
    SIGN_IN_UNKNOWN_ERROR,
    LOW_DISK_SPACE_ERROR
  };

  // A struct to represent the error to display on the screen.
  struct ErrorInfo {
    explicit ErrorInfo(Error error);
    ErrorInfo(Error error, const std::optional<int>& arg);
    ErrorInfo(const ErrorInfo&);
    ErrorInfo& operator=(const ErrorInfo&);

    // The error message to show.
    Error error;

    // Some messages show an error code with the error string (e.g. Something
    // went wrong. Error code: 7). The value of error code for such errors can
    // be passed using this arg. For SIGN_IN_UNKNOWN_ERROR the arg should be
    // specific provisioning result code. For SIGN_IN_CLOUD_PROVISION_FLOW_*
    // errors the arg should be error code received from ARC.
    std::optional<int> arg;
  };

  // Delegate to handle manual authentication related events.
  class TermsOfServiceDelegate {
   public:
    // Called when the user press AGREE button on terms of service page.
    virtual void OnTermsAgreed(bool is_metrics_enabled,
                               bool is_backup_and_restore_enabled,
                               bool is_location_service_enabled) = 0;

    // Called when the user rejects the terms of service or closes the page.
    virtual void OnTermsRejected() = 0;

    // Called when "RETRY" button on the error page is clicked during terms of
    // service negotiation.
    virtual void OnTermsRetryClicked() = 0;

    // Called when terms of service page is loaded or fails to load.
    virtual void OnTermsLoadResult(bool success) = 0;

   protected:
    virtual ~TermsOfServiceDelegate() = default;
  };

  // Delegate to handle general error events. Note that some of the callback
  // will only be called when more the specific callback in the other delegate
  // is not appropriate.
  class ErrorDelegate {
   public:
    // Called when the window is closed but only when terms of service
    // negotiation is not ongoing, in which case OnTermsRejected will be called.
    virtual void OnWindowClosed() = 0;

    // Called when "RETRY" button on the error page is clicked, except when
    // terms of service negotiation or manual authentication is ongoing. In
    // those cases, the more specific retry function in the other delegates is
    // called.
    virtual void OnRetryClicked() = 0;

    // Called when send feedback button on error page is clicked.
    virtual void OnSendFeedbackClicked() = 0;

    // Called when network tests link on error page is clicked.
    virtual void OnRunNetworkTestsClicked() = 0;

    // Called when error page is shown.
    virtual void OnErrorPageShown(bool network_tests_shown) = 0;

   protected:
    virtual ~ErrorDelegate() = default;
  };

  using RequestOpenAppCallback =
      base::RepeatingCallback<void(Profile* profile)>;

  explicit ArcSupportHost(Profile* profile);

  ArcSupportHost(const ArcSupportHost&) = delete;
  ArcSupportHost& operator=(const ArcSupportHost&) = delete;

  ~ArcSupportHost() override;

  void SetTermsOfServiceDelegate(TermsOfServiceDelegate* delegate);
  void SetErrorDelegate(ErrorDelegate* delegate);

  // Returns the should_show_run_network_tests_ field.
  bool GetShouldShowRunNetworkTests();

  // Returns the outermost native view. This will be used as the parent for
  // dialog boxes.
  gfx::NativeWindow GetNativeWindow() const;

  // Called when the communication to arc_support Chrome App is ready.
  void SetMessageHost(arc::ArcSupportMessageHost* message_host);

  // Called when the communication to arc_support Chrome App is closed.
  // The argument message_host is used to check if the given |message_host|
  // is what this instance uses know, to avoid racy case.
  // If |message_host| is different from the one this instance knows,
  // this is no op.
  void UnsetMessageHost(arc::ArcSupportMessageHost* message_host);

  // Sets the ARC managed state. This must be called before ARC support app
  // is started.
  void SetArcManaged(bool is_arc_managed);

  // Requests to close the extension window.
  void Close();

  // Requests to show the "Terms Of Service" page.
  void ShowTermsOfService();

  // Requests to show the "ARC is loading" page.
  void ShowArcLoading();

  // Requests to show the error page
  void ShowError(ErrorInfo error_info,
                 bool should_show_send_feedback,
                 bool should_show_run_network_tests);

  void SetMetricsPreferenceCheckbox(bool is_enabled, bool is_managed);
  void SetBackupAndRestorePreferenceCheckbox(bool is_enabled, bool is_managed);
  void SetLocationServicesPreferenceCheckbox(bool is_enabled, bool is_managed);

  // arc::ArcSupportMessageHost::Observer override:
  void OnMessage(const base::Value::Dict& message) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Returns current page that has to be shown in OptIn UI.
  // Note that this can be inconsistent from the actually shown page.
  // TODO(hidehiko): Remove this exposure.
  UIPage ui_page() const { return ui_page_; }

  void SetRequestOpenAppCallbackForTesting(
      const RequestOpenAppCallback& callback);

 private:
  struct PreferenceCheckboxData {
    PreferenceCheckboxData() : PreferenceCheckboxData(false, false) {}
    PreferenceCheckboxData(bool is_enabled, bool is_managed)
        : is_enabled(is_enabled), is_managed(is_managed) {}

    bool is_enabled;
    bool is_managed;
  };

  // Requests to start the ARC support Chrome app.
  void RequestAppStart();

  void SetWindowBound(const display::Display& display);

  bool Initialize();

  // Requests to ARC support Chrome app to show the specified page.
  void ShowPage(UIPage ui_page);

  // Sends a preference update to the extension.
  // The message will be
  // {
  //   'action': action_name,
  //   'enabled': is_enabled,
  //   'managed': is_managed
  // }
  void SendPreferenceCheckboxUpdate(const std::string& action_name,
                                    const PreferenceCheckboxData& data);

  void DisconnectMessageHost();

  const raw_ptr<Profile> profile_;
  RequestOpenAppCallback request_open_app_callback_;

  // Not owned.
  raw_ptr<TermsOfServiceDelegate> tos_delegate_ = nullptr;

  // Not owned.
  raw_ptr<ErrorDelegate> error_delegate_ = nullptr;

  // True, if ARC support app is requested to start, but the connection is not
  // yet established. Reset to false, when the app is started and the
  // connection to the app is established.
  bool app_start_pending_ = false;

  // The instance is created and managed by Chrome.
  raw_ptr<arc::ArcSupportMessageHost> message_host_ = nullptr;

  std::optional<display::ScopedOptionalDisplayObserver> display_observer_;

  // The lifetime of the message_host_ is out of control from ARC.
  // Fields below are UI parameter cache in case the value is set before
  // connection to the ARC support Chrome app is established.
  UIPage ui_page_ = UIPage::NO_PAGE;

  // These have valid values iff ui_page_ == ERROR.
  std::optional<ErrorInfo> error_info_;
  bool should_show_send_feedback_;
  bool should_show_run_network_tests_;

  bool is_arc_managed_ = false;

  PreferenceCheckboxData metrics_checkbox_;
  PreferenceCheckboxData backup_and_restore_checkbox_;
  PreferenceCheckboxData location_services_checkbox_;
};

#endif  // CHROME_BROWSER_ASH_ARC_ARC_SUPPORT_HOST_H_
