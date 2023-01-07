// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_SESSION_STARTUP_PREF_H__
#define CHROME_BROWSER_PREFS_SESSION_STARTUP_PREF_H__

#include <vector>

#include "url/gurl.h"

class PrefService;
class Profile;

#if !BUILDFLAG(IS_ANDROID)
struct StartupTab;
using StartupTabs = std::vector<StartupTab>;
#endif

namespace user_prefs {
class PrefRegistrySyncable;
}

// StartupPref specifies what should happen at startup for a specified profile.
// StartupPref is stored in the preferences for a particular profile.
struct SessionStartupPref {
  // Integer values should not be changed because reset reports depend on these.
  enum Type {
    // Indicates the user wants to open the New Tab page.
    DEFAULT = 0,

    // Indicates the user wants to restore the last session.
    LAST = 2,

    // Indicates the user wants to open a specific set of URLs. The URLs
    // are contained in urls.
    URLS = 3,

    // Indicates the user wants to restore the last session and open a specific
    // set of URLs. The URLs are contained in urls.
    LAST_AND_URLS = 4,
  };

  // For historical reasons the enum and value registered in the prefs don't
  // line up. These are the values registered in prefs.
  // The values are also recorded in Settings.StartupPageLoadSettings histogram,
  // so make sure to update histograms.xml if you change these.
  enum PrefValue {
    kPrefValueLast = 1,
    kPrefValueURLs = 4,
    kPrefValueNewTab = 5,
    kPrefValueLastAndURLs = 6,
    kPrefValueMax = 7,
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the default value for |type|.
  static Type GetDefaultStartupType();

  // What should happen on startup for the specified profile.
  static void SetStartupPref(Profile* profile, const SessionStartupPref& pref);
  static void SetStartupPref(PrefService* prefs,
                             const SessionStartupPref& pref);
  static SessionStartupPref GetStartupPref(const Profile* profile);
  static SessionStartupPref GetStartupPref(const PrefService* prefs);

  // Whether the startup type and URLs are managed via mandatory policy.
  static bool TypeIsManaged(const PrefService* prefs);
  static bool URLsAreManaged(const PrefService* prefs);

  // Whether the startup type has a recommended value (regardless of whether or
  // not that value is in use).
  static bool TypeHasRecommendedValue(const PrefService* prefs);

  // Whether the startup type has not been overridden from its default.
  static bool TypeIsDefault(const PrefService* prefs);

  // Converts an integer pref value to a SessionStartupPref::Type.
  static SessionStartupPref::Type PrefValueToType(int pref_value);

  explicit SessionStartupPref(Type type);

  SessionStartupPref(const SessionStartupPref& other);

  ~SessionStartupPref();

  // Returns true if |type| is indicating last session should be restored.
  bool ShouldRestoreLastSession() const;

  // Returns true if |type| is indicating a specific set of URLs should be
  // opened.
  bool ShouldOpenUrls() const;

#if !BUILDFLAG(IS_ANDROID)
  // Convert to StartupTabs.
  StartupTabs ToStartupTabs() const;
#endif

  // What to do on startup.
  Type type;

  // The URLs to open. Only used if |type| is URLS or LAST_AND_URLS.
  std::vector<GURL> urls;
};

#endif  // CHROME_BROWSER_PREFS_SESSION_STARTUP_PREF_H__
