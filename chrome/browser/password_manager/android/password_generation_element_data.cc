// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_generation_element_data.h"

PasswordGenerationElementData::PasswordGenerationElementData(
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  form_data = ui_data.form_data;
  form_signature = autofill::CalculateFormSignature(ui_data.form_data);
  field_signature = autofill::CalculateFieldSignatureByNameAndType(
      ui_data.generation_element, autofill::FormControlType::kInputPassword);
  generation_element_id = ui_data.generation_element_id;
  max_password_length = ui_data.max_length;
}

PasswordGenerationElementData::PasswordGenerationElementData() = default;
PasswordGenerationElementData::PasswordGenerationElementData(
    const PasswordGenerationElementData&) = default;
PasswordGenerationElementData& PasswordGenerationElementData::operator=(
    const PasswordGenerationElementData&) = default;
PasswordGenerationElementData::PasswordGenerationElementData(
    PasswordGenerationElementData&&) = default;
PasswordGenerationElementData& PasswordGenerationElementData::operator=(
    PasswordGenerationElementData&&) = default;
