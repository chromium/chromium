// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_ACTUATOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_ACTUATOR_H_

#include <string>

#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_text_inserter.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "url/gurl.h"

namespace ash::input_method {

class EditorTextActuator : public orca::mojom::TextActuator {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnTextInserted() = 0;
    virtual void ProcessConsentAction(ConsentAction consent_action) = 0;
    virtual void ShowUI() = 0;
    virtual void CloseUI() = 0;
    virtual EditorMode GetEditorMode() const = 0;
    virtual size_t GetSelectedTextLength() = 0;
  };

  EditorTextActuator(
      Profile* profile,
      mojo::PendingAssociatedReceiver<orca::mojom::TextActuator> receiver,
      Delegate* delegate);
  ~EditorTextActuator() override;

  // orca::mojom::TextActuator overrides
  void InsertText(const std::string& text) override;
  void ApproveConsent() override;
  void DeclineConsent() override;
  void OpenUrlInNewWindow(const GURL& url) override;
  void ShowUI() override;
  void CloseUI() override;
  void SubmitFeedback(const std::string& description) override;

  void OnFocus(int context_id);
  void OnBlur();

 private:
  raw_ptr<Profile> profile_;
  mojo::AssociatedReceiver<orca::mojom::TextActuator> text_actuator_receiver_;

  // Not owned by this class.
  raw_ptr<Delegate> delegate_;
  EditorTextInserter inserter_;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_ACTUATOR_H_
