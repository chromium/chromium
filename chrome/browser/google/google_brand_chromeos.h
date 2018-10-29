// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_CHROMEOS_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_CHROMEOS_H_

#include <string>

#include "base/callback_forward.h"

namespace google_brand {
namespace chromeos {

// Returns the brand code stored in Local State that has been assigned to a
// partner. Returns empty string if the information is not available.
std::string GetBrand();

// Returns a variation of the brand code based on enrollment type.
// TODO(crbug.com/888725): Rename this to GetBrand and replace the current one.
std::string GetRlzBrand();

// Clears brand code for the current session (not persisted through browser
// restart). Future calls to GetBrand() will return an empty string.
void ClearBrandForCurrentSession();

// Reads the brand code from a board-specific data file and stores it to
// Local State. |callback| is invoked on the calling thread upon success, and
// is not invoked if the brand code is not found or is empty.
void InitBrand(const base::Closure& callback);

}  // namespace chromeos
}  // namespace google_brand

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_CHROMEOS_H_
