// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_ECHO_PRIVATE_API_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_ECHO_PRIVATE_API_UTIL_H_

#include "base/values.h"

class PrefRegistrySimple;

namespace chromeos::echo_offer {

// Registers the EchoCheckedOffers field in Local State.
void RegisterPrefs(PrefRegistrySimple* registry);

// Removes empty dictionaries from |dict|, potentially nested.
// Does not modify empty lists.
void RemoveEmptyValueDicts(base::Value::Dict& dict);

}  // namespace chromeos::echo_offer

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_ECHO_PRIVATE_API_UTIL_H_
