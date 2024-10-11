// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_WEB_APP_ID_CONSTANTS_H_
#define ASH_CONSTANTS_WEB_APP_ID_CONSTANTS_H_

namespace web_app {

// The URLs used to generate the app IDs MUST match the start_url field of the
// manifest served by the PWA.
// Please maintain the alphabetical order when adding new app IDs.

// Generated as: web_app::GenerateAppId(/*manifest_id=*/"/", GURL(
//     "https://new.express.adobe.com/"))
inline constexpr char kAdobeExpressAppId[] = "jbgdhngddinepfilfcmbcioimbgcolma";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://calculator.apps.chrome/"))
inline constexpr char kCalculatorAppId[] = "oabkinaljpjeilageghcdlnekhphhphl";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://camera-app/views/main.html"))
inline constexpr char kCameraAppId[] = "njfbnohfdkmbmnjapinfcopialeghnmh";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://canvas.apps.chrome/"))
inline constexpr char kCanvasAppId[] = "ieailfmhaghpphfffooibmlghaeopach";

// Generated as: web_app::GenerateAppIdFromManifestId(
//     web_app::GenerateManifestIdFromStartUrlOnly(GURL(
//         "https://gemini.google.com/")));
inline constexpr char kContainerAppId[] = "caidcmannjgahlnbpmidmiecjcoiiigg";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome-untrusted://crosh/"))
inline constexpr char kCroshAppId[] = "cgfnfgkafmcdkdgilmojlnaadileaach";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://cursive.apps.chrome/"))
inline constexpr char kCursiveAppId[] = "apignacaigpffemhdbhmnajajaccbckh";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt,
// GURL("chrome://diagnostics/"))
inline constexpr char kDiagnosticsAppId[] = "keejpcfcpecjhmepmpcfgjemkmlicpam";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt,
// GURL("chrome://accessory-update/"))
inline constexpr char kFirmwareUpdateAppId[] =
    "nedcdcceagjbkiaecmdbpafcmlhkiifa";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://mail.google.com/mail/?usp=installed_webapp"))
inline constexpr char kGmailAppId[] = "fmgjjmmmlfnkbppncabfkddbjimcfncm";
inline constexpr char kGmailManifestId[] =
    "https://mail.google.com/mail/?usp=installed_webapp";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://calendar.google.com/calendar/r"))
inline constexpr char kGoogleCalendarAppId[] =
    "kjbdgfilnfhdoflbpgamdcdgpehopbep";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://mail.google.com/chat/"))
inline constexpr char kGoogleChatAppId[] = "mdpkiolbdkhdjpekfbkbmhigcaggjagi";

// Generated as: web_app::GenerateAppIdFromManifestId(webapps::ManifestId(
//     "https://classroom.google.com/?lfhs=2"))
inline constexpr char kGoogleClassroomAppId[] =
    "kjcjfjccmpngedeildfijeanhihmolck";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://docs.google.com/document/?usp=installed_webapp"))
inline constexpr char kGoogleDocsAppId[] = "mpnpojknpmmopombnjdcgaaiekajbnjb";
inline constexpr char kGoogleDocsManifestId[] =
    "https://docs.google.com/document/?usp=installed_webapp";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://drive.google.com/?lfhs=2"))
inline constexpr char kGoogleDriveAppId[] = "aghbiahbpaijignceidepookljebhfak";
inline constexpr char kGoogleDriveManifestId[] =
    "https://drive.google.com/?lfhs=2";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://keep.google.com/?usp=installed_webapp"))
inline constexpr char kGoogleKeepAppId[] = "eilembjdkfgodjkcjnpgpaenohkicgjd";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://www.google.com/maps?force=tt&source=ttpwa"))
inline constexpr char kGoogleMapsAppId[] = "mnhkaebcjjhencmpkapnbdaogjamfbcj";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://meet.google.com/landing?lfhs=2"))
inline constexpr char kGoogleMeetAppId[] = "kjgfgldnnfoeklkmfkjfagphfepbbdan";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://play.google.com/store/movies?usp=installed_webapp"))
inline constexpr char kGoogleMoviesAppId[] = "aiihaadhfoadjgjcegeomiajkajbjlcn";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://news.google.com/?lfhs=2"))
inline constexpr char kGoogleNewsAppId[] = "kfgapjallbhpciobgmlhlhokknljkgho";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://docs.google.com/spreadsheets/?usp=installed_webapp"))
inline constexpr char kGoogleSheetsAppId[] = "fhihpiojkbmbpdjeoajapmgkhlnakfjf";
inline constexpr char kGoogleSheetsManifestId[] =
    "https://docs.google.com/spreadsheets/?usp=installed_webapp";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://docs.google.com/presentation/?usp=installed_webapp"))
inline constexpr char kGoogleSlidesAppId[] = "kefjledonklijopmnomlcbpllchaibag";
inline constexpr char kGoogleSlidesManifestId[] =
    "https://docs.google.com/presentation/?usp=installed_webapp";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://graduation/"))
inline constexpr char kGraduationAppId[] = "dcmgllglecpogfcjmdkdncendnjphdcd";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://help-app/"))
inline constexpr char kHelpAppId[] = "nbljnnecbjbmifnoehiemkgefbnpoeak";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://discover.apps.chrome/"))
inline constexpr char kMallAppId[] = "hmiijpebigdebehjghkpkediohageppo";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://media-app/"))
inline constexpr char kMediaAppId[] = "jhdjimmaggjajfjphpljagpgkidjilnj";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://messages.google.com/web/"))
inline constexpr char kMessagesAppId[] = "hpfldicfbfomlpcikngkocigghgafkph";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://messages-web.sandbox.google.com/web/"))
inline constexpr char kMessagesDogfoodDeprecatedAppId[] =
    "gkgiochgbaoelfjibmnaomdepldjceib";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://www.microsoft365.com/?from=Homescreen"))
inline constexpr char kMicrosoft365AppId[] = "onhfoihkhodaeblmangmjjgfpfehnlkm";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://test-system-app/pwa.html"))
inline constexpr char kMockSystemAppId[] = "maphiehpiinjgiaepbljmopkodkadcbh";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://play.geforcenow.com/mall/"))
inline constexpr char kNvidiaGeForceNowAppId[] =
    "egmafekfmcnknbdlbfbhafbllplmjlhn";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://os-feedback/"))
inline constexpr char kOsFeedbackAppId[] = "iffgohomcomlpmkfikfffagkkoojjffm";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://os-settings/"))
inline constexpr char kOsSettingsAppId[] = "odknhmnlageboeamepcngndbggdpaobj";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://personalization/"))
inline constexpr char kPersonalizationAppId[] =
    "glenkcidjgckcomnliblmkokolehpckn";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://books.google.com/ebooks/app"))
inline constexpr char kPlayBooksAppId[] = "jglfhlbohpgcbefmhdmpancnijacbbji";

// Generated as:web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//      "chrome://print-management/"))
inline constexpr char kPrintManagementAppId[] =
    "fglkccnmnaankjodgccmiodmlkpaiodc";

// Generated as:web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//      "chrome://recorder-app/"))
inline constexpr char kRecorderAppId[] = "aegafoechlhchmknlbhmofidaodfkhhk";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt,
// GURL("chrome://sanitize/"))
inline constexpr char kSanitizeAppId[] = "kochjemhjheifpfhkppkameonoheenjk";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt,
// GURL("chrome://scanning/"))
inline constexpr char kScanningAppId[] = "cdkahakpgkdaoffdmfgnhgomkelkocfo";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://settings/"))
inline constexpr char kSettingsAppId[] = "inogagmajamaleonmanpkpkkigmklfad";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://shortcut-customization"))
inline constexpr char kShortcutCustomizationAppId[] =
    "ihgeegogifolehadhdgelgcnbnmemikp";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://shimless-rma/"))
inline constexpr char kShimlessRMAAppId[] = "ijolhdommgkkhpenofmpkkhlepahelcm";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://www.showtime.com/"))
inline constexpr char kShowtimeAppId[] = "eoccpgmpiempcflglfokeengliildkag";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://www.youtube.com/?feature=ytca"))
inline constexpr char kYoutubeAppId[] = "agimnkijcaahngcdmfeangaknmldooml";
inline constexpr char kYoutubeManifestId[] =
    "https://www.youtube.com/?feature=ytca";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://music.youtube.com/?source=pwa"))
inline constexpr char kYoutubeMusicAppId[] = "cinhimbnkkaeohfgghhklpknlkffjgod";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://tv.youtube.com/"))
inline constexpr char kYoutubeTVAppId[] = "kiemjbkkegajmpbobdfngbmjccjhnofh";

#if !defined(OFFICIAL_BUILD)
// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "chrome://sample-system-web-app"))
inline constexpr char kSampleSystemWebAppId[] =
    "jalmdcokfklmaoadompgacjlcomfckcf";
#endif  // !defined(OFFICIAL_BUILD)

// Generated as: web_app::GenerateAppId(/*manifest_id=*/"", GURL(
//     "chrome://password-manager/?source=pwa"))
inline constexpr char kPasswordManagerAppId[] =
    "kajebgjangihfbkjfejcanhanjmmbcfd";

}  // namespace web_app

#endif  // ASH_CONSTANTS_WEB_APP_ID_CONSTANTS_H_
