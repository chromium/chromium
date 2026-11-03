// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_SYNCED_THEME_BRIDGE_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_SYNCED_THEME_BRIDGE_H_

#include "base/android/scoped_java_ref.h"

class Profile;

// The C++ counterpart to NtpSyncedThemeBridge.java. It reports synced theme
// changes from NtpAndroidCustomBackgroundService to its Java peer, which it
// creates and owns.
//
// The service owns this bridge and creates it when Java first asks for the
// bridge of a profile, see
// NtpAndroidCustomBackgroundService::GetOrCreateSyncedThemeBridge(). Java
// doesn't hold a native pointer to it: calls from Java reach the service
// through static JNI methods keyed by Profile, so there is nothing for Java to
// destroy, which the process-wide NtpSyncedThemeManager couldn't do in time.
class NtpSyncedThemeBridge {
 public:
  // Creates the Java peer for `profile`.
  explicit NtpSyncedThemeBridge(Profile* profile);

  NtpSyncedThemeBridge(const NtpSyncedThemeBridge&) = delete;
  NtpSyncedThemeBridge& operator=(const NtpSyncedThemeBridge&) = delete;

  virtual ~NtpSyncedThemeBridge();

  // Returns the Java peer owned by this bridge.
  const base::android::ScopedJavaGlobalRef<jobject>& java_object() const {
    return j_java_obj_;
  }

  // Called when a Chrome color is synced from Chrome Sync.
  virtual void OnChromeColorSynced(int color_id);

  // Called when the default theme is restored from Chrome Sync.
  virtual void OnDefaultThemeSynced();

  // Called when the underlying Android theme sync bridge reaches readiness.
  virtual void OnThemeSyncReady();

  // Called when the theme collection background in preferences was updated
  // either by sync (`is_sync` is true) or by the local daily refresh
  // pre-fetching the next image (`is_sync` is false).
  virtual void OnThemeCollectionUpdated(bool is_sync);

 protected:
  // For mocks, which have no Java peer.
  NtpSyncedThemeBridge();

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_java_obj_;
};

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_SYNCED_THEME_BRIDGE_H_
