// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SYSTEM_ACTUATOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SYSTEM_ACTUATOR_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chrome/browser/ash/input_method/editor_text_insertion.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "url/gurl.h"

namespace ash::input_method {

class EditorSystemActuator : public orca::mojom::SystemActuator {
 public:
  class System {
   public:
    virtual ~System() = default;
    virtual void Announce(const std::u16string& message) = 0;
    virtual void ProcessConsentAction(ConsentAction consent_action) = 0;
    virtual void ShowUI() = 0;
    virtual void CloseUI() = 0;
    virtual void HandleTrigger(
        std::optional<std::string_view> preset_query_id,
        std::optional<std::string_view> freeform_text) = 0;
    virtual EditorMetricsRecorder* GetMetricsRecorder() = 0;
    virtual size_t GetSelectedTextLength() = 0;
  };

  EditorSystemActuator(
      Profile* profile,
      mojo::PendingAssociatedReceiver<orca::mojom::SystemActuator> receiver,
      System* system);
  ~EditorSystemActuator() override;

  // orca::mojom::SystemActuator overrides
  void InsertText(const std::string& text) override;
  void ApproveConsent() override;
  void DeclineConsent() override;
  void OpenUrlInNewWindow(const GURL& url) override;
  void ShowUI() override;
  void CloseUI() override;
  void SubmitFeedback(const std::string& description) override;
  void OnTrigger(orca::mojom::TriggerContextPtr trigger_context) override;
  void EmitMetricEvent(orca::mojom::MetricEvent metric_event) override;
  void OnInputContextUpdated(const GURL& url);

  // Relevant input events
  void OnFocus(int context_id);

 private:
  void QueueTextInsertion(const std::string pending_text);

  raw_ptr<Profile> profile_;
  mojo::AssociatedReceiver<orca::mojom::SystemActuator>
      system_actuator_receiver_;

  // Not owned by this class.
  raw_ptr<System> system_;

  // Possibly holds a queued text insertion operation. If a text insertion op
  // has been queued, then it will be inserted in the next focused text field.
  // Only one text insertion can be queued at a time, with new text insertions
  // overwriting previously queued insertions.
  std::unique_ptr<EditorTextInsertion> queued_text_insertion_;

  GURL current_url_;

  base::OneShotTimer announcement_delay_;
  base::WeakPtrFactory<EditorSystemActuator> weak_ptr_factory_{this};
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SYSTEM_ACTUATOR_H_
