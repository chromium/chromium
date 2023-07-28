// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_

#include "chrome/browser/ash/input_method/editor_event_sink.h"
#include "chrome/browser/ash/input_method/editor_instance_impl.h"
#include "chrome/browser/ash/input_method/editor_text_actuator.h"
#include "chrome/browser/ash/input_method/mojom/editor.mojom.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"

namespace ash {
namespace input_method {

// Acts as a central "connector" for all things related to the orca project.
// This includes all current (and future) trigger points, providing the required
// plumbing to broker mojo connections from WebUIs and other clients, and
// providing an overall unified interface for the backend of the project.
class EditorMediator : public EditorInstanceImpl::Delegate,
                       public EditorEventSink {
 public:
  EditorMediator();
  ~EditorMediator() override;

  // Fetch the current instance of this class. Note that this class MUST be
  // constructed prior to calling this method.
  static EditorMediator* Get();

  // Binds a new editor instance request from a client.
  void BindEditorInstance(
      mojo::PendingReceiver<mojom::EditorInstance> pending_receiver);

  // Handles a trigger event received from the system. This event could come
  // from a number of system locations.
  void HandleTrigger();

  // EditorEventSink
  void OnFocus(int context_id) override;
  void OnBlur() override;

  // EditorInstanceImpl::Delegate overrides
  void CommitEditorResult(std::string_view text) override;

 private:
  EditorInstanceImpl editor_instance_impl_;
  EditorTextActuator text_actuator_;

  // May contain an instance of MakoPageHandler. This is used to control the
  // lifetime of the Mako WebUI.
  std::unique_ptr<ash::MakoPageHandler> mako_page_handler_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_
