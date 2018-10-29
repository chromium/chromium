// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_logging.h"

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/installable/installable_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace {

const std::string& GetMessagePrefix() {
  static base::NoDestructor<std::string> message_prefix(
      "Site cannot be installed: ");
  return *message_prefix;
}

// Error message strings corresponding to the InstallableStatusCode enum.
static const char kRendererExitingMessage[] =
    "the page is in the process of being closed";
static const char kRendererCancelledMessage[] =
    "the page has requested the banner prompt be cancelled";
static const char kUserNavigatedMessage[] =
    "the page was navigated before the banner could be shown";
static const char kNotInMainFrameMessage[] =
    "the page is not loaded in the main frame";
static const char kNotFromSecureOriginMessage[] =
    "the page is not served from a secure origin";
static const char kNoManifestMessage[] =
    "the page has no manifest <link> URL";
static const char kManifestEmptyMessage[] =
    "the manifest could not be fetched, is empty, or could not be parsed";
static const char kStartUrlNotValidMessage[] =
    "the start URL in manifest is not valid";
static const char kManifestMissingNameOrShortNameMessage[] =
    "one of manifest name or short name must be specified";
static const char kManifestDisplayNotSupportedMessage[] =
    "the manifest display property must be set to 'standalone' or 'fullscreen'";
static const char kManifestMissingSuitableIconMessage[] =
    "the manifest does not contain a suitable icon - PNG format of at least "
    "%dpx is required, the sizes attribute must be set, and the purpose "
    "attribute, if set, must include \"any\".";
static const char kNoMatchingServiceWorkerMessage[] =
    "no matching service worker detected. You may need to reload the page, or "
    "check that the service worker for the current page also controls the "
    "start URL from the manifest";
static const char kNoAcceptableIconMessage[] =
    "a %dpx square PNG icon is required, but no supplied icon meets this "
    "requirement";
static const char kCannotDownloadIconMessage[] =
    "could not download a required icon from the manifest";
static const char kNoIconAvailableMessage[] =
    "icon downloaded from the manifest was empty or corrupted";
static const char kPlatformNotSupportedOnAndroidMessage[] =
    "the specified application platform is not supported on Android";
static const char kNoIdSpecifiedMessage[] =
    "no Play store ID provided";
static const char kIdsDoNotMatchMessage[] =
    "a Play Store app URL and Play Store ID were specified in the manifest, "
    "but they do not match";
static const char kUrlNotSupportedForWebApkMessage[] =
    "a URL in the web manifest contains a username, password, or port";
static const char kInIncognitoMessage[] =
    "the page is loaded in an incognito window";
static const char kNotOfflineCapable[] = "the page does not work offline";
}  // namespace

void LogErrorToConsole(content::WebContents* web_contents,
                       InstallableStatusCode code) {
  if (!web_contents)
    return;

  content::ConsoleMessageLevel severity = content::CONSOLE_MESSAGE_LEVEL_ERROR;
  std::string message;
  switch (code) {
    case NO_ERROR_DETECTED:
    // These codes are solely used for UMA reporting.
    case ALREADY_INSTALLED:
    case INSUFFICIENT_ENGAGEMENT:
    case PACKAGE_NAME_OR_START_URL_EMPTY:
    case PREVIOUSLY_BLOCKED:
    case PREVIOUSLY_IGNORED:
    case SHOWING_NATIVE_APP_BANNER:
    case SHOWING_WEB_APP_BANNER:
    case FAILED_TO_CREATE_BANNER:
    case WAITING_FOR_MANIFEST:
    case WAITING_FOR_INSTALLABLE_CHECK:
    case NO_GESTURE:
    case WAITING_FOR_NATIVE_DATA:
    case SHOWING_APP_INSTALLATION_DIALOG:
    case MAX_ERROR_CODE:
      return;
    case RENDERER_EXITING:
      message = kRendererExitingMessage;
      break;
    case RENDERER_CANCELLED:
      message = kRendererCancelledMessage;
      severity = content::CONSOLE_MESSAGE_LEVEL_INFO;
      break;
    case USER_NAVIGATED:
      message = kUserNavigatedMessage;
      severity = content::CONSOLE_MESSAGE_LEVEL_WARNING;
      break;
    case NOT_IN_MAIN_FRAME:
      message = kNotInMainFrameMessage;
      break;
    case NOT_FROM_SECURE_ORIGIN:
      message = kNotFromSecureOriginMessage;
      break;
    case NO_MANIFEST:
      message = kNoManifestMessage;
      break;
    case MANIFEST_EMPTY:
      message = kManifestEmptyMessage;
      break;
    case START_URL_NOT_VALID:
      message = kStartUrlNotValidMessage;
      break;
    case MANIFEST_MISSING_NAME_OR_SHORT_NAME:
      message = kManifestMissingNameOrShortNameMessage;
      break;
    case MANIFEST_DISPLAY_NOT_SUPPORTED:
      message = kManifestDisplayNotSupportedMessage;
      break;
    case MANIFEST_MISSING_SUITABLE_ICON:
      message =
          base::StringPrintf(kManifestMissingSuitableIconMessage,
                             InstallableManager::GetMinimumIconSizeInPx());
      break;
    case NO_MATCHING_SERVICE_WORKER:
      message = kNoMatchingServiceWorkerMessage;
      break;
    case NO_ACCEPTABLE_ICON:
      message =
          base::StringPrintf(kNoAcceptableIconMessage,
                             InstallableManager::GetMinimumIconSizeInPx());
      break;
    case CANNOT_DOWNLOAD_ICON:
      message = kCannotDownloadIconMessage;
      break;
    case NO_ICON_AVAILABLE:
      message = kNoIconAvailableMessage;
      break;
    case PLATFORM_NOT_SUPPORTED_ON_ANDROID:
      message = kPlatformNotSupportedOnAndroidMessage;
      severity = content::CONSOLE_MESSAGE_LEVEL_WARNING;
      break;
    case NO_ID_SPECIFIED:
      message = kNoIdSpecifiedMessage;
      break;
    case IDS_DO_NOT_MATCH:
      message = kIdsDoNotMatchMessage;
      break;
    case URL_NOT_SUPPORTED_FOR_WEBAPK:
      message = kUrlNotSupportedForWebApkMessage;
      break;
    case IN_INCOGNITO:
      message = kInIncognitoMessage;
      break;
    case NOT_OFFLINE_CAPABLE:
      message = kNotOfflineCapable;
      break;
  }

  web_contents->GetMainFrame()->AddMessageToConsole(
      severity, GetMessagePrefix() + message);
}
