// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
class AddressAccessoryController;
class CreditCardAccessoryController;
}  // namespace autofill

namespace favicon {
class FaviconService;
}  // namespace favicon

class AccessoryController;
class PasswordAccessoryController;

// Use ManualFillingController::GetOrCreate to obtain instances of this class.
class ManualFillingControllerImpl
    : public ManualFillingController,
      public content::WebContentsUserData<ManualFillingControllerImpl> {
 public:
  ~ManualFillingControllerImpl() override;

  // ManualFillingController:
  void RefreshSuggestions(
      const autofill::AccessorySheetData& accessory_sheet_data) override;
  void NotifyFocusedInputChanged(
      autofill::mojom::FocusedFieldType focused_field_type) override;
  void UpdateSourceAvailability(FillingSource source,
                                bool has_suggestions) override;
  void Hide() override;
  void OnAutomaticGenerationStatusChanged(bool available) override;
  void OnFillingTriggered(autofill::AccessoryTabType type,
                          const autofill::UserInfo::Field& selection) override;
  void OnOptionSelected(
      autofill::AccessoryAction selected_action) const override;
  void GetFavicon(int desired_size_in_pixel,
                  const std::string& credential_origin,
                  IconCallback icon_callback) override;
  gfx::NativeView container_view() const override;

  // Returns a weak pointer for this object.
  base::WeakPtr<ManualFillingController> AsWeakPtr();

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows injecting a fake/mock
  // view, a mock favicon service and type-specific controllers.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      favicon::FaviconService* favicon_service,
      base::WeakPtr<PasswordAccessoryController> pwd_controller,
      base::WeakPtr<autofill::AddressAccessoryController> address_controller,
      base::WeakPtr<autofill::CreditCardAccessoryController> cc_controller,
      std::unique_ptr<ManualFillingViewInterface> test_view);

#if defined(UNIT_TEST)
  // Returns the held view for testing.
  ManualFillingViewInterface* view() const { return view_.get(); }
#endif  // defined(UNIT_TEST)

 protected:
  friend class ManualFillingController;  // Allow protected access in factories.

  // Enables calling initialization code that relies on a fully constructed
  // ManualFillingController that is attached to a WebContents instance.
  // This is matters for subcomponents which lazily trigger the creation of this
  // class. If called in constructors, it would cause an infinite creation loop.
  void Initialize();

 private:
  friend class content::WebContentsUserData<ManualFillingControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit ManualFillingControllerImpl(content::WebContents* contents);

  // Constructor that allows to inject a mock favicon service and a mock view.
  ManualFillingControllerImpl(
      content::WebContents* web_contents,
      favicon::FaviconService* favicon_service,
      base::WeakPtr<PasswordAccessoryController> pwd_controller,
      base::WeakPtr<autofill::AddressAccessoryController> address_controller,
      base::WeakPtr<autofill::CreditCardAccessoryController> cc_controller,
      std::unique_ptr<ManualFillingViewInterface> view);

  // Returns true if the keyboard accessory needs to be shown.
  bool ShouldShowAccessory() const;

  // Adjusts visibility based on focused field type and available suggestions.
  void UpdateVisibility();

  // Handles a favicon response requested by |GetFavicon| and responds to the
  // given callback with a (possibly empty) icon bitmap.
  void OnImageFetched(
      IconCallback icon_callback,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Returns the controller that is responsible for a tab of given |type|.
  AccessoryController* GetControllerForTab(autofill::AccessoryTabType type);

  // Returns the controller that is responsible for a given |action|.
  AccessoryController* GetControllerForAction(
      autofill::AccessoryAction action) const;

  // Returns the controller that is responsible for a given |action|.
  PasswordAccessoryController* GetPasswordController() const;

  // The tab for which this class is scoped.
  content::WebContents* web_contents_ = nullptr;

  // The favicon service used to retrieve icons for a given origin.
  favicon::FaviconService* favicon_service_ = nullptr;

  // This set contains sources to be shown to the user.
  base::flat_set<FillingSource> available_sources_;

  // Type of the last known selected field. Helps to determine UI visibility.
  autofill::mojom::FocusedFieldType focused_field_type_ =
      autofill::mojom::FocusedFieldType::kUnknown;

  // Used to track requested favicons. On destruction, requests are cancelled.
  base::CancelableTaskTracker favicon_tracker_;

  // Controllers which handle events relating to a specific tab and the
  // associated data.
  base::WeakPtr<PasswordAccessoryController> pwd_controller_for_testing_;
  base::WeakPtr<autofill::AddressAccessoryController> address_controller_;
  base::WeakPtr<autofill::CreditCardAccessoryController> cc_controller_;

  // Hold the native instance of the view. Must be last declared and initialized
  // member so the view can be created in the constructor with a fully set up
  // controller instance.
  std::unique_ptr<ManualFillingViewInterface> view_ =
      ManualFillingViewInterface::Create(this);

  base::WeakPtrFactory<ManualFillingControllerImpl> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ManualFillingControllerImpl);
};

#endif  // CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_IMPL_H_
