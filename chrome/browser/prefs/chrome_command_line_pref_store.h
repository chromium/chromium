// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_CHROME_COMMAND_LINE_PREF_STORE_H_
#define CHROME_BROWSER_PREFS_CHROME_COMMAND_LINE_PREF_STORE_H_

#include "base/command_line.h"
#include "components/prefs/command_line_pref_store.h"

// This PrefStore keeps track of preferences set by command-line switches,
// such as proxy settings.
class ChromeCommandLinePrefStore : public CommandLinePrefStore {
 public:
  explicit ChromeCommandLinePrefStore(const base::CommandLine* command_line);

  ChromeCommandLinePrefStore(const ChromeCommandLinePrefStore&) = delete;
  ChromeCommandLinePrefStore& operator=(const ChromeCommandLinePrefStore&) =
      delete;

 protected:
  ~ChromeCommandLinePrefStore() override;

  // Logs a message and returns false if the proxy switches are
  // self-contradictory. Protected so it can be used in unit testing.
  // TODO(bauerb): make this method public and remove the subclass, which calls
  // this method, from the test.
  bool ValidateProxySwitches();

 private:
  friend class TestCommandLinePrefStore;

  // Using the string and boolean maps, apply command-line switches to their
  // corresponding preferences in this pref store.
  void ApplySimpleSwitches();

  // Determines the proxy mode preference from the given proxy switches.
  void ApplyProxyMode();

  // Apply the SSL/TLS preferences from the given switches.
  void ApplySSLSwitches();

  // Determines whether the background mode is force-disabled.
  void ApplyBackgroundModeSwitches();

  // Re-enables some ports that may have been disallowed for security reasons.
  void ApplyExplicitlyAllowedPortSwitch();

  // Mappings of command line switches to prefs.
  static const BooleanSwitchToPreferenceMapEntry boolean_switch_map_[];
  static const SwitchToPreferenceMapEntry string_switch_map_[];
  static const SwitchToPreferenceMapEntry path_switch_map_[];
  static const SwitchToPreferenceMapEntry integer_switch_map_[];
};

#endif  // CHROME_BROWSER_PREFS_CHROME_COMMAND_LINE_PREF_STORE_H_
