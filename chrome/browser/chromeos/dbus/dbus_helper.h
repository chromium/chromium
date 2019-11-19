// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_DBUS_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_DBUS_HELPER_H_

namespace chromeos {

// Initializes the DBus thread manager and chrome DBus services.
void InitializeDBus();

// D-Bus clients may depend on feature list. This initializes only those clients
// and must be called after feature list initialization.
void InitializeFeatureListDependentDBus();

// Shuts down the DBus thread manager and chrome DBus services.
void ShutdownDBus();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_DBUS_HELPER_H_
