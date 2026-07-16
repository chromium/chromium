// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_TOAST_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_TOAST_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/messages/android/message_enums.h"

namespace content {
class WebContents;
}  // namespace content

namespace messages {
class MessageWrapper;
}  // namespace messages

namespace contextual_tasks {

// Helper class to display a message on Android when the side panel is hidden
// due to resizing.
class ContextualTasksToast {
 public:
  // Shows a message on a given web_contents.
  // Returns a unique_ptr to the message. Destroying the object will dismiss the
  // message.
  static std::unique_ptr<ContextualTasksToast> Show(
      content::WebContents* web_contents,
      int title_res_id,
      int description_res_id);

  ContextualTasksToast(const ContextualTasksToast&) = delete;
  ContextualTasksToast& operator=(const ContextualTasksToast&) = delete;
  ~ContextualTasksToast();

 private:
  ContextualTasksToast(content::WebContents* web_contents,
                       int title_res_id,
                       int description_res_id);

  void HandleMessageAccepted();
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);

  base::WeakPtr<content::WebContents> web_contents_;
  std::unique_ptr<messages::MessageWrapper> message_;

  base::WeakPtrFactory<ContextualTasksToast> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_TOAST_H_
