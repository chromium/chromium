// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_SHILL_ERROR_H_
#define CHROME_BROWSER_CHROMEOS_NET_SHILL_ERROR_H_

#include <string>


namespace chromeos {
namespace shill_error {

std::u16string GetShillErrorString(const std::string& error,
                                   const std::string& network_id);

// Returns true if |error| is known to be a configuration error.
bool IsConfigurationError(const std::string& error);

}  // namespace shill_error
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_SHILL_ERROR_H_
