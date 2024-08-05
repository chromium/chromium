// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_AVAILABILITY_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_AVAILABILITY_H_

namespace ash {

// Check whether Mahi is allowed. This function checks two restrictions:
//   * age: if not demo mode, the account must not hit minor restrictions
//   * country: the country code must be in the allow list.
bool CanUseMahiService();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_AVAILABILITY_H_
