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
  class Config {
   public:
    Config(base::apple::ScopedCFTypeRef<CFPropertyListRef> extension_id,
           base::apple::ScopedCFTypeRef<CFPropertyListRef> team_id,
           base::apple::ScopedCFTypeRef<CFPropertyListRef> hosts);
    Config(const Config&);
    Config(Config&&);
    Config& operator=(const Config&);
    Config& operator=(Config&&);
    ~Config();
    base::apple::ScopedCFTypeRef<CFPropertyListRef> extension_id;
    base::apple::ScopedCFTypeRef<CFPropertyListRef> team_id;
    base::apple::ScopedCFTypeRef<CFPropertyListRef> hosts;
  };

  virtual ~CFPreferencesObserver() = default;
  virtual void Subscribe(base::RepeatingClosure on_update) = 0;
  virtual void Unsubscribe() = 0;
  virtual base::OnceCallback<Config()> GetReadConfigCallback() = 0;
};

// Responsible for syncing 'Hosts' field of Apple's com.apple.extensiblesso
// property with Chrome's kExtensibleEnterpriseSSOConfiguredHosts pref.
// On construction it subscribes to the Darwin notification center to observe
// changes to said property. Unsubscribes on destruction.
class ExtensibleEnterpriseSSOPrefsHandler {
 public:
  explicit ExtensibleEnterpriseSSOPrefsHandler(PrefService* local_state);

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

  static const CFStringRef kOktaSSOExtensionID;
  static const CFStringRef kOktaSSOTeamID;

 private:
  void OnConfigRead(base::ListValue res);

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
