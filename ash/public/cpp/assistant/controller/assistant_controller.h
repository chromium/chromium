// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"

class GURL;

namespace ash {

class AssistantControllerObserver;

// The interface for the Assistant controller.
class ASH_PUBLIC_EXPORT AssistantController {
 public:
  // Returns the singleton instance owned by Shell.
  static AssistantController* Get();

  // Adds/removes the specified |observer|.
  virtual void AddObserver(AssistantControllerObserver* observer) = 0;
  virtual void RemoveObserver(AssistantControllerObserver* observer) = 0;

  // Opens Google Assistant settings.
  virtual void OpenAssistantSettings() = 0;

  // Opens the specified |url| in a new browser tab. Special handling is applied
  // to deep links which may cause deviation from this behavior.
  virtual void OpenUrl(const GURL& url,
                       bool in_background = false,
                       bool from_server = false) = 0;

  // Returns a weak pointer to this instance.
  virtual base::WeakPtr<AssistantController> GetWeakPtr() = 0;

  // Provides a reference to the underlying |assistant| service.
  virtual void SetAssistant(assistant::Assistant* assistant) = 0;

  // Methods below may only be called after |SetAssistant| is called.
  // Show speaker id enrollment flow.
  virtual void StartSpeakerIdEnrollmentFlow() = 0;

  // Send Assistant feedback to Assistant server. If |pii_allowed| is
  // true then the user gives permission to attach Assistant debug info.
  // |feedback_description| is user's feedback input.
  virtual void SendAssistantFeedback(bool pii_allowed,
                                     const std::string& feedback_description,
                                     const std::string& screenshot_png) = 0;

 protected:
  AssistantController();
  virtual ~AssistantController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_H_
