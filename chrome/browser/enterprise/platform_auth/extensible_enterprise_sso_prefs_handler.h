// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_PREFS_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_PREFS_HANDLER_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/values.h"

class PrefService;
class PrefRegistrySimple;

namespace enterprise_auth {

// Wraps the communication with the OS for subscribing to the Darwin
// notification system and reading com.apple.extensiblesso from CFPreferences.
class CFPreferencesObserver {
 public:
  // This class is a raw snapshot of the `com.apple.extensiblesso` MDM payload.
  // Compared to the ExtensibleEnterpriseSSOPrefsHandler::Config it makes no
  // assumptions about the type, format and validity of its fields.
  class RawConfig {
   public:
    RawConfig(base::apple::ScopedCFTypeRef<CFPropertyListRef> extension_id,
              base::apple::ScopedCFTypeRef<CFPropertyListRef> team_id,
              base::apple::ScopedCFTypeRef<CFPropertyListRef> hosts);
    RawConfig(const RawConfig&);
    RawConfig(RawConfig&&);
    RawConfig& operator=(const RawConfig&);
    RawConfig& operator=(RawConfig&&);
    ~RawConfig();
    base::apple::ScopedCFTypeRef<CFPropertyListRef> extension_id;
    base::apple::ScopedCFTypeRef<CFPropertyListRef> team_id;
    base::apple::ScopedCFTypeRef<CFPropertyListRef> hosts;
  };

  virtual ~CFPreferencesObserver() = default;
  virtual void Subscribe(base::RepeatingClosure on_update) = 0;
  virtual void Unsubscribe() = 0;
  virtual base::OnceCallback<RawConfig()> GetReadConfigCallback() = 0;
};

// Responsible for syncing 'Hosts' field of Apple's com.apple.extensiblesso
// property with Chrome's kExtensibleEnterpriseSSOConfiguredHosts pref.
// On construction it subscribes to the Darwin notification center to observe
// changes to said property. Unsubscribes on destruction.
class ExtensibleEnterpriseSSOPrefsHandler {
 public:
  explicit ExtensibleEnterpriseSSOPrefsHandler(PrefService* local_state);

  // Represents the `com.apple.extensiblesso` MDM payload installed on the
  // device.
  struct Config {
    // `ExtensionIdentifier` entry of the payload. Never empty.
    std::string extension_id;

    // `TeamIdentifier` entry of the payload. Never empty.
    std::string team_id;

    // `Hosts` entry of the payload. Non-string and empty entries are dropped.
    std::vector<std::string> hosts;
  };

  ~ExtensibleEnterpriseSSOPrefsHandler();

  ExtensibleEnterpriseSSOPrefsHandler(ExtensibleEnterpriseSSOPrefsHandler&) =
      delete;
  ExtensibleEnterpriseSSOPrefsHandler& operator=(
      const ExtensibleEnterpriseSSOPrefsHandler&) = delete;

  // Must be called before this object is created.
  static void RegisterPrefs(PrefRegistrySimple* pref_registry);

  // Asynchronously reads the com.apple.extensiblesso property on a thread pool.
  // On completion the result is applied to Chrome's preferences.
  // Must be called on the main thread.
  void UpdatePrefs();

 private:
  void OnConfigRead(std::optional<Config> config);

  friend class ScopedCFPreferenceObserverOverride;
  static void OverrideCFPreferenceObserverForTesting(
      base::RepeatingCallback<std::unique_ptr<CFPreferencesObserver>()>
          cf_prefs_observer_override);

  std::unique_ptr<CFPreferencesObserver> cf_preferences_observer_;
  raw_ptr<PrefService> local_state_{nullptr};

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ExtensibleEnterpriseSSOPrefsHandler> weak_ptr_factory_{
      this};
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_PREFS_HANDLER_H_
