// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_logging.h"

#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/installable/installable_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace {

// Error message strings corresponding to the InstallableStatusCode enum.
static const char kNotInMainFrameMessage[] =
    "Page is not loaded in the main frame";
static const char kNotFromSecureOriginMessage[] =
    "Page is not served from a secure origin";
static const char kNoManifestMessage[] = "Page has no manifest <link> URL";
static const char kManifestEmptyMessage[] =
    "Manifest could not be fetched, is empty, or could not be parsed";
static const char kStartUrlNotValidMessage[] =
    "Manifest start URL is not valid";
static const char kManifestMissingNameOrShortNameMessage[] =
    "Manifest does not contain a 'name' or 'short_name' field";
static const char kManifestDisplayNotSupportedMessage[] =
    "Manifest 'display' property must be one of 'standalone', 'fullscreen', or "
    "'minimal-ui'";
static const char kManifestMissingSuitableIconMessage[] =
    "Manifest does not contain a suitable icon - PNG format of at least "
    "%dpx is required, the sizes attribute must be set, and the purpose "
    "attribute, if set, must include \"any\" or \"maskable\".";
static const char kNoMatchingServiceWorkerMessage[] =
    "No matching service worker detected. You may need to reload the page, or "
    "check that the service worker for the current page also controls the "
    "start URL from the manifest";
static const char kNoAcceptableIconMessage[] =
    "No supplied icon is at least %dpx square in PNG format";
static const char kCannotDownloadIconMessage[] =
    "Could not download a required icon from the manifest";
static const char kNoIconAvailableMessage[] =
    "Downloaded icon was empty or corrupted";
static const char kPlatformNotSupportedOnAndroidMessage[] =
    "The specified application platform is not supported on Android";
static const char kNoIdSpecifiedMessage[] = "No Play store ID provided";
static const char kIdsDoNotMatchMessage[] =
    "The Play Store app URL and Play Store ID do not match";
static const char kAlreadyInstalledMessage[] = "The app is already installed";
static const char kUrlNotSupportedForWebApkMessage[] =
    "A URL in the manifest contains a username, password, or port";
static const char kInIncognitoMessage[] =
    "Page is loaded in an incognito window";
static const char kNotOfflineCapable[] = "Page does not work offline";
static const char kNoUrlForServiceWorker[] =
    "Could not check service worker without a 'start_url' field in the "
    "manifest";
static const char kPreferRelatedApplications[] =
    "Manifest specifies prefer_related_applications: true";

const std::string& GetMessagePrefix() {
  static base::NoDestructor<std::string> message_prefix(
      "Site cannot be installed: ");
  return *message_prefix;
}

}  // namespace

std::string GetErrorMessage(InstallableStatusCode code) {
  std::string message;
  switch (code) {
    case NO_ERROR_DETECTED:
    // These codes are solely used for UMA reporting.
    case RENDERER_EXITING:
    case RENDERER_CANCELLED:
    case USER_NAVIGATED:
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
      break;
    case NO_ID_SPECIFIED:
      message = kNoIdSpecifiedMessage;
      break;
    case IDS_DO_NOT_MATCH:
      message = kIdsDoNotMatchMessage;
      break;
    case ALREADY_INSTALLED:
      message = kAlreadyInstalledMessage;
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
    case NO_URL_FOR_SERVICE_WORKER:
      message = kNoUrlForServiceWorker;
      break;
    case PREFER_RELATED_APPLICATIONS:
      message = kPreferRelatedApplications;
      break;
  }

  return message;
}

void LogErrorToConsole(content::WebContents* web_contents,
                       InstallableStatusCode code) {
  if (!web_contents)
    return;

  std::string message = GetErrorMessage(code);

  if (message.empty())
    return;

  web_contents->GetMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, GetMessagePrefix() + message);
}
