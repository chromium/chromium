// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webrtc_desktop_capture_private/webrtc_desktop_capture_private_api.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/webrtc_desktop_capture_private.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace extensions {

namespace {

const char kUrlNotSecure[] =
    "URL scheme for the specified target is not secure.";

}  // namespace

WebrtcDesktopCapturePrivateChooseDesktopMediaFunction::
    WebrtcDesktopCapturePrivateChooseDesktopMediaFunction() {
}

WebrtcDesktopCapturePrivateChooseDesktopMediaFunction::
    ~WebrtcDesktopCapturePrivateChooseDesktopMediaFunction() {
}

ExtensionFunction::ResponseAction
WebrtcDesktopCapturePrivateChooseDesktopMediaFunction::Run() {
  using Params =
      extensions::api::webrtc_desktop_capture_private::ChooseDesktopMedia
          ::Params;
  EXTENSION_FUNCTION_VALIDATE(args().size() > 0);
  const auto& request_id_value = args()[0];
  EXTENSION_FUNCTION_VALIDATE(request_id_value.is_int());
  request_id_ = request_id_value.GetInt();
  DesktopCaptureRequestsRegistry::GetInstance()->AddRequest(source_process_id(),
                                                            request_id_, this);

  mutable_args().erase(args().begin());

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(params->request.guest_process_id,
                                       params->request.guest_render_frame_id);

  if (!render_frame_host) {
    return RespondNow(Error(kTargetNotFoundError));
  }

  GURL origin =
      render_frame_host->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kAllowHttpScreenCapture) &&
      !network::IsUrlPotentiallyTrustworthy(origin)) {
    return RespondNow(Error(kUrlNotSecure));
  }
  std::u16string target_name =
      base::UTF8ToUTF16(network::IsUrlPotentiallyTrustworthy(origin)
                            ? net::GetHostAndOptionalPort(origin)
                            : origin.spec());

  using Sources = std::vector<api::desktop_capture::DesktopCaptureSourceType>;
  Sources* sources = reinterpret_cast<Sources*>(&params->sources);

  // TODO(crbug.com/40226648): Plumb systemAudio, selfBrowserSurface and
  // suppressLocalAudioPlaybackIntended here.
  return Execute(*sources, /*exclude_system_audio=*/false,
                 /*exclude_self_browser_surface=*/false,
                 /*suppress_local_audio_playback_intended=*/false,
                 render_frame_host, origin, target_name);
}

WebrtcDesktopCapturePrivateCancelChooseDesktopMediaFunction::
    WebrtcDesktopCapturePrivateCancelChooseDesktopMediaFunction() {}

WebrtcDesktopCapturePrivateCancelChooseDesktopMediaFunction::
    ~WebrtcDesktopCapturePrivateCancelChooseDesktopMediaFunction() {}

}  // namespace extensions
