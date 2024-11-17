// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SYSTEM_APPS_PUBLIC_SYSTEM_WEB_APP_TYPE_H_
#define ASH_WEBUI_SYSTEM_APPS_PUBLIC_SYSTEM_WEB_APP_TYPE_H_

#include "build/chromeos_buildflags.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#error "Ash-only"
#endif

namespace ash {

// An enum that lists the different System Apps that exist. Can be used to
// retrieve the App ID from the underlying Web App system.
//
// These values are persisted to the web_app database. Entries should not be
// renumbered and numeric values should never be reused.
//
// When deprecating, comment out the entry so that it's not accidentally
// re-used.
enum class SystemWebAppType {
  FILE_MANAGER = 1,
  // TELEMETRY_DEPRECATED = 2,

  // A sample System Web App to illustrate SWA development best practices, and
  // various SWA platform features.
  //
  // This App is only enabled on non-official builds. You can find a brief SWA
  // platform introduction (Google internal) at: http://go/system-web-apps.
  //
  // Source: //ash/webui/sample_system_web_app_ui/
  // Contact: dominicshulz@google.com, ortuno@chromium.org
  SAMPLE = 3,

  SETTINGS = 4,
  CAMERA = 5,
  TERMINAL = 6,
  MEDIA = 7,
  HELP = 8,
  PRINT_MANAGEMENT = 9,
  SCANNING = 10,
  DIAGNOSTICS = 11,
  CONNECTIVITY_DIAGNOSTICS = 12,
  ECHE = 13,
  CROSH = 14,
  PERSONALIZATION = 15,
  SHORTCUT_CUSTOMIZATION = 16,

  // SHIMLESS RMA Flow is SWA that provides step by step guides for the
  // repair/RMA process.
  //
  // You can find information about this SWA at: http://go/shimless-ux.
  //
  // Source: //ash/webui/shimless_rma/
  // Contact: cros-device-enablement@google.com
  SHIMLESS_RMA = 17,

  // A System Web App that launches on Demo Mode startup, to display animated
  // content that highlights various features of ChromeOS
  //
  // Prefer to file bugs to the internal Demo Mode component:
  // b/components/812312
  //
  // Source: //ash/webui/demo_mode_app_ui/
  // Contact: cros-demo-mode-eng@google.com
  DEMO_MODE = 18,

  // OS FEEDBACK is a SWA that provides step by step guides to submit a
  // feedback report on Chrome OS.
  //
  // Source: //ash/webui/os_feedback_ui
  // contact: cros-device-enablement@google.com
  OS_FEEDBACK = 19,

  // Projector aka Screencast (go/projector-player-dd) aims to make it simple
  // for teachers and students to record and share instructional videos on a
  // Chromebook. This app enables teachers to create a library of
  // custom-tailored instructional content that students can search and view at
  // home.
  //
  // Source: //ash/webui/projector_app/
  // Contact: cros-projector@google.com
  // Buganizer component: b/components/1080013
  // This app is only included in Chrome-branded builds. Non-official builds
  // will have a mock page.
  PROJECTOR = 20,

  // OsUrlHandler was removed.
  // OS_URL_HANDLER = 21,

  // FIRMWARE UPDATE App is SWA that lets users update all their peripheral
  // firmwares in one place.
  // You can find information about this SWA at: http://go/fwupd-app.
  // Source: //ash/webui/firmware_update/
  // Contact: cros-device-enablement@google.com
  FIRMWARE_UPDATE = 22,

  // OsFlags is called by Lacros to show the chrome://flags page as
  // applications to the user. Note that this page is accessible to the user
  // as os://flags through search.
  // contact: skuhne@google.com
  OS_FLAGS = 23,

  // FaceML was deprecated.
  // FACE_ML = 24,

  // VC Background allows users to control webcam settings, including blur and
  // background image.
  // Source: //ash/webui/vc_background_ui/
  // Contact: assistive-eng@google.com
  VC_BACKGROUND = 25,

  // CrOS implementation of the print preview surface.
  // Source: //ash/webui/print_preview_cros/
  // Contact: cros-device-enablement@google.com
  PRINT_PREVIEW_CROS = 26,

  // Boca implementation.
  // Source: //ash/webui/boca_ui/
  // Contact: cros-edu-eng@google.com
  BOCA = 27,

  // Mall is an app for finding and installing other apps.
  // Source: //ash/webui/mall/
  // Contact: crosdev-commerce-eng@google.com
  MALL = 28,

  // CrOS SWA that performs a soft reset for the users.
  // Source: //ash/webui/sanitize_ui/
  // Contact: behnoodm@google.com
  // Contact: cryptohome-core@google.com
  OS_SANITIZE = 29,

  // Recorder app for ChromeOS.
  //
  // Source: //ash/webui/recorder_app_ui/
  // Contact: chromeos-recorder-app@google.com
  RECORDER = 30,

  // Graduation app for ChromeOS EDU users.
  //
  // Source: //ash/webui/graduation_ui/
  // Contact: cros-families-eng@google.com
  GRADUATION = 31,

  // When adding a new System App, remember to:
  //
  // 1. Add a corresponding histogram suffix in WebAppSystemAppInternalName
  //    (histogram_suffixes_list.xml). The suffix name should match the App's
  //    |internal_name|. This is for reporting per-app install results.
  //
  // 2. Add a corresponding proto enum entry (with the same numerical value) to
  //    SystemWebAppDataProto in system_web_app_data.proto. This is for
  //    identifying system apps during Chrome start-up (i.e. when
  //    SystemWebAppManager hasn't finished synchronizing all apps).
  //
  // 3. Add a comment above the enum entry in this file. It should include a
  //    description (what it does in one sentence), at least one email contact,
  //    source location (if it's in chromium source tree), and other relevant
  //    information.
  //
  //    Other relevant information should come in separate paragraphs after the
  //    description. This can be anything useful for triaging or routing bugs.
  //    For example, your team doesn't use chromium's bug tracker, the App is
  //    only available on certain devices.
  //
  //    Source location should point to where the App's WebUIController is
  //    defined. It doesn't have to include the complete source repository (e.g.
  //    if the App is hosted in internal repositories).
  //
  // 4. Put a blank line after each enum (before next enum's comment).
  //
  // 5. Use ash::LaunchSystemWebAppAsync to launch your SWA (with the type
  //    added above). This provides extra safety in edge cases (e.g. when in
  //    incognito or guest sessions).
  //
  // 6. Update kMaxValue.
  //
  // 7. Add your System Web App to |kSystemWebAppsMapping| in
  //    chrome/browser/apps/app_service/policy_util.cc to make it discoverable
  //    in policies.
  //
  // 8. Have one of System Web App Platform owners review the CL.
  //    See: //ash/webui/PLATFORM_OWNERS
  kMaxValue = GRADUATION,
};

}  // namespace ash

#endif  // ASH_WEBUI_SYSTEM_APPS_PUBLIC_SYSTEM_WEB_APP_TYPE_H_
