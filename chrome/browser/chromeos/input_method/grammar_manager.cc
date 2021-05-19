// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/grammar_manager.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/ime_input_context_handler_interface.h"

namespace chromeos {
namespace {

constexpr base::TimeDelta kCheckDelay = base::TimeDelta::FromMilliseconds(500);

}  // namespace

GrammarManager::GrammarManager() {}

GrammarManager::~GrammarManager() = default;

bool GrammarManager::IsOnDeviceGrammarEnabled() {
  return base::FeatureList::IsEnabled(
      chromeos::features::kOnDeviceGrammarCheck);
}

void GrammarManager::OnFocus(int context_id) {
  if (context_id != context_id_) {
    last_text_ = u"";
  }
  context_id_ = context_id;
}

bool GrammarManager::OnKeyEvent(const ui::KeyEvent& event) {
  return false;
}

void GrammarManager::OnSurroundingTextChanged(const std::u16string& text,
                                              int cursor_pos,
                                              int anchor_pos) {
  if (text == last_text_)
    return;

  // Grammar check is cpu consuming, so we only send request to ml service when
  // the user has stopped typing for some time.
  delay_timer_.Start(
      FROM_HERE, kCheckDelay,
      base::BindOnce(&GrammarManager::Check, base::Unretained(this), text));

  last_text_ = text;
}

void GrammarManager::Check(const std::u16string& text) {
  if (text != last_text_) {
    return;
  }

  // TODO(crbug/1132699): implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace chromeos
