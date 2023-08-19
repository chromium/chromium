// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_WEB_APP_H_
#define CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_WEB_APP_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chrome/browser/chromeos/video_conference/video_conference_ukm_helper.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace content {
class Page;
class RenderWidgetHost;
class WebContents;
}  // namespace content

namespace video_conference {

struct VideoConferencePermissions;

// This class stores information about videoconferencing web apps for
// `VideoConferenceManagerClient`s. A VC webapp is an open page that has
// captured or is capturing the microphone, camera, or screen.
//
// This class also tracks the destruction of its corresponding webcontents or a
// change to the primary page for its `VideoConferenceManagerClient`.
//
// NOTE: we treat `PrimaryPageChanged()` as a new VcWebApp and hence remove the
// old one from the client.
class VideoConferenceWebApp
    : public content::WebContentsObserver,
      public content::WebContentsUserData<VideoConferenceWebApp> {
 public:
  VideoConferenceWebApp(const VideoConferenceWebApp&) = delete;
  VideoConferenceWebApp& operator=(const VideoConferenceWebApp&) = delete;

  ~VideoConferenceWebApp() override;

  // Brings the associated tab into focus.
  void ActivateApp();

  // Returns a pair containing the (camera, microphone) permissions granted
  // status for this app.
  VideoConferencePermissions GetPermissions();

  // Returns bool indicating whether the VC webapp is an extension and is not
  // capturing camera, microphone, or screen.
  bool IsInactiveExtension();

  // content::WebcontentsObserver overrides
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

  // Set capturing status in state for the specified media device. This method
  // is also responsible for updating data needed for UKM reporting.
  void SetCapturingStatus(VideoConferenceMediaType device, bool is_capturing);

  VideoConferenceWebAppState& state() { return state_; }

 private:
  friend class WebContentsUserData<VideoConferenceWebApp>;

  // `remove_media_app_callback` is called on `WebContentsDestroyed()` or
  // `PrimaryPageChanged()`.
  VideoConferenceWebApp(
      content::WebContents* web_contents,
      base::UnguessableToken id,
      base::RepeatingCallback<void(const base::UnguessableToken&)>
          remove_media_app_callback,
      base::RepeatingCallback<
          void(crosapi::mojom::VideoConferenceClientUpdatePtr)>
          client_update_callback);

  // This callback corresponds to a method on a
  // `VideoConferenceManagerClientImpl`. It is safe to call even if the client
  // has been destroyed.
  base::RepeatingCallback<void(const base::UnguessableToken&)>
      remove_media_app_callback_;

  // Callback to send a new client update.
  base::RepeatingCallback<void(crosapi::mojom::VideoConferenceClientUpdatePtr)>
      client_update_callback_;

  VideoConferenceWebAppState state_;
  std::unique_ptr<VideoConferenceUkmHelper> vc_ukm_helper_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace video_conference

#endif  // CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_WEB_APP_H_
