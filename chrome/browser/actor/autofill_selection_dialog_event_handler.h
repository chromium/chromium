// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_AUTOFILL_SELECTION_DIALOG_EVENT_HANDLER_H_
#define CHROME_BROWSER_ACTOR_AUTOFILL_SELECTION_DIALOG_EVENT_HANDLER_H_

#include "chrome/common/actor_webui.mojom-forward.h"

namespace actor {

// Interface for handling asynchronous events from the autofill selection
// dialog in the Glic UI.
class AutofillSelectionDialogEventHandler {
 public:
  virtual ~AutofillSelectionDialogEventHandler() = default;

  // Called when a form is presented in the Glic UI.
  virtual void OnFormPresented(
      webui::mojom::AutofillSuggestionDialogOnFormPresentedParamsPtr
          params) = 0;

  // Called when a suggestion preview is changed for a form.
  virtual void OnFormPreviewChanged(
      webui::mojom::AutofillSuggestionDialogOnFormPreviewChangedParamsPtr
          params) = 0;

  // Called when a form is confirmed with a selected suggestion.
  virtual void OnFormConfirmed(
      webui::mojom::AutofillSuggestionDialogOnFormConfirmedParamsPtr
          params) = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_AUTOFILL_SELECTION_DIALOG_EVENT_HANDLER_H_
