// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/password_manager/password_accessory_view_interface.h"
#include "components/autofill/core/common/filling_status.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace favicon {
class FaviconService;
}

namespace password_manager {
class PasswordManagerDriver;
}  // namespace password_manager

class PasswordGenerationDialogViewInterface;

// The controller for the view located below the keyboard accessory.
// Upon creation, it creates (and owns) a corresponding PasswordAccessoryView.
// This view will be provided with data and will notify this controller about
// interactions (like requesting to fill a password suggestions).
//
// Create it for a WebContents instance by calling:
//     PasswordAccessoryController::CreateForWebContents(web_contents);
// After that, it's attached to the |web_contents| instance and can be retrieved
// by calling:
//     PasswordAccessoryController::FromWebContents(web_contents);
// Any further calls to |CreateForWebContents| will be a noop.
//
// TODO(fhorschig): This class currently only supports credentials originating
// from the main frame. Supporting iframes is intended: https://crbug.com/854150
class PasswordAccessoryController
    : public content::WebContentsUserData<PasswordAccessoryController> {
 public:
  using CreateDialogFactory = base::RepeatingCallback<std::unique_ptr<
      PasswordGenerationDialogViewInterface>(PasswordAccessoryController*)>;
  ~PasswordAccessoryController() override;

  // -----------------------------
  // Methods called by the client:
  // -----------------------------

  // Returns true, if the accessory controller may exist for |web_contents|.
  // Otherwise (e.g. if VR is enabled), it returns false.
  static bool AllowedForWebContents(content::WebContents* web_contents);

  // Saves credentials for an origin so that they can be used in the sheet.
  void SavePasswordsForOrigin(
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const url::Origin& origin);

  // Notifies the view that automatic password generation status changed.
  void OnAutomaticGenerationStatusChanged(
      bool available,
      const base::Optional<
          autofill::password_generation::PasswordGenerationUIData>& ui_data,
      const base::WeakPtr<password_manager::PasswordManagerDriver>& driver);

  // Completes a filling attempt by recording metrics, giving feedback to the
  // user and dismissing the accessory sheet.
  void OnFilledIntoFocusedField(autofill::FillingStatus status);

  // Makes sure, that all shown suggestions are appropriate for the currently
  // focused field and for fields that lost the focus. If a field lost focus,
  // |is_fillable| will be false.
  void RefreshSuggestionsForField(const url::Origin& origin,
                                  bool is_fillable,
                                  bool is_password_field);

  // Reacts to a navigation on the main frame, e.g. by clearing caches.
  void DidNavigateMainFrame();

  // Requests to show the accessory bar. The accessory will only be shown
  // when the keyboard becomes visible.
  void ShowWhenKeyboardIsVisible();

  // Requests to hide the accessory. This hides both the accessory sheet
  // (if open) and the accessory bar.
  void Hide();

  // --------------------------
  // Methods called by UI code:
  // --------------------------

  // Uses the give |favicon_service| to get an icon for the currently focused
  // frame. The given callback is called with an image unless an icon for a new
  // origin was called. In the latter case, the callback is dropped.
  // The callback is called with an |IsEmpty()| image if there is no favicon.
  void GetFavicon(int desired_size_in_pixel,
                  base::OnceCallback<void(const gfx::Image&)> icon_callback);

  // Called by the UI code to request that |textToFill| is to be filled into the
  // currently focused field.
  void OnFillingTriggered(bool is_password, const base::string16& textToFill);

  // Called by the UI code because a user triggered the |selectedOption|.
  void OnOptionSelected(const base::string16& selectedOption) const;

  // Called by the UI code to signal that the user requested password
  // generation. This should prompt a modal dialog with the generated password.
  void OnGenerationRequested();

  // Called from the modal dialog if the user accepted the generated password.
  void GeneratedPasswordAccepted(const base::string16& password);

  // Called from the modal dialog if the user rejected the generated password.
  void GeneratedPasswordRejected();

  // -----------------
  // Member accessors:
  // -----------------

  // The web page view containing the focused field.
  gfx::NativeView container_view() const;

  gfx::NativeWindow native_window() const;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows inject a fake/mock view.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      std::unique_ptr<PasswordAccessoryViewInterface> test_view,
      CreateDialogFactory create_dialog_callback,
      favicon::FaviconService* favicon_service);

#if defined(UNIT_TEST)
  // Returns the held view for testing.
  PasswordAccessoryViewInterface* view() const { return view_.get(); }
#endif  // defined(UNIT_TEST)

 private:
  // Data including the form and field for which generation was requested,
  // their signatures and the maximum password size.
  struct GenerationElementData;

  // Data for a credential pair that is transformed into a suggestion.
  struct SuggestionElementData;

  // Data allowing to cache favicons and favicon-related requests.
  struct FaviconRequestData;

  // Required for construction via |CreateForWebContents|:
  explicit PasswordAccessoryController(content::WebContents* contents);
  friend class content::WebContentsUserData<PasswordAccessoryController>;

  // Constructor that allows to inject a mock or fake view.
  PasswordAccessoryController(
      content::WebContents* web_contents,
      std::unique_ptr<PasswordAccessoryViewInterface> view,
      CreateDialogFactory create_dialog_callback,
      favicon::FaviconService* favicon_service);

  // Creates the view items based on the given |suggestions|.
  // If |is_password_field| is false, password suggestions won't be interactive.
  static std::vector<PasswordAccessoryViewInterface::AccessoryItem>
  CreateViewItems(const url::Origin& origin,
                  const std::vector<SuggestionElementData>& suggestions,
                  bool is_password_field);

  // Handles a favicon response requested by |GetFavicon| and calls the waiting
  // last_icon_callback_ with a (possibly empty) icon bitmap.
  void OnImageFetched(
      url::Origin origin,
      const favicon_base::FaviconRawBitmapResult& bitmap_results);

  // Contains the last set of credentials by origin.
  std::map<url::Origin, std::vector<SuggestionElementData>> origin_suggestions_;

  // The tab for which this class is scoped.
  content::WebContents* web_contents_;

  // Data for the generation element used to generate the password.
  std::unique_ptr<GenerationElementData> generation_element_data_;

  // The origin of the currently focused frame. It's used to ensure that
  // favicons are not displayed across origins.
  url::Origin current_origin_;

  // TODO(fhorschig): Find a way to use unordered_map with origin keys.
  // A cache for all favicons that were requested. This includes all iframes
  // for which the accessory was displayed.
  std::map<url::Origin, FaviconRequestData> icons_request_data_;

  // Used to track a requested favicon. If the set of suggestion changes, this
  // object aborts the request. Upon destruction, requests are cancelled, too.
  base::CancelableTaskTracker favicon_tracker_;

  // Password manager driver for the target frame used for password generation.
  base::WeakPtr<password_manager::PasswordManagerDriver> target_frame_driver_;

  // Modal dialog view meant to display the generated password.
  std::unique_ptr<PasswordGenerationDialogViewInterface> dialog_view_;

  // Remembers whether the last focused field was a password field. That way,
  // the reconstructed elements have the correct type.
  bool last_focused_field_was_for_passwords_ = false;

  // Hold the native instance of the view. Must be last declared and initialized
  // member so the view can be created in the constructor with a fully set up
  // controller instance.
  std::unique_ptr<PasswordAccessoryViewInterface> view_;

  // Creation callback for the modal dialog view meant to facilitate testing.
  CreateDialogFactory create_dialog_factory_;

  // The favicon service used to make retrieve icons for a given origin.
  favicon::FaviconService* favicon_service_;

  base::WeakPtrFactory<PasswordAccessoryController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAccessoryController);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_
