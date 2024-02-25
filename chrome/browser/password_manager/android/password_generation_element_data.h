// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_GENERATION_ELEMENT_DATA_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_GENERATION_ELEMENT_DATA_H_

#include "components/autofill/core/common/password_generation_util.h"

// Data including the form and field for which generation was requested,
// their signatures and the maximum password size.
struct PasswordGenerationElementData {
  explicit PasswordGenerationElementData(
      const autofill::password_generation::PasswordGenerationUIData& ui_data);
  PasswordGenerationElementData();

  PasswordGenerationElementData(const PasswordGenerationElementData&);
  PasswordGenerationElementData& operator=(
      const PasswordGenerationElementData&);
  PasswordGenerationElementData(PasswordGenerationElementData&&);
  PasswordGenerationElementData& operator=(PasswordGenerationElementData&&);

  // Form for which password generation is triggered.
  autofill::FormData form_data;

  // Signature of the form for which password generation is triggered.
  autofill::FormSignature form_signature;

  // Signature of the field for which password generation is triggered.
  autofill::FieldSignature field_signature;

  // Renderer ID of the password field triggering generation.
  autofill::FieldRendererId generation_element_id;

  // Maximum length of the generated password.
  uint32_t max_password_length = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_GENERATION_ELEMENT_DATA_H_
