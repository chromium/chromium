// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_BACKEND_PLURAL_STRING_HANDLER_H_
#define ASH_WEBUI_COMMON_BACKEND_PLURAL_STRING_HANDLER_H_

#include <map>
#include <string>

#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash {

class PluralStringHandler : public content::WebUIMessageHandler {
 public:
  PluralStringHandler();

  PluralStringHandler(const PluralStringHandler&) = delete;
  PluralStringHandler& operator=(const PluralStringHandler&) = delete;

  ~PluralStringHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // Adds to map of string IDs for pluralization.
  void AddStringToPluralMap(const std::string& name, int id);

  void SetWebUIForTest(content::WebUI* web_ui);

 private:
  // Returns a localized, pluralized string.
  void HandleGetPluralString(const base::Value::List& args);

  std::map<std::string, int> string_id_map_;
};

}  // namespace ash

#endif  // ASH_WEBUI_COMMON_BACKEND_PLURAL_STRING_HANDLER_H_
