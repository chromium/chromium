// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_

// The flow for geolocation permissions on Android needs to take into account
// the global geolocation settings so it differs from the desktop one. It
// works as follows.
// GeolocationPermissionContextAndroid::RequestPermission intercepts the flow
// and proceeds to check the system location.
// This will in fact check several possible settings
//     - The global system geolocation setting
//     - The Google location settings on pre KK devices
//     - An old internal Chrome setting on pre-JB MR1 devices
// With all that information it will decide if system location is enabled.
// If enabled, it proceeds with the per site flow via
// GeolocationPermissionContext (which will check per site permissions, create
// infobars, etc.).
//
// Otherwise the permission is already decided.
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/android/location_settings.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "components/location/android/location_settings_dialog_context.h"
#include "components/location/android/location_settings_dialog_outcome.h"

namespace content {
class WebContents;
}

namespace infobars {
class InfoBar;
}

class GURL;
class PrefRegistrySimple;

class GeolocationPermissionContextAndroid
    : public GeolocationPermissionContext {
 public:
  // This enum is used in histograms, thus is append only. Do not re-order or
  // remove any entries, or add any except at the end.
  enum class LocationSettingsDialogBackOff {
    kNoBackOff,
    kOneWeek,
    kOneMonth,
    kThreeMonths,
    kCount,
  };

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit GeolocationPermissionContextAndroid(Profile* profile);
  ~GeolocationPermissionContextAndroid() override;

 private:
  friend class GeolocationPermissionContextTests;
  friend class PermissionManagerTest;

  static void AddDayOffsetForTesting(int days);
  static void SetDSEOriginForTesting(const char* dse_origin);

  // GeolocationPermissionContext:
  void RequestPermission(content::WebContents* web_contents,
                         const PermissionRequestID& id,
                         const GURL& requesting_frame_origin,
                         bool user_gesture,
                         BrowserPermissionCallback callback) override;
  void UserMadePermissionDecision(const PermissionRequestID& id,
                                  const GURL& requesting_origin,
                                  const GURL& embedding_origin,
                                  ContentSetting content_setting) override;
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting) override;
  PermissionResult UpdatePermissionStatusWithDeviceStatus(
      PermissionResult result,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  // Functions to handle back off for showing the Location Settings Dialog.
  std::string GetLocationSettingsBackOffLevelPref(bool is_default_search) const;
  std::string GetLocationSettingsNextShowPref(bool is_default_search) const;
  bool IsInLocationSettingsBackOff(bool is_default_search) const;
  void ResetLocationSettingsBackOff(bool is_default_search);
  void UpdateLocationSettingsBackOff(bool is_default_search);
  LocationSettingsDialogBackOff LocationSettingsBackOffLevel(
      bool is_default_search) const;

  // Returns whether location access is possible for the given origin. Ignores
  // Location Settings Dialog backoff, as the backoff is ignored if the user
  // will be prompted for permission.
  bool IsLocationAccessPossible(content::WebContents* web_contents,
                                const GURL& requesting_origin,
                                bool user_gesture);

  bool IsRequestingOriginDSE(const GURL& requesting_origin) const;

  void HandleUpdateAndroidPermissions(const PermissionRequestID& id,
                                      const GURL& requesting_frame_origin,
                                      const GURL& embedding_origin,
                                      BrowserPermissionCallback callback,
                                      bool permissions_updated);

  // Will return true if the location settings dialog will be shown for the
  // given origins. This is true if the location setting is off, the dialog can
  // be shown, any gesture requirements for the origin are met, and the dialog
  // is not being suppressed for backoff.
  bool CanShowLocationSettingsDialog(const GURL& requesting_origin,
                                     bool user_gesture,
                                     bool ignore_backoff) const;

  void OnLocationSettingsDialogShown(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool persist,
      ContentSetting content_setting,
      LocationSettingsDialogOutcome prompt_outcome);

  void FinishNotifyPermissionSet(const PermissionRequestID& id,
                                 const GURL& requesting_origin,
                                 const GURL& embedding_origin,
                                 BrowserPermissionCallback callback,
                                 bool persist,
                                 ContentSetting content_setting);

  // Overrides the LocationSettings object used to determine whether
  // system and Chrome-wide location permissions are enabled.
  void SetLocationSettingsForTesting(
      std::unique_ptr<LocationSettings> settings);

  std::unique_ptr<LocationSettings> location_settings_;

  PermissionRequestID location_settings_dialog_request_id_;
  BrowserPermissionCallback location_settings_dialog_callback_;

  // Must be the last member, to ensure that it will be destroyed first, which
  // will invalidate weak pointers.
  base::WeakPtrFactory<GeolocationPermissionContextAndroid> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContextAndroid);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
