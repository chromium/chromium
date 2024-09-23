// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_
#define CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/token.h"
#include "chrome/browser/compose/compose_session.h"
#include "chrome/browser/compose/proactive_nudge_tracker.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/common/compose/compose.mojom.h"
#include "components/autofill/content/browser/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_dialog_controller.h"
#include "components/compose/core/browser/compose_manager.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

class ComposeEnabling;

// An implementation of `ComposeClient` for Desktop and Android.
class ChromeComposeClient
    : public compose::ComposeClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromeComposeClient>,
      public autofill::AutofillManager::Observer,
      public compose::mojom::ComposeClientUntrustedPageHandler,
      public compose::ProactiveNudgeTracker::Delegate,
      public ComposeSession::Observer,
      public InnerTextProvider {
 public:
  using EntryPoint = autofill::AutofillComposeDelegate::UiEntryPoint;
  class FieldChangeObserver : public autofill::AutofillManager::Observer {
   public:
    explicit FieldChangeObserver(content::WebContents* web_contents);
    ~FieldChangeObserver() override;

    // autofill::AutofillManager::Observer:
    // Used to observe field text content changes so that the proactive nudge
    // can be dismissed after a set number of change events.
    // TODO(b/40286232): Throttling of this event may be added in the future, in
    // which case this implementation would no longer adhere to a strict event
    // count.
    void OnAfterTextFieldDidChange(autofill::AutofillManager& manager,
                                   autofill::FormGlobalId form,
                                   autofill::FieldGlobalId field,
                                   const std::u16string& text_value) override;
    // Used to reset the field content changes count when a new suggestions UI
    // is shown.
    void OnSuggestionsShown(autofill::AutofillManager& manager) override;

    // Asks Autofill to hide any open compose-related popups.
    void HideComposeNudges();

    void SetSkipSuggestionTypeForTest(bool skip_suggestion_type);

    // TODO(b/343204155): SuggestionType check is skipped during testing as the
    // TestAutofillClient API does not currently support use of the Suggestions
    // field.
    bool skip_suggestion_type_for_test_ = false;

    raw_ptr<content::WebContents> web_contents_;
    // Current count of change events fired on the current focused text field,
    // as recorded by `OnAfterTextFieldDidChange`.
    unsigned int text_field_change_event_count_ = 0;

    autofill::ScopedAutofillManagersObservation autofill_managers_observation_{
        this};
  };

  ChromeComposeClient(const ChromeComposeClient&) = delete;
  ChromeComposeClient& operator=(const ChromeComposeClient&) = delete;
  ~ChromeComposeClient() override;

  // compose::ComposeClient:
  compose::ComposeManager& GetManager() override;
  void ShowComposeDialog(
      EntryPoint ui_entry_point,
      const autofill::FormFieldData& trigger_field,
      std::optional<autofill::AutofillClient::PopupScreenLocation>
          popup_screen_location,
      ComposeCallback callback) override;
  bool HasSession(const autofill::FieldGlobalId& trigger_field_id) override;
  bool ShouldTriggerPopup(
      const autofill::FormData& form_data,
      const autofill::FormFieldData& trigger_field,
      autofill::AutofillSuggestionTriggerSource trigger_source) override;
  compose::PageUkmTracker* GetPageUkmTracker() override;
  void DisableProactiveNudge() override;
  void OpenProactiveNudgeSettings() override;
  void AddSiteToNeverPromptList(const url::Origin& origin) override;

  // ComposeSession::Observer:
  void OnSessionComplete(autofill::FieldGlobalId field_global_id,
                         compose::ComposeSessionCloseReason close_reason,
                         const compose::ComposeSessionEvents& events) override;

  // autofill::AutofillManager::Observer:
  // Used to observe field focus changes so that the saved state notification
  // is only shown when an autofill suggestion will not be shown on another
  // field.
  void OnAfterFocusOnFormField(autofill::AutofillManager& manager,
                               autofill::FormGlobalId form,
                               autofill::FieldGlobalId field) override;

  // ComposeClientUntrustedPageHandler
  // Shows the compose dialog.
  void ShowUI() override;
  // Closes the compose dialog. `reason` describes the user action that
  // triggered the close.
  void CloseUI(compose::mojom::CloseReason reason) override;
  // Update corresponding prefs and state when FRE is completed.
  void CompleteFirstRun() override;
  // Opens the Compose-related Chrome settings page in a new tab when the
  // "Go to Settings" link is clicked in the MSBB dialog.
  void OpenComposeSettings() override;

  // InnerTextProvider
  void GetInnerText(content::RenderFrameHost& host,
                    std::optional<int> node_id,
                    content_extraction::InnerTextCallback callback) override;

  bool GetMSBBStateFromPrefs();

  void UpdateAllSessionsWithFirstRunComplete();

  virtual bool ShouldTriggerContextMenu(content::RenderFrameHost* rfh,
                                        content::ContextMenuParams& params);

  void BindComposeDialog(
      mojo::PendingReceiver<compose::mojom::ComposeClientUntrustedPageHandler>
          client_handler,
      mojo::PendingReceiver<compose::mojom::ComposeSessionUntrustedPageHandler>
          handler,
      mojo::PendingRemote<compose::mojom::ComposeUntrustedDialog> dialog);

  // content::WebContentsObserver implementation.
  // Called when the primary page location changes. This includes reloads.
  // TODO: Look into using DocumentUserData or keying sessions on render ID
  // to more accurately save and remove state.
  void PrimaryPageChanged(content::Page& page) override;

  // Notification that the `render_widget_host` for this WebContents has gained
  // focus. We will use this to relaunch a MSBB flow if applicable.
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;

  // content::WebContentsObserver implementation.
  // Called when there has been direct user interaction with the WebContents.
  // Used to close the dialog when the user scrolls.
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

  // Called when the focused element changes. This is only used to inform
  // the proactive nudge tracker that focus has changed until the
  // AutofillManager::Observer APIs for focus tracking are fixed.
  void OnFocusChangedInPage(content::FocusedNodeDetails* details) override;

  // compose::ProactiveNudgeTracker::Delegate implementation.
  void ShowProactiveNudge(autofill::FormGlobalId form,
                          autofill::FieldGlobalId field,
                          compose::ComposeEntryPoint entry_point) override;

  // Returns the Compose optimization guide hints for the current URL.
  // compose::ProactiveNudgeTracker::Delegate implementation.
  compose::ComposeHintMetadata GetComposeHintMetadata() override;

  ComposeEnabling& GetComposeEnabling();

  // Returns true when the dialog is showing and false otherwise.
  bool IsDialogShowing();

  // Returns true when the delay timmer to show the popup is running.
  bool IsPopupTimerRunning();

  // Helper methods for setting up testing state.
  int GetSessionCountForTest();
  void SetOptimizationGuideForTest(
      optimization_guide::OptimizationGuideDecider* opt_guide);
  void SetModelExecutorForTest(
      optimization_guide::OptimizationGuideModelExecutor* model_executor);
  void SetSkipShowDialogForTest(bool should_skip);
  void SetSessionIdForTest(base::Token session_id);
  void SetInnerTextProviderForTest(InnerTextProvider* inner_text);

  // If there is an active session calls the OpenFeedbackPage method on it.
  // Used only for testing.
  void OpenFeedbackPageForTest(std::string feedback_id);

 protected:
  explicit ChromeComposeClient(content::WebContents* web_contents);
  optimization_guide::OptimizationGuideModelExecutor* GetModelExecutor();
  optimization_guide::OptimizationGuideDecider* GetOptimizationGuide();
  base::Token GetSessionId();
  InnerTextProvider* GetInnerTextProvider();
  std::unique_ptr<ComposeEnabling> compose_enabling_;

 private:
  friend class content::WebContentsUserData<ChromeComposeClient>;
  FRIEND_TEST_ALL_PREFIXES(ChromeComposeClientTest,
                           TestComposeQualityFeedbackPositive);
  FRIEND_TEST_ALL_PREFIXES(ChromeComposeClientTest,
                           TestComposeQualityFeedbackNegative);
  FRIEND_TEST_ALL_PREFIXES(ChromeComposeClientTest,
                           TextFieldChangeThresholdHidesProactiveNudge);

  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;

  // Prepares to open the dialog with an existing session for the active field.
  // Must be called when there is an existing session for the active field (i.e.
  // |GetSessionForACtiveComposeField| returns a valid session.
  // Also records resumption metrics for the existing session.
  void PrepareToResumeExistingSession(ComposeCallback callback,
                                      bool has_selection,
                                      bool popup_clicked);
  // Creates a session for `trigger_field` and initializes it as necessary.
  // `callback` is a callback to the renderer to insert the compose response
  // into the compose field.
  void CreateNewSession(ComposeCallback callback,
                        const autofill::FormFieldData& trigger_field,
                        std::string_view selected_text,
                        bool popup_clicked);

  // Set the exit reason for a session that does not progress past the FRE.
  void SetFirstRunSessionCloseReason(
      compose::ComposeFreOrMsbbSessionCloseReason close_reason);

  // Set the exit reason for a session that does not progress past the
  // MSBB UI.
  void SetMSBBSessionCloseReason(
      compose::ComposeFreOrMsbbSessionCloseReason close_reason);

  // Set the exit reason for a session.
  void SetSessionCloseReason(compose::ComposeSessionCloseReason close_reason);

  // Launch Hats with the active session
  void LaunchHatsSurveyForActiveSession(
      compose::ComposeSessionCloseReason close_reason);

  // Removes `active_compose_field_id_` from `sessions_` and resets
  // `active_compose_field_id_` and `active_compose_form_id_`
  void RemoveActiveSession();

  // Removes all sessions and resets `active_compose_field_id_` and
  // `active_compose_form_id_`.
  void RemoveAllSessions();

  // Shows the saved state notification for `field_id` as long as any newly
  // focused field will not show autofill suggestions.
  void ShowSavedStateNotification(autofill::FieldGlobalId field_id);

  // Returns nullptr if no such session exists.
  ComposeSession* GetSessionForActiveComposeField();

  // Returns true if the active field has an existing session that is not
  // expired.
  bool ActiveFieldHasUnexpiredSession();

  // Checks if the page assessed language is supported by Compose.
  bool IsPageLanguageSupported();

  compose::ComposeManagerImpl manager_{this};

  std::unique_ptr<compose::ComposeDialogController> compose_dialog_controller_;
  // A handle to optimization guide for information about URLs that have
  // recently been navigated to.
  raw_ptr<optimization_guide::OptimizationGuideDecider> opt_guide_;

  std::optional<optimization_guide::OptimizationGuideModelExecutor*>
      model_executor_for_test_;

  std::optional<base::Token> session_id_for_test_;

  // The unique renderer and form IDs of the last field the user selected
  // compose on.
  std::optional<FieldIdentifier> active_compose_ids_;

  // The last trigger source used when calling |ShouldTriggerPopup|. This is
  // reset after the dialog is shown.
  autofill::AutofillSuggestionTriggerSource last_popup_trigger_source_ =
      autofill::AutofillSuggestionTriggerSource::kUnspecified;

  std::optional<InnerTextProvider*> inner_text_provider_for_test_;

  // Saved states for each compose field.
  base::flat_map<autofill::FieldGlobalId, std::unique_ptr<ComposeSession>>
      sessions_;

  // A mojom receiver that is bound to `this` in `BindComposeDialog()`. A pipe
  // may disconnect but this receiver will still be bound, until reset in the
  // next bind call. With mojo, there is no need to immediately reset the
  // binding when the pipe disconnects. Any callbacks in receiver methods can be
  // safely called even when the pipe is disconnected.
  mojo::Receiver<compose::mojom::ComposeClientUntrustedPageHandler>
      client_page_receiver_{this};

  // Time that the last call to show the dialog was started.
  base::TimeTicks show_dialog_start_;

  // Used to test Compose in a tab at |chrome-untrusted://compose|.
  std::unique_ptr<ComposeSession> debug_session_;

  // Collects per-pageload UKM metrics and reports them on destruction (if any
  // were collected).
  std::unique_ptr<compose::PageUkmTracker> page_ukm_tracker_;

  bool skip_show_dialog_for_test_ = false;

  // This boolean gets set to true upon opening the Settings page via the
  // OpenComposeSettings function, and gets set back to false when the current
  // page is refocused using OnWebContentsFocused.
  bool open_settings_requested_ = false;

  // A state machine that decides whether the proactive nudge should be shown at
  // a given moment.
  compose::ProactiveNudgeTracker nudge_tracker_;

  FieldChangeObserver field_change_observer_;

  // Observer for autofill field focus changes. This is used to prevent showing
  // the saved state notification on a previous focused field when an autofill
  // suggestion will be shown in a newly focused field.
  autofill::ScopedAutofillManagersObservation autofill_managers_observation_{
      this};

  BooleanPrefMember proactive_nudge_enabled_;

  // Time since page load, or time since page has changed if it's not loaded
  // yet.
  base::TimeTicks page_change_time_;

  compose::ComposeEntryPoint most_recent_nudge_entry_point_ =
      compose::ComposeEntryPoint::kProactiveNudge;

  base::WeakPtrFactory<ChromeComposeClient> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_
