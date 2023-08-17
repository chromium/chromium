// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_ASH_DBUS_HELPER_H_
#define CHROME_BROWSER_ASH_DBUS_ASH_DBUS_HELPER_H_

namespace ash {

// An observer for DBus initialization and shutdown for test.
// TODO(b/296389100): Replace with a `AshDBusConfig`.
class DBusHelperObserverForTest {
 public:
  virtual ~DBusHelperObserverForTest();

  static void Set(DBusHelperObserverForTest* observer);

  // Invoked after `InitializeDBus()`.
  virtual void PostInitializeDBus() = 0;

  // Invoked before `ShutdownDBus()`.
  virtual void PreShutdownDBus() = 0;
};

// Initializes the D-Bus thread manager and Chrome D-Bus services for Ash.
void InitializeDBus();

// D-Bus clients may depend on feature list. This initializes only those clients
// in Ash, and must be called after feature list initialization.
void InitializeFeatureListDependentDBus();

// Shuts down the D-Bus thread manager and Chrome D-Bus services for Ash.
void ShutdownDBus();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_ASH_DBUS_HELPER_H_
