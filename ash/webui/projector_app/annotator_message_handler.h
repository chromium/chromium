// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_ANNOTATOR_MESSAGE_HANDLER_H_
#define ASH_WEBUI_PROJECTOR_APP_ANNOTATOR_MESSAGE_HANDLER_H_

#include "ash/public/cpp/projector/projector_annotator_controller.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

struct AnnotatorTool;

// Handles communication with the Annotator WebUI (i.e.
// chrome://projector/annotator/annotator_embedder.html)
class AnnotatorMessageHandler : public content::WebUIMessageHandler {
 public:
  AnnotatorMessageHandler();
  AnnotatorMessageHandler(const AnnotatorMessageHandler&) = delete;
  AnnotatorMessageHandler& operator=(const AnnotatorMessageHandler&) = delete;
  ~AnnotatorMessageHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void SetTool(const AnnotatorTool& tool);
  void Undo();
  void Redo();
  void Clear();
  void set_web_ui_for_test(content::WebUI* web_ui) { set_web_ui(web_ui); }
  content::WebUI* get_web_ui_for_test() { return web_ui(); }

 private:
  void OnToolSet(const base::Value::List& args);
  void OnUndoRedoAvailabilityChanged(const base::Value::List& args);
  void OnCanvasInitialized(const base::Value::List& args);
  void OnError(const base::Value::List& args);
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_ANNOTATOR_MESSAGE_HANDLER_H_
