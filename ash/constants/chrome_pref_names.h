// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_CHROME_PREF_NAMES_H_
#define ASH_CONSTANTS_CHROME_PREF_NAMES_H_

// !!!NO NEW PREFS SHOULD BE ADDED.!!!
//
// This file contains a few copy of web browser preference/local_state key
// constants used in ChromeOS's OS system parts.
// Conceptually, OS should not depend on details of an app, but for historical
// reasons we have designed/implemented the system in the way.
// The constants must have the same value with the ones defined in
// chrome/common/pref_names.h, and so should have static_assert() there.
//
// TODO(crbug.com/487139800): Remove/Reduce the number of constants in this
// file.

namespace ash::chrome_prefs {

// Sorted in the lexicographical order.
inline constexpr char kAudioCaptureAllowed[] = "hardware.audio_capture_enabled";
inline constexpr char kDevToolsAvailability[] = "devtools.availability";
inline constexpr char kDisableAuthNegotiateCnameLookup[] =
    "auth.disable_negotiate_cname_lookup";
inline constexpr char kDisableScreenshots[] = "disable_screenshots";
inline constexpr char kDnsOverHttpsMode[] = "dns_over_https.mode";
inline constexpr char kDnsOverHttpsTemplates[] = "dns_over_https.templates";
inline constexpr char kDownloadDefaultDirectory[] =
    "download.default_directory";
inline constexpr char kForceEphemeralProfiles[] = "profile.ephemeral_mode";
inline constexpr char kPdfAnnotationsEnabled[] = "pdf.enable_annotations";
inline constexpr char kPrinterTypeDenyList[] =
    "printing.printer_type_deny_list";
inline constexpr char kRestoreOnStartup[] = "session.restore_on_startup";
inline constexpr char kRlzPingDelaySeconds[] = "rlz_ping_delay";
inline constexpr char kSaveFileDefaultDirectory[] =
    "savefile.default_directory";
inline constexpr char kSelectFileLastDirectory[] = "selectfile.last_directory";
inline constexpr char kSitePerProcess[] = "site_isolation.site_per_process";
inline constexpr char kUserFeedbackAllowed[] = "feedback_allowed";
inline constexpr char kVideoCaptureAllowed[] = "hardware.video_capture_enabled";
inline constexpr char kVideoCaptureAllowedUrls[] =
    "hardware.video_capture_allowed_urls";
inline constexpr char kWasRestarted[] = "was.restarted";

}  // namespace ash::chrome_prefs

#endif  // ASH_CONSTANTS_CHROME_PREF_NAMES_H_
