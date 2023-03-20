// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/backend/plural_string_handler.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

PluralStringHandler::PluralStringHandler() = default;
PluralStringHandler::~PluralStringHandler() = default;

void PluralStringHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getPluralString",
      base::BindRepeating(&PluralStringHandler::HandleGetPluralString,
                          base::Unretained(this)));
}

void PluralStringHandler::AddStringToPluralMap(const std::string& name,
                                               int string_id) {
  DCHECK(!base::Contains(string_id_map_, name));
  string_id_map_[name] = string_id;
}

void PluralStringHandler::HandleGetPluralString(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(3U, args.size());
  const std::string callback = args[0].GetString();
  const std::string name = args[1].GetString();
  const int count = args[2].GetInt();
  if (!base::Contains(string_id_map_, name)) {
    // Only reachable if the WebUI renderer is misbehaving.
    LOG(ERROR) << "Invalid string ID received: " << name;
    return;
  }
  const std::u16string localized_string =
      l10n_util::GetPluralStringFUTF16(string_id_map_.at(name), count);
  ResolveJavascriptCallback(base::Value(callback),
                            base::Value(localized_string));
}

void PluralStringHandler::SetWebUIForTest(content::WebUI* web_ui) {
  DCHECK(web_ui);
  set_web_ui(web_ui);
}

}  // namespace ash
