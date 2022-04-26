// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/annotator_message_handler.h"

#include <memory>

#include "ash/public/cpp/projector/annotator_tool.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "base/check.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

namespace ash {

AnnotatorMessageHandler::AnnotatorMessageHandler() = default;
AnnotatorMessageHandler::~AnnotatorMessageHandler() = default;

void AnnotatorMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "onToolSet", base::BindRepeating(&AnnotatorMessageHandler::OnToolSet,
                                       base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "onUndoRedoAvailabilityChanged",
      base::BindRepeating(
          &AnnotatorMessageHandler::OnUndoRedoAvailabilityChanged,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "onCanvasInitialized",
      base::BindRepeating(&AnnotatorMessageHandler::OnCanvasInitialized,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "onError", base::BindRepeating(&AnnotatorMessageHandler::OnError,
                                     base::Unretained(this)));
}

void AnnotatorMessageHandler::SetTool(const AnnotatorTool& tool) {
  AllowJavascript();
  FireWebUIListener("setTool", tool.ToValue());
}

void AnnotatorMessageHandler::Undo() {
  AllowJavascript();
  FireWebUIListener("undo");
}

void AnnotatorMessageHandler::Redo() {
  AllowJavascript();
  FireWebUIListener("redo");
}

void AnnotatorMessageHandler::Clear() {
  AllowJavascript();
  FireWebUIListener("clear");
}

void AnnotatorMessageHandler::OnToolSet(const base::Value::List& args) {
  DCHECK_EQ(args.size(), 1u);
  ProjectorController::Get()->OnToolSet(AnnotatorTool::FromValue(args[0]));
}

void AnnotatorMessageHandler::OnUndoRedoAvailabilityChanged(
    const base::Value::List& args) {
  DCHECK_EQ(args.size(), 2u);
  DCHECK(args[0].is_bool());
  DCHECK(args[1].is_bool());
  ProjectorController::Get()->OnUndoRedoAvailabilityChanged(args[0].GetBool(),
                                                            args[1].GetBool());
}

void AnnotatorMessageHandler::OnCanvasInitialized(
    const base::Value::List& args) {
  DCHECK_EQ(args.size(), 1u);
  DCHECK(args[0].is_bool());
  ProjectorController::Get()->OnCanvasInitialized(args[0].GetBool());
}

void AnnotatorMessageHandler::OnError(const base::Value::List& args) {
  // TODO(b/200846160): The annotator is in an error state. Show creation flow
  // error notification and trigger a reload of the WebContent hosting the
  // annotator to clear the error state.
}

}  // namespace ash
