// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Tab Capture API.

#include "chrome/browser/extensions/api/tab_capture/tab_capture_api.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tab_capture/offscreen_tabs_owner.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"

using content::DesktopMediaID;
using content::WebContentsMediaCaptureId;
using extensions::api::tab_capture::MediaStreamConstraint;

namespace TabCapture = extensions::api::tab_capture;
namespace GetCapturedTabs = TabCapture::GetCapturedTabs;

namespace extensions {
namespace {

const char kCapturingSameTab[] = "Cannot capture a tab with an active stream.";
const char kFindingTabError[] = "Error finding tab to capture.";
const char kNoAudioOrVideo[] = "Capture failed. No audio or video requested.";
const char kGrantError[] =
    "Extension has not been invoked for the current page (see activeTab "
    "permission). Chrome pages cannot be captured.";

const char kNotWhitelistedForOffscreenTabApi[] =
    "Extension is not whitelisted for use of the unstable, in-development "
    "chrome.tabCapture.captureOffscreenTab API.";
const char kInvalidStartUrl[] =
    "Invalid/Missing/Malformatted starting URL for off-screen tab.";
const char kTooManyOffscreenTabs[] =
    "Extension has already started too many off-screen tabs.";
const char kCapturingSameOffscreenTab[] =
    "Cannot capture the same off-screen tab more than once.";

const char kInvalidOriginError[] = "Caller tab.url is not a valid URL.";
const char kInvalidTabIdError[] = "Invalid tab specified.";
const char kTabUrlNotSecure[] =
    "URL scheme for the specified tab is not secure.";

// Keys/values passed to renderer-side JS bindings.
const char kMediaStreamSource[] = "chromeMediaSource";
const char kMediaStreamSourceId[] = "chromeMediaSourceId";
const char kMediaStreamSourceTab[] = "tab";

// Tab Capture-specific video constraint to enable automatic resolution/rate
// throttling mode in the capture pipeline.
const char kEnableAutoThrottlingKey[] = "enableAutoThrottling";

bool OptionsSpecifyAudioOrVideo(const TabCapture::CaptureOptions& options) {
  return (options.audio && *options.audio) || (options.video && *options.video);
}

bool IsAcceptableOffscreenTabUrl(const GURL& url) {
  return url.is_valid() && (url.SchemeIsHTTPOrHTTPS() || url.SchemeIs("data"));
}

// Removes all mandatory and optional constraint entries that start with the
// "goog" prefix.  These are never needed and may cause the renderer-side
// getUserMedia() call to fail.  http://crbug.com/579729
//
// TODO(miu): Remove once tabCapture API is migrated to new constraints spec.
// http://crbug.com/579729
void FilterDeprecatedGoogConstraints(TabCapture::CaptureOptions* options) {
  const auto FilterGoogKeysFromDictionary = [](base::DictionaryValue* dict) {
    std::vector<std::string> bad_keys;
    base::DictionaryValue::Iterator it(*dict);
    for (; !it.IsAtEnd(); it.Advance()) {
      if (base::StartsWith(it.key(), "goog", base::CompareCase::SENSITIVE))
        bad_keys.push_back(it.key());
    }
    for (const std::string& k : bad_keys) {
      std::unique_ptr<base::Value> ignored;
      dict->RemoveWithoutPathExpansion(k, &ignored);
    }
  };

  if (options->audio_constraints) {
    FilterGoogKeysFromDictionary(
        &options->audio_constraints->mandatory.additional_properties);
    if (options->audio_constraints->optional) {
      FilterGoogKeysFromDictionary(
          &options->audio_constraints->optional->additional_properties);
    }
  }

  if (options->video_constraints) {
    FilterGoogKeysFromDictionary(
        &options->video_constraints->mandatory.additional_properties);
    if (options->video_constraints->optional) {
      FilterGoogKeysFromDictionary(
          &options->video_constraints->optional->additional_properties);
    }
  }
}

bool GetAutoThrottlingFromOptions(TabCapture::CaptureOptions* options) {
  bool enable_auto_throttling = false;
  if (options && options->video && *options->video) {
    if (options->video_constraints) {
      // Check for the Tab Capture-specific video constraint for enabling
      // automatic resolution/rate throttling mode in the capture pipeline.  See
      // implementation comments for content::WebContentsVideoCaptureDevice.
      base::DictionaryValue& props =
          options->video_constraints->mandatory.additional_properties;
      if (!props.GetBooleanWithoutPathExpansion(
              kEnableAutoThrottlingKey, &enable_auto_throttling)) {
        enable_auto_throttling = false;
      }
      // Remove the key from the properties to avoid an "unrecognized
      // constraint" error in the renderer.
      props.RemoveWithoutPathExpansion(kEnableAutoThrottlingKey, nullptr);
    }
  }

  return enable_auto_throttling;
}

DesktopMediaID BuildDesktopMediaID(content::WebContents* target_contents,
                                   TabCapture::CaptureOptions* options) {
  content::RenderFrameHost* const target_frame =
      target_contents->GetMainFrame();
  DesktopMediaID source(
      DesktopMediaID::TYPE_WEB_CONTENTS, DesktopMediaID::kNullId,
      WebContentsMediaCaptureId(target_frame->GetProcess()->GetID(),
                                target_frame->GetRoutingID(),
                                GetAutoThrottlingFromOptions(options), false));
  return source;
}

// Add Chrome-specific source identifiers to the MediaStreamConstraints objects
// in |options| to provide references to the |target_contents| to be captured.
void AddMediaStreamSourceConstraints(content::WebContents* target_contents,
                                     TabCapture::CaptureOptions* options,
                                     const std::string& device_id) {
  DCHECK(options);
  DCHECK(target_contents);

  MediaStreamConstraint* constraints_to_modify[2] = {nullptr, nullptr};

  if (options->audio && *options->audio) {
    if (!options->audio_constraints)
      options->audio_constraints.reset(new MediaStreamConstraint);
    constraints_to_modify[0] = options->audio_constraints.get();
  }

  if (options->video && *options->video) {
    if (!options->video_constraints)
      options->video_constraints.reset(new MediaStreamConstraint);
    constraints_to_modify[1] = options->video_constraints.get();
  }

  // Append chrome specific tab constraints.
  for (MediaStreamConstraint* msc : constraints_to_modify) {
    if (!msc)
      continue;
    base::DictionaryValue* constraint = &msc->mandatory.additional_properties;
    constraint->SetString(kMediaStreamSource, kMediaStreamSourceTab);
    constraint->SetString(kMediaStreamSourceId, device_id);
  }
}

// Find the last-active browser that matches a profile this ExtensionFunction
// can access.  We can't use FindLastActiveWithProfile() because we may want to
// include incognito profile browsers.
Browser* GetLastActiveBrowser(const Profile* profile,
                              const bool match_incognito_profile) {
  BrowserList* browser_list = BrowserList::GetInstance();
  Browser* target_browser = nullptr;
  for (auto iter = browser_list->begin_last_active();
       iter != browser_list->end_last_active(); ++iter) {
    Profile* browser_profile = (*iter)->profile();
    if (browser_profile == profile ||
        (match_incognito_profile &&
         browser_profile->GetOriginalProfile() == profile)) {
      target_browser = *iter;
      break;
    }
  }

  return target_browser;
}

}  // namespace

// Whitelisted extensions that do not check for a browser action grant because
// they provide API's. If there are additional extension ids that need
// whitelisting and are *not* the Media Router extension, add them to a new
// kWhitelist array.
const char* const kMediaRouterExtensionIds[] = {
    "enhhojjnijigcajfphajepfemndkmdlo",  // Dev
    "pkedcjkdefgpdelpbcmbmeomcjbeemfm",  // Stable
};

ExtensionFunction::ResponseAction TabCaptureCaptureFunction::Run() {
  std::unique_ptr<api::tab_capture::Capture::Params> params =
      TabCapture::Capture::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const bool match_incognito_profile = include_incognito_information();
  Browser* target_browser =
      GetLastActiveBrowser(profile, match_incognito_profile);
  if (!target_browser)
    return RespondNow(Error(kFindingTabError));

  content::WebContents* target_contents =
      target_browser->tab_strip_model()->GetActiveWebContents();
  if (!target_contents)
    return RespondNow(Error(kFindingTabError));

  const std::string& extension_id = extension()->id();

  // Make sure either we have been granted permission to capture through an
  // extension icon click or our extension is whitelisted.
  if (!extension()->permissions_data()->HasAPIPermissionForTab(
          SessionTabHelper::IdForTab(target_contents).id(),
          APIPermission::kTabCaptureForTab) &&
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWhitelistedExtensionID) != extension_id &&
      !SimpleFeature::IsIdInArray(extension_id, kMediaRouterExtensionIds,
                                  base::size(kMediaRouterExtensionIds))) {
    return RespondNow(Error(kGrantError));
  }

  if (!OptionsSpecifyAudioOrVideo(params->options))
    return RespondNow(Error(kNoAudioOrVideo));

  DesktopMediaID source =
      BuildDesktopMediaID(target_contents, &params->options);
  content::WebContents* const extension_web_contents = GetSenderWebContents();
  EXTENSION_FUNCTION_VALIDATE(extension_web_contents);
  TabCaptureRegistry* registry = TabCaptureRegistry::Get(browser_context());
  std::string device_id = registry->AddRequest(
      target_contents, extension_id, false, extension()->url(), source,
      extension()->name(), extension_web_contents);
  if (device_id.empty()) {
    // TODO(miu): Allow multiple consumers of single tab capture.
    // http://crbug.com/535336
    return RespondNow(Error(kCapturingSameTab));
  }
  FilterDeprecatedGoogConstraints(&params->options);
  AddMediaStreamSourceConstraints(target_contents, &params->options, device_id);

  // At this point, everything is set up in the browser process.  It's now up to
  // the custom JS bindings in the extension's render process to request a
  // MediaStream using navigator.webkitGetUserMedia().  The result dictionary,
  // passed to SetResult() here, contains the extra "hidden options" that will
  // allow the Chrome platform implementation for getUserMedia() to start the
  // virtual audio/video capture devices and set up all the data flows.  The
  // custom JS bindings can be found here:
  // chrome/renderer/resources/extensions/tab_capture_custom_bindings.js
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->MergeDictionary(params->options.ToValue().get());
  return RespondNow(OneArgument(std::move(result)));
}

ExtensionFunction::ResponseAction TabCaptureGetCapturedTabsFunction::Run() {
  TabCaptureRegistry* registry = TabCaptureRegistry::Get(browser_context());
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  if (registry)
    registry->GetCapturedTabs(extension()->id(), list.get());
  return RespondNow(OneArgument(std::move(list)));
}

ExtensionFunction::ResponseAction TabCaptureCaptureOffscreenTabFunction::Run() {
  std::unique_ptr<TabCapture::CaptureOffscreenTab::Params> params =
      TabCapture::CaptureOffscreenTab::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // Make sure the extension is whitelisted for using this API, regardless of
  // Chrome channel.
  //
  // TODO(miu): Use _api_features.json and extensions::Feature library instead.
  // http://crbug.com/537732
  const bool is_whitelisted_extension =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWhitelistedExtensionID) == extension()->id() ||
      SimpleFeature::IsIdInArray(extension()->id(), kMediaRouterExtensionIds,
                                 base::size(kMediaRouterExtensionIds));
  if (!is_whitelisted_extension)
    return RespondNow(Error(kNotWhitelistedForOffscreenTabApi));

  const GURL start_url(params->start_url);
  if (!IsAcceptableOffscreenTabUrl(start_url))
    return RespondNow(Error(kInvalidStartUrl));

  if (!OptionsSpecifyAudioOrVideo(params->options))
    return RespondNow(Error(kNoAudioOrVideo));

  content::WebContents* const extension_web_contents = GetSenderWebContents();
  EXTENSION_FUNCTION_VALIDATE(extension_web_contents);
  OffscreenTab* const offscreen_tab =
      OffscreenTabsOwner::Get(extension_web_contents)->OpenNewTab(
          start_url,
          DetermineInitialSize(params->options),
          (is_whitelisted_extension && params->options.presentation_id) ?
              *params->options.presentation_id : std::string());
  if (!offscreen_tab)
    return RespondNow(Error(kTooManyOffscreenTabs));

  content::WebContents* target_contents = offscreen_tab->web_contents();
  const std::string& extension_id = extension()->id();
  DesktopMediaID source =
      BuildDesktopMediaID(target_contents, &params->options);
  TabCaptureRegistry* registry = TabCaptureRegistry::Get(browser_context());
  std::string device_id = registry->AddRequest(
      target_contents, extension_id, true, extension()->url(), source,
      extension()->name(), extension_web_contents);
  if (device_id.empty()) {
    // TODO(miu): Allow multiple consumers of single tab capture.
    // http://crbug.com/535336
    return RespondNow(Error(kCapturingSameOffscreenTab));
  }
  FilterDeprecatedGoogConstraints(&params->options);
  AddMediaStreamSourceConstraints(target_contents, &params->options, device_id);

  // At this point, everything is set up in the browser process.  It's now up to
  // the custom JS bindings in the extension's render process to complete the
  // request.  See the comment at end of TabCaptureCaptureFunction::RunSync()
  // for more details.
  return RespondNow(OneArgument(params->options.ToValue()));
}

// static
gfx::Size TabCaptureCaptureOffscreenTabFunction::DetermineInitialSize(
    const TabCapture::CaptureOptions& options) {
  static const int kDefaultWidth = 1280;
  static const int kDefaultHeight = 720;

  if (!options.video_constraints)
    return gfx::Size(kDefaultWidth, kDefaultHeight);

  gfx::Size min_size;
  int width = -1;
  int height = -1;
  const base::DictionaryValue& mandatory_properties =
      options.video_constraints->mandatory.additional_properties;
  if (mandatory_properties.GetInteger("maxWidth", &width) && width >= 0 &&
      mandatory_properties.GetInteger("maxHeight", &height) && height >= 0) {
    return gfx::Size(width, height);
  }
  if (mandatory_properties.GetInteger("minWidth", &width) && width >= 0 &&
      mandatory_properties.GetInteger("minHeight", &height) && height >= 0) {
    min_size.SetSize(width, height);
  }

  // Use optional size constraints if no mandatory ones were provided.
  if (options.video_constraints->optional) {
    const base::DictionaryValue& optional_properties =
        options.video_constraints->optional->additional_properties;
    if (optional_properties.GetInteger("maxWidth", &width) && width >= 0 &&
        optional_properties.GetInteger("maxHeight", &height) && height >= 0) {
      if (min_size.IsEmpty()) {
        return gfx::Size(width, height);
      } else {
        return gfx::Size(std::max(width, min_size.width()),
                         std::max(height, min_size.height()));
      }
    }
    if (min_size.IsEmpty() &&
        optional_properties.GetInteger("minWidth", &width) && width >= 0 &&
        optional_properties.GetInteger("minHeight", &height) && height >= 0) {
      min_size.SetSize(width, height);
    }
  }

  // No maximum size was provided, so just return the default size bounded by
  // the minimum size.
  return gfx::Size(std::max(kDefaultWidth, min_size.width()),
                   std::max(kDefaultHeight, min_size.height()));
}

ExtensionFunction::ResponseAction TabCaptureGetMediaStreamIdFunction::Run() {
  std::unique_ptr<api::tab_capture::GetMediaStreamId::Params> params =
      TabCapture::GetMediaStreamId::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* target_contents = nullptr;
  if (params->options && params->options->target_tab_id) {
    if (!ExtensionTabUtil::GetTabById(*(params->options->target_tab_id),
                                      browser_context(), true,
                                      &target_contents)) {
      return RespondNow(Error(kInvalidTabIdError));
    }
  } else {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    const bool match_incognito_profile = include_incognito_information();
    Browser* target_browser =
        GetLastActiveBrowser(profile, match_incognito_profile);
    if (!target_browser)
      return RespondNow(Error(kFindingTabError));

    target_contents = target_browser->tab_strip_model()->GetActiveWebContents();
  }
  if (!target_contents)
    return RespondNow(Error(kFindingTabError));

  const std::string& extension_id = extension()->id();

  // Make sure either we have been granted permission to capture through an
  // extension icon click or our extension is whitelisted.
  if (!extension()->permissions_data()->HasAPIPermissionForTab(
          SessionTabHelper::IdForTab(target_contents).id(),
          APIPermission::kTabCaptureForTab) &&
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWhitelistedExtensionID) != extension_id) {
    return RespondNow(Error(kGrantError));
  }

  // |consumer_contents| is the WebContents for which the stream is created.
  content::WebContents* consumer_contents = nullptr;
  std::string consumer_name;
  GURL origin;
  if (params->options && params->options->consumer_tab_id) {
    if (!ExtensionTabUtil::GetTabById(*(params->options->consumer_tab_id),
                                      browser_context(), true,
                                      &consumer_contents)) {
      return RespondNow(Error(kInvalidTabIdError));
    }

    origin = consumer_contents->GetLastCommittedURL().GetOrigin();
    if (!origin.is_valid()) {
      return RespondNow(Error(kInvalidOriginError));
    }

    if (!content::IsOriginSecure(origin)) {
      return RespondNow(Error(kTabUrlNotSecure));
    }

    consumer_name = net::GetHostAndOptionalPort(origin);
  } else {
    origin = extension()->url();
    consumer_name = extension()->name();
    consumer_contents = GetSenderWebContents();
  }
  EXTENSION_FUNCTION_VALIDATE(consumer_contents);

  DesktopMediaID source = BuildDesktopMediaID(target_contents, nullptr);
  TabCaptureRegistry* registry = TabCaptureRegistry::Get(browser_context());
  std::string device_id =
      registry->AddRequest(target_contents, extension_id, false, origin, source,
                           consumer_name, consumer_contents);
  if (device_id.empty()) {
    return RespondNow(Error(kCapturingSameTab));
  }

  return RespondNow(OneArgument(std::make_unique<base::Value>(device_id)));
}

}  // namespace extensions
