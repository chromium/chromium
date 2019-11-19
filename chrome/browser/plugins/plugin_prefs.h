// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

class Profile;

namespace content {
struct WebPluginInfo;
}

// This class stores information about whether a plugin or a plugin group is
// enabled or disabled.
// Except where otherwise noted, it can be used on every thread.
class PluginPrefs : public RefcountedKeyedService {
 public:
  enum PolicyStatus {
    NO_POLICY = 0,  // Neither enabled or disabled by policy.
    POLICY_ENABLED,
    POLICY_DISABLED,
  };

  // Returns the instance associated with |profile|, creating it if necessary.
  static scoped_refptr<PluginPrefs> GetForProfile(Profile* profile);

  // Usually the PluginPrefs associated with a TestingProfile is NULL.
  // This method overrides that for a given TestingProfile, returning the newly
  // created PluginPrefs object.
  static scoped_refptr<PluginPrefs> GetForTestingProfile(Profile* profile);

  // Creates a new instance. This method should only be used for testing.
  PluginPrefs();

  // Associates this instance with |prefs|. This enables or disables
  // plugin groups as defined by the user's preferences.
  // This method should only be called on the UI thread.
  void SetPrefs(PrefService* prefs);

  // Returns whether there is a policy enabling or disabling plugins of the
  // given name.
  PolicyStatus PolicyStatusForPlugin(const base::string16& name) const;

  // Returns whether the plugin is enabled or not.
  bool IsPluginEnabled(const content::WebPluginInfo& plugin) const;

  void set_profile(Profile* profile) { profile_ = profile; }

  // RefCountedProfileKeyedBase method override.
  void ShutdownOnUIThread() override;

 private:
  friend class base::RefCountedThreadSafe<PluginPrefs>;
  friend class PDFIFrameNavigationThrottleTest;
  friend class PluginPrefsTest;
  friend class PrintPreviewDialogControllerBrowserTest;

  ~PluginPrefs() override;

  // Callback for changes to the AlwaysOpenPdfExternally policy.
  void UpdatePdfPolicy(const std::string& pref_name);

  // Allows unit tests to directly set the AlwaysOpenPdfExternally pref.
  void SetAlwaysOpenPdfExternallyForTests(bool always_open_pdf_externally);

  bool always_open_pdf_externally_ = false;

  Profile* profile_ = nullptr;

  // Weak pointer, owned by the profile.
  PrefService* prefs_ = nullptr;

  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PluginPrefs);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_H_
